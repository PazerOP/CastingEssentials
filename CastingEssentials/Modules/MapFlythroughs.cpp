#include "MapFlythroughs.h"
#include "Misc/DebugOverlay.h"
#include "Modules/CameraState.h"
#include "Modules/CameraTools.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"

#include "Misc/HLTVCameraHack.h"

#include <KeyValues.h>
#include <filesystem.h>
#include <ivrenderview.h>
#include <client/cliententitylist.h>
#include <shared/util_shared.h>
#include <client/cdll_client_int.h>
#include <engine/ivdebugoverlay.h>
#include <client/c_baseentity.h>
#include <view_shared.h>
#include <debugoverlay_shared.h>
#include <toolframework/ienginetool.h>
#include <collisionutils.h>

#include <regex>

MapFlythroughs::MapFlythroughs()
{
	ce_cameratrigger_begin = new ConCommand("ce_autocamera_trigger_begin", [](const CCommand& args) { GetModule()->BeginCameraTrigger(); }, nullptr, FCVAR_UNREGISTERED);
	ce_cameratrigger_end = new ConCommand("ce_autocamera_trigger_end", [](const CCommand& args) { GetModule()->EndCameraTrigger(); }, nullptr, FCVAR_UNREGISTERED);

	ce_autocamera_begin_storyboard = new ConCommand("ce_autocamera_begin_storyboard", [](const CCommand& args) { GetModule()->BeginStoryboard(args); }, nullptr, FCVAR_UNREGISTERED);

	ce_autocamera_mark_camera = new ConCommand("ce_autocamera_create", [](const CCommand& args) { GetModule()->MarkCamera(args); });
	ce_autocamera_goto_camera = new ConCommand("ce_autocamera_goto", [](const CCommand& args) { GetModule()->GotoCamera(args); }, nullptr, 0, &MapFlythroughs::GotoCameraCompletion);
	ce_autocamera_cycle = new ConCommand("ce_autocamera_cycle", [](const CCommand& args) { GetModule()->CycleCamera(args); }, "Switches to the next or previous autocamera. If not currently in an autocamera, switches to the nearest autocamera with a view of the current position.");

	ce_autocamera_reload_config = new ConCommand("ce_autocamera_reload_config", [](const CCommand& args) { GetModule()->LoadConfig(); });

	ce_autocamera_show_triggers = new ConVar("ce_autocamera_show_triggers", "0", FCVAR_UNREGISTERED, "Shows all triggers on the map.");
	ce_autocamera_show_cameras = new ConVar("ce_autocamera_show_cameras", "0", FCVAR_NONE, "1 = Shows all cameras on the map. 2 = Shows view frustums as well.");

	m_CreatingCameraTrigger = false;
	m_CameraTriggerStart.Init();
}

static bool GetView(Vector* pos, QAngle* ang)
{
	CViewSetup view;
	if (!Interfaces::GetClientDLL()->GetPlayerView(view))
	{
		PluginWarning("[%s] Failed to GetPlayerView!\n", __FUNCSIG__);
		return false;
	}

	if (pos)
		*pos = view.origin;

	if (ang)
		*ang = view.angles;

	return true;
}

static Vector GetCrosshairTarget()
{
	CViewSetup view;
	if (!Interfaces::GetClientDLL()->GetPlayerView(view))
		PluginWarning("[%s] Failed to GetPlayerView!\n", __FUNCSIG__);

	const Vector forward;
	AngleVectors(view.angles, const_cast<Vector*>(&forward));

	Ray_t ray;
	ray.Init(view.origin, view.origin + forward * 5000);

	const int viewEntity = Interfaces::GetHLTVCamera()->m_iTraget1;
	CTraceFilterSimple tf(Interfaces::GetClientEntityList()->GetClientEntity(viewEntity), COLLISION_GROUP_NONE);

	trace_t tr;
	Interfaces::GetEngineTrace()->TraceRay(ray, MASK_SOLID, &tf, &tr);
	return tr.endpos;
}

void MapFlythroughs::OnTick(bool ingame)
{
	if (ingame)
	{
		if (m_CreatingCameraTrigger)
			NDebugOverlay::Box(Vector(0, 0, 0), m_CameraTriggerStart, GetCrosshairTarget(), 255, 255, 128, 64, 0);

		while (m_ActiveStoryboard)
		{
			if (!m_ActiveStoryboardElement)
			{
				m_ActiveStoryboard.reset();
				break;
			}

			if (!m_ActiveStoryboardElement->m_Trigger)
			{
				ExecuteStoryboardElement(m_ActiveStoryboardElement, nullptr);
				m_ActiveStoryboardElement = m_ActiveStoryboardElement->m_Next;
			}
			else
			{
				std::vector<C_BaseEntity*> entities;
				CheckTrigger(m_ActiveStoryboardElement->m_Trigger, entities);

				if (entities.size())
				{
					ExecuteStoryboardElement(m_ActiveStoryboardElement, entities.front());
					m_ActiveStoryboardElement = m_ActiveStoryboardElement->m_Next;
				}

				break;
			}
		}

		if (ce_autocamera_show_triggers->GetBool())
			DrawTriggers();

		if (ce_autocamera_show_cameras->GetBool())
			DrawCameras();
	}
}

void MapFlythroughs::LevelInitPreEntity()
{
	LoadConfig();
}

void MapFlythroughs::LoadConfig()
{
	const char* const mapName = Interfaces::GetEngineClient()->GetLevelName();
	LoadConfig(mapName);
}

void MapFlythroughs::LoadConfig(const char* bspName)
{
	debugoverlay->ClearAllOverlays();

	m_MalformedTriggers.clear();
	m_Triggers.clear();
	m_MalformedCameras.clear();
	m_Cameras.clear();
	m_MalformedStoryboards.clear();
	m_Storyboards.clear();

	std::string mapName(bspName);
	mapName.erase(mapName.size() - 4, 4);	// remove .bsp
	mapName.erase(0, 5);					// remove /maps/

	KeyValuesAD kv("AutoCameras");
	m_ConfigFilename = strprintf("addons/castingessentials/autocameras/%s.vdf", mapName.c_str());
	if (!kv->LoadFromFile(Interfaces::GetFileSystem(), m_ConfigFilename.c_str(), nullptr, true))
	{
		PluginWarning("Unable to load autocameras from %s!\n", m_ConfigFilename.c_str());
		return;
	}

	{
		constexpr const char* mapOriginKey = "map_origin";
		const char* const mapOriginValue = kv->GetString(mapOriginKey, "0 0 0");
		if (!ParseVector(m_MapOrigin, mapOriginValue))
		{
			PluginWarning("Failed to parse \"%s\" vector from value \"%s\" in %s! Mirrored cameras will not work.\n", mapOriginKey, mapOriginValue, m_ConfigFilename.c_str());
		}
	}

	for (KeyValues* trigger = kv->GetFirstTrueSubKey(); trigger; trigger = trigger->GetNextTrueSubKey())
	{
		if (stricmp("trigger", trigger->GetName()))
			continue;

		LoadTrigger(trigger, m_ConfigFilename.c_str());
	}

	for (KeyValues* camera = kv->GetFirstTrueSubKey(); camera; camera = camera->GetNextTrueSubKey())
	{
		if (stricmp("camera", camera->GetName()))
			continue;

		LoadCamera(camera, m_ConfigFilename.c_str());
	}

	for (KeyValues* storyboard = kv->GetFirstTrueSubKey(); storyboard; storyboard = storyboard->GetNextTrueSubKey())
	{
		if (stricmp("storyboard", storyboard->GetName()))
			continue;

		LoadStoryboard(storyboard, m_ConfigFilename.c_str());
	}

	SetupMirroredCameras();

	PluginMsg("Loaded autocameras from %s.\n", m_ConfigFilename.c_str());
}

static void FixupBounds(Vector& mins, Vector& maxs)
{
	if (mins.x > maxs.x)
		std::swap(mins.x, maxs.x);
	if (mins.y > maxs.y)
		std::swap(mins.y, maxs.y);
	if (mins.z > maxs.z)
		std::swap(mins.z, maxs.z);
}

bool MapFlythroughs::LoadTrigger(KeyValues* const trigger, const char* const filename)
{
	std::shared_ptr<Trigger> newTrigger(new Trigger);
	const char* const name = trigger->GetString("name", nullptr);
	{
		if (!name)
		{
			Warning("Missing required value \"name\" on a Trigger in \"%s\"!\n", filename);
			return false;
		}
		newTrigger->m_Name = name;
	}

	// mins
	{
		const char* const mins = trigger->GetString("mins", nullptr);
		if (!mins)
		{
			Warning("Missing required value \"mins\" in \"%s\" on trigger named \"%s\"!\n", filename, name);
			m_MalformedTriggers.push_back(name);
			return false;
		}
		if (!ParseVector(newTrigger->m_Mins, mins))
		{
			Warning("Failed to parse mins \"%s\" in \"%s\" on trigger named \"%s\"!\n", mins, filename, name);
			m_MalformedTriggers.push_back(name);
			return false;
		}
	}

	// maxs
	{
		const char* const maxs = trigger->GetString("maxs", nullptr);
		if (!maxs)
		{
			Warning("Missing required value \"maxs\" in \"%s\" on trigger named \"%s\"!\n", filename, name);
			m_MalformedTriggers.push_back(name);
			return false;
		}
		if (!ParseVector(newTrigger->m_Maxs, maxs))
		{
			Warning("Failed to parse maxs \"%s\" in \"%s\" on trigger named \"%s\"!\n", maxs, filename, name);
			m_MalformedTriggers.push_back(name);
			return false;
		}
	}

	FixupBounds(newTrigger->m_Mins, newTrigger->m_Maxs);

	m_Triggers.push_back(newTrigger);
	return true;
}

bool MapFlythroughs::LoadCamera(KeyValues* const camera, const char* const filename)
{
	std::shared_ptr<Camera> newCamera(new Camera);
	const char* const name = camera->GetString("name", nullptr);
	{
		if (!name)
		{
			Warning("Missing required value \"name\" on a Camera in \"%s\"!\n", filename);
			return false;
		}
		newCamera->m_Name = name;
	}

	const char* const mirror = camera->GetString("mirror", nullptr);
	if (mirror)
	{
		newCamera->m_MirroredCameraName = mirror;
	}
	else
	{
		// pos
		{
			const char* const pos = camera->GetString("pos", nullptr);
			if (!pos)
			{
				Warning("Missing required value \"pos\" in \"%s\" on camera named \"%s\"!\n", filename, name);
				m_MalformedCameras.push_back(name);
				return false;
			}
			if (!ParseVector(newCamera->m_Pos, pos))
			{
				Warning("Failed to parse pos \"%s\" in \"%s\" on camera named \"%s\"!\n", pos, filename, name);
				m_MalformedCameras.push_back(name);
				return false;
			}
		}

		// ang
		{
			const char* const ang = camera->GetString("ang", nullptr);
			if (!ang)
			{
				Warning("Missing required value \"ang\" in \"%s\" on camera named \"%s\"!\n", filename, name);
				m_MalformedCameras.push_back(name);
				return false;
			}
			if (!ParseAngle(newCamera->m_DefaultAngle, ang))
			{
				Warning("Failed to parrse ang \"%s\" in \"%s\" on camera named \"%s\"!\n", ang, filename, name);
				m_MalformedCameras.push_back(name);
				return false;
			}
		}
	}

	m_Cameras.push_back(newCamera);
	return true;
}

bool MapFlythroughs::LoadStoryboard(KeyValues* const storyboard, const char* const filename)
{
	std::shared_ptr<Storyboard> newStoryboard(new Storyboard);
	const char* const name = storyboard->GetString("name", nullptr);
	{
		if (!name)
		{
			Warning("Missing requied value \"name\" on a Storyboard in \"%s\"!\n", filename);
			return false;
		}
		newStoryboard->m_Name = name;
	}

	std::shared_ptr<StoryboardElement> lastElement;
	for (KeyValues* element = storyboard->GetFirstTrueSubKey(); element; element = element->GetNextTrueSubKey())
	{
		std::shared_ptr<StoryboardElement> newElement;
		if (!stricmp("shot", element->GetName()))
		{
			if (!LoadShot(newElement, element, name, filename))
			{
				m_MalformedStoryboards.push_back(name);
				return false;
			}
		}
		else if (!stricmp("action", element->GetName()))
		{
			if (!LoadAction(newElement, element, name, filename))
			{
				m_MalformedStoryboards.push_back(name);
				return false;
			}
		}
		
		if (!lastElement)
		{
			newStoryboard->m_FirstElement = newElement;
			lastElement = newElement;
		}
		else
		{
			lastElement->m_Next = newElement;
			lastElement = newElement;
		}
	}

	m_Storyboards.push_back(newStoryboard);
	return true;
}

bool MapFlythroughs::LoadShot(std::shared_ptr<StoryboardElement>& shotOut, KeyValues* const shotKV, const char* const storyboardName, const char* const filename)
{
	std::shared_ptr<Shot> newShot(new Shot);

	// camera
	{
		const char* const cameraName = shotKV->GetString("camera", nullptr);
		if (!cameraName)
		{
			Warning("Missing required value \"camera\" on a shot in a storyboard named \"%s\" in \"%s\"!\n", storyboardName, filename);
			return false;
		}

		newShot->m_Camera = FindCamera(cameraName);
		if (!newShot->m_Camera)
		{
			Warning("Unable to find camera name \"%s\" for a shot in a storyboard named \"%s\" in \"%s\"!\n", cameraName, storyboardName, filename);
			return false;
		}
	}

	// trigger
	{
		const char* const triggerName = shotKV->GetString("trigger", nullptr);
		if (triggerName)
		{
			newShot->m_Trigger = FindTrigger(triggerName);
			if (!newShot->m_Trigger)
			{
				Warning("Unable to find trigger name \"%s\" for a shot in a storyboard named \"%s\" in \"%s\"!\n", triggerName, storyboardName, filename);
				return false;
			}
		}
	}

	// target
	{
		const char* const targetName = shotKV->GetString("target", nullptr);
		if (targetName)
		{
			Assert(newShot->m_Target == Target::Invalid);
			if (!stricmp(targetName, "none"))
				newShot->m_Target = Target::None;
			else if (!stricmp(targetName, "first_player"))
				newShot->m_Target = Target::FirstPlayer;

			if (newShot->m_Target == Target::Invalid)
			{
				Warning("Unknown target name \"%s\" for a shot in a storyboard named \"%s\" in \"%s\"!\n", targetName, storyboardName, filename);
				return false;
			}
		}
		else
			newShot->m_Target = Target::None;
	}

	shotOut = newShot;
	return true;
}

bool MapFlythroughs::LoadAction(std::shared_ptr<StoryboardElement>& actionOut, KeyValues* const actionKV, const char* const storyboardName, const char* const filename)
{
	std::shared_ptr<Action> newAction(new Action);

	// action
	{
		const char* const actionName = actionKV->GetString("action", nullptr);
		if (!actionName)
		{
			Warning("Missing required value \"camera\" on an action in a storyboard named \"%s\" in \"%s\"!\n", storyboardName, filename);
			return false;
		}

		Action::Effect parsedAction = Action::Invalid;
		if (!stricmp(actionName, "lerp_to_player"))
			parsedAction = Action::LerpToPlayer;

		if (parsedAction == Action::Invalid)
		{
			Warning("Unknown action type \"%s\" for an action in a storyboard named \"%s\" in \"%s\"!\n", actionName, storyboardName, filename);
			return false;
		}

		newAction->m_Action = parsedAction;
	}

	// trigger
	{
		const char* const triggerName = actionKV->GetString("trigger", nullptr);
		if (triggerName)
		{
			newAction->m_Trigger = FindTrigger(triggerName);
			if (!newAction->m_Trigger)
			{
				Warning("Unable to find trigger name \"%s\" for an action in a storyboard named \"%s\" in \"%s\"!\n", triggerName, storyboardName, filename);
				return false;
			}
		}
	}

	// target
	{
		const char* const targetName = actionKV->GetString("target", nullptr);
		if (targetName)
		{
			Target parsedTarget = Target::Invalid;
			if (!stricmp(targetName, "none"))
				parsedTarget = Target::None;
			else if (!stricmp(targetName, "first_player"))
				parsedTarget = Target::FirstPlayer;

			if (parsedTarget == Target::Invalid)
			{
				Warning("Unknown target name \"%s\" for an action in a storyboard named \"%s\" in \"%s\"!\n", targetName, storyboardName, filename);
				return false;
			}

			newAction->m_Target = parsedTarget;
		}
		else
			newAction->m_Target = Target::None;
	}

	actionOut = newAction;
	return true;
}

void MapFlythroughs::CheckTrigger(const std::shared_ptr<Trigger>& trigger, std::vector<C_BaseEntity*>& entities)
{
	Assert(trigger);
	if (!trigger)
		return;

	for (int i = 1; i <= Interfaces::GetEngineTool()->GetMaxClients(); i++)
	{
		IClientEntity* const clientEnt = Interfaces::GetClientEntityList()->GetClientEntity(i);
		if (!clientEnt)
			continue;

		IClientRenderable* const renderable = clientEnt->GetClientRenderable();
		if (!renderable)
			continue;

		Vector mins, maxs;
		renderable->GetRenderBoundsWorldspace(mins, maxs);

		if (IsBoxIntersectingBox(trigger->m_Mins, trigger->m_Maxs, mins, maxs))
		{
			C_BaseEntity* const baseEntity = clientEnt->GetBaseEntity();
			if (!baseEntity)
				continue;

			entities.push_back(baseEntity);
		}
	}
}

void MapFlythroughs::ExecuteStoryboardElement(const std::shared_ptr<StoryboardElement>& element, C_BaseEntity* const triggerer)
{
	// Shot
	{
		auto cast = std::dynamic_pointer_cast<Shot>(element);
		if (cast)
		{
			ExecuteShot(cast, triggerer);
			return;
		}
	}

	// Action
	{
		auto cast = std::dynamic_pointer_cast<Action>(element);
		if (cast)
		{
			ExecuteAction(cast, triggerer);
			return;
		}
	}

	Warning("[%s] Failed to cast StoryboardElement for execution!\n", __FUNCSIG__);
	return;
}

void MapFlythroughs::ExecuteShot(const std::shared_ptr<Shot>& shot, C_BaseEntity* const triggerer)
{
	const auto& camera = shot->m_Camera;

	CameraTools::GetModule()->SpecPosition(camera->m_Pos, camera->m_DefaultAngle);

	if (triggerer && shot->m_Target == Target::FirstPlayer)
	{
		auto const hltvcamera = Interfaces::GetHLTVCamera();
		//Interfaces::GetHLTVCamera()->
		hltvcamera->m_flLastAngleUpdateTime = -1;
		hltvcamera->m_iTraget1 = triggerer->entindex();
		hltvcamera->m_flInertia = 30;
	}
}

void MapFlythroughs::ExecuteAction(const std::shared_ptr<Action>& action, C_BaseEntity* const triggerer)
{
	//Assert(!"Not implemented");
}

void MapFlythroughs::BeginCameraTrigger()
{
	m_CameraTriggerStart = GetCrosshairTarget();
	m_CreatingCameraTrigger = true;
}

void MapFlythroughs::EndCameraTrigger()
{
	if (!m_CreatingCameraTrigger)
	{
		PluginWarning("Must use %s before using %s!\n", ce_cameratrigger_begin->GetName(), ce_cameratrigger_end->GetName());
		return;
	}

	const Vector cameraTriggerEnd = GetCrosshairTarget();
	NDebugOverlay::Box(Vector(0, 0, 0), m_CameraTriggerStart, cameraTriggerEnd, 128, 255, 128, 64, 5);

	ConColorMsg(Color(128, 255, 128, 255), "Trigger { mins \"%1.1f %1.1f %1.1f\" maxs \"%1.1f %1.1f %1.1f\" }\n",
		m_CameraTriggerStart.x, m_CameraTriggerStart.y, m_CameraTriggerStart.z,
		cameraTriggerEnd.x, cameraTriggerEnd.y, cameraTriggerEnd.z);

	m_CreatingCameraTrigger = false;
	m_CameraTriggerStart.Init();
}

void MapFlythroughs::BeginStoryboard(const CCommand& args)
{
	const auto& storyboard = m_Storyboards.front();
	m_ActiveStoryboard = storyboard;
	m_ActiveStoryboardElement = m_ActiveStoryboard->m_FirstElement;
}

void MapFlythroughs::MarkCamera(const CCommand& args)
{
	if (args.ArgC() != 2)
	{
		Warning("Usage: %s <name>\n", ce_autocamera_mark_camera->GetName());
		return;
	}

	const Vector pos;
	const QAngle ang;
	if (!GetView(const_cast<Vector*>(&pos), const_cast<QAngle*>(&ang)))
		return;

	std::string angString;
	if (fabs(ang.z) < 0.25)
		angString = strprintf("\"%1.1f %1.1f\"", ang.x, ang.y);
	else
		angString = strprintf("\"%1.1f %1.1f %1.1f\"", ang.x, ang.y, ang.z);

	ConColorMsg(Color(128, 255, 128, 255), "Camera { name \"%s\" pos \"%1.1f %1.1f %1.1f\" ang %s }\n",
		args.Arg(1), pos.x, pos.y, pos.z, angString.c_str());
}

constexpr vec_t BitsToFloat_cx(unsigned long i)
{
	return *reinterpret_cast<vec_t*>(&i);
}

void MapFlythroughs::CycleCamera(const CCommand& args)
{
	if (m_Cameras.size() < 1)
	{
		Warning("%s: No defined cameras for this map!\n", ce_autocamera_cycle->GetName());
		return;
	}

	if (args.ArgC() != 2)
		goto Usage;

	enum Direction
	{
		NEXT,
		PREV,
	} dir;

	// Get direction from arguments
	if (!strnicmp(args.Arg(1), "next", 4))
		dir = NEXT;
	else if (!strnicmp(args.Arg(1), "prev", 4))
		dir = PREV;
	else
		goto Usage;

	// Get current camera origin
	const Vector camOrigin;
	{
		QAngle dummy;
		Assert(CameraState::GetModule());
		if (CameraState::GetModule())
			CameraState::GetModule()->GetThisFramePluginView(const_cast<Vector&>(camOrigin), dummy);
	}

	// Find an existing autocamera with the current origin, then go backwards/forwards (based on file order)
	{
		bool matchedPos = false;
		std::shared_ptr<Camera> prevCamera;
		for (const auto& camera : m_Cameras)
		{
			if (AlmostEqual(camera->m_Pos, camOrigin))
			{
				matchedPos = true;

				if (dir == PREV)
				{
					if (prevCamera)
						return GotoCamera(prevCamera);
					else
						break;
				}
				else if (dir == NEXT)
				{
					prevCamera = camera;
					continue;
				}
			}

			if (dir == PREV)
				prevCamera = camera;
			else if (dir == NEXT && prevCamera)
				return GotoCamera(camera);
		}

		if (matchedPos)
		{
			// If we made it here, there's an edge case where we were asked to go backward from
			// the start of the list, or forward from the end of the list.
			if (dir == PREV && !prevCamera)
				return GotoCamera(m_Cameras.back());
			else if (dir == NEXT && prevCamera == m_Cameras.back())
				return GotoCamera(m_Cameras.front());
			else
				Error("[%s:%i] wat\n", __FUNCSIG__, __LINE__);
		}
	}

	// If we're here, our camera position didn't match any autocamera definitions. Find autocameras
	// with line of sight to the current position.
	{
		const Vector boxMins(camOrigin - Vector(0.5));
		const Vector boxMaxs(camOrigin + Vector(0.5));

		std::shared_ptr<Camera> idealCamera;
		std::shared_ptr<Camera> idealCameraLOS;
		for (const auto& camera : m_Cameras)
		{
			const auto idealCameraDist = idealCamera ? idealCamera->m_Pos.DistTo(camOrigin) : FLOAT32_NAN;
			const auto idealCameraLOSDist = idealCameraLOS ? idealCameraLOS->m_Pos.DistTo(camOrigin) : FLOAT32_NAN;
			const auto thisDist = camera->m_Pos.DistTo(camOrigin);

			if (idealCameraLOS && (dir == NEXT) ? (idealCameraLOSDist < thisDist) : (idealCameraLOSDist > thisDist))
				continue;	// Already have a better camera with LOS

			constexpr float test = 100000;
			Frustum_t frustum;
			GeneratePerspectiveFrustum(camera->m_Pos, camera->m_DefaultAngle, 1, 100000, 90, (16.0 / 9.0), frustum);
			
			if (!R_CullBox(boxMins, boxMaxs, frustum))
				continue;	// Not in frustum

			CTraceFilterNoNPCsOrPlayer noPlayers(nullptr, COLLISION_GROUP_NONE);
			trace_t tr;
			UTIL_TraceLine(camera->m_Pos, camOrigin, MASK_OPAQUE, &noPlayers, &tr);

			if (tr.fraction >= 1)
			{
				if (!idealCameraLOS || (dir == NEXT) ? (idealCameraLOSDist > thisDist) : (idealCameraLOSDist < thisDist))
					idealCameraLOS = idealCamera = camera;
			}
			else if (!idealCamera || (dir == NEXT) ? (idealCameraDist > thisDist) : (idealCameraDist < thisDist))
				idealCamera = camera;
		}

		if (idealCameraLOS)
			return GotoCamera(idealCameraLOS);
		else if (idealCamera)
			return GotoCamera(idealCamera);
	}

	// If we're here, nobody was looking at this point, regardless of LOS :(
	// Find the nearest/furthest camera.
	{
		std::shared_ptr<Camera> idealCamera;
		for (const auto& camera : m_Cameras)
		{
			const auto idealCameraDist = idealCamera ? idealCamera->m_Pos.DistTo(camOrigin) : FLOAT32_NAN;
			const auto thisDist = camera->m_Pos.DistTo(camOrigin);

			if (idealCamera && (dir == NEXT) ? (idealCameraDist < thisDist) : (idealCameraDist > thisDist))
				continue;	// Already have a better camera

			idealCamera = camera;
		}

		if (idealCamera)
			return GotoCamera(idealCamera);
	}

	Warning("%s: Unable to navigate to a camera for some reason.\n", ce_autocamera_cycle->GetName());
	return;

Usage:
	Warning("%s: Usage: %s <prev/next>\n", ce_autocamera_cycle->GetName(), ce_autocamera_cycle->GetName());
}

void MapFlythroughs::GotoCamera(const std::shared_ptr<Camera>& camera)
{
	CameraTools* const ctools = CameraTools::GetModule();
	if (!ctools)
	{
		PluginWarning("%s: Camera Tools module unavailable!\n", __FUNCSIG__);
		return;
	}

	ctools->SpecPosition(camera->m_Pos, camera->m_DefaultAngle);
}

void MapFlythroughs::GotoCamera(const CCommand& args)
{
	if (args.ArgC() != 2)
	{
		Warning("Usage: %s <camera name>\n", ce_autocamera_goto_camera->GetName());
		return;
	}

	const char* const name = args.Arg(1);
	auto const cam = FindCamera(name);
	if (!cam)
	{
		if (FindContainedString(m_MalformedCameras, name))
			Warning("%s: Unable to goto camera \"%s\" because its definition in \"%s\" is malformed.\n", ce_autocamera_goto_camera->GetName(), name, m_ConfigFilename.c_str());
		else
			Warning("%s: Unable to find camera with name \"%s\"!\n", ce_autocamera_goto_camera->GetName(), name);

		return;
	}

	GotoCamera(cam);
}

int MapFlythroughs::GotoCameraCompletion(const char* const partial, char commands[COMMAND_COMPLETION_MAXITEMS][COMMAND_COMPLETION_ITEM_LENGTH])
{
	std::cmatch match;
	const std::string regexString = strprintf("\\s*%s\\s+\"?(.*?)\"?\\s*$", GetModule()->ce_autocamera_goto_camera->GetName());

	// Interesting... this only works if you pass a const char* to std::regex's constructor, and not if you pass an std::string.
	if (!std::regex_search(partial, match, std::regex(regexString.c_str(), std::regex_constants::icase)))
		return 0;
	
	MapFlythroughs* const mod = GetModule();

	auto cameras = mod->GetAlphabeticalCameras();
	if (match[1].length() >= 1)
	{
		const char* const matchStr = match[1].first;
		for (size_t i = 0; i < cameras.size(); i++)
		{
			if (!stristr(cameras[i]->m_Name.c_str(), matchStr))
				cameras.erase(cameras.begin() + i--);
		}
	}

	size_t i;
	for (i = 0; i < COMMAND_COMPLETION_MAXITEMS && i < cameras.size(); i++)
	{
		strcpy_s(commands[i], COMMAND_COMPLETION_ITEM_LENGTH, mod->ce_autocamera_goto_camera->GetName());
		const auto& nameStr = cameras[i]->m_Name;
		strcat_s(commands[i], COMMAND_COMPLETION_ITEM_LENGTH, (std::string(" ") + nameStr).c_str());
	}
	return i;
}

void MapFlythroughs::DrawTriggers()
{
	for (const auto& trigger : m_Triggers)
	{
		NDebugOverlay::Box(Vector(0, 0, 0), trigger->m_Mins, trigger->m_Maxs, 128, 255, 128, 64, 0);

		const Vector difference = VectorLerp(trigger->m_Mins, trigger->m_Maxs, 0.5);
		NDebugOverlay::Text(VectorLerp(trigger->m_Mins, trigger->m_Maxs, 0.5), trigger->m_Name.c_str(), false, 0);
	}
}

constexpr float test(float input)
{
	return input * 5020;
}

void MapFlythroughs::DrawCameras()
{
	for (auto camera : m_Cameras)
	{
		Vector forward, up, right;
		AngleVectors(camera->m_DefaultAngle, &forward, &right, &up);

		// Draw cameras
		if (ce_autocamera_show_cameras->GetInt() > 0)
		{
			NDebugOverlay::BoxAngles(camera->m_Pos, Vector(-16, -16, -16), Vector(16, 16, 16), camera->m_DefaultAngle, 128, 128, 255, 64, 0);

			const Vector endPos = camera->m_Pos + (forward * 128);
			NDebugOverlay::Line(camera->m_Pos, endPos, 128, 128, 255, true, 0);
			NDebugOverlay::Cross3DOriented(endPos, camera->m_DefaultAngle, 16, 128, 128, 255, true, 0);

			NDebugOverlay::Text(camera->m_Pos, camera->m_Name.c_str(), false, 0);
		}

		// From http://stackoverflow.com/a/27872276
		// Draw camera frustums
		if (ce_autocamera_show_cameras->GetInt() > 1)
		{
			constexpr float farDistance = 512;
			constexpr float viewRatio = 16.0 / 9.0;
			constexpr float halfFOV = Deg2Rad(75 / 2.0);

			// Compute the center points of the near and far planes
			const Vector farCenter = camera->m_Pos + forward * farDistance;
			
			// Compute the widths and heights of the near and far planes:
			const float farHeight = 2 * (std::tanf(halfFOV) * farDistance);
			const float farWidth = farHeight * viewRatio;

			// Compute the corner points from the near and far planes:
			const Vector farTopLeft = farCenter + up * (farHeight * 0.5) - right * (farWidth * 0.5);
			const Vector farTopRight = farCenter + up * (farHeight * 0.5) + right * (farWidth * 0.5);
			const Vector farBottomLeft = farCenter - up * (farHeight * 0.5) - right * (farWidth * 0.5);
			const Vector farBottomRight = farCenter - up * (farHeight * 0.5) + right * (farWidth * 0.5);

			// Draw camera frustums
			NDebugOverlay::TriangleIgnoreZ(camera->m_Pos, farTopRight, farTopLeft, 200, 200, 200, 32, false, 0);
			NDebugOverlay::TriangleIgnoreZ(camera->m_Pos, farBottomRight, farTopRight, 200, 200, 200, 32, false, 0);
			NDebugOverlay::TriangleIgnoreZ(camera->m_Pos, farBottomLeft, farBottomRight, 200, 200, 200, 32, false, 0);
			NDebugOverlay::TriangleIgnoreZ(camera->m_Pos, farTopLeft, farBottomLeft, 200, 200, 200, 32, false, 0);

			// Triangle outlines
			NDebugOverlay::Line(camera->m_Pos, farTopRight, 200, 200, 200, false, 0);
			NDebugOverlay::Line(camera->m_Pos, farTopLeft, 200, 200, 200, false, 0);
			NDebugOverlay::Line(camera->m_Pos, farBottomLeft, 200, 200, 200, false, 0);
			NDebugOverlay::Line(camera->m_Pos, farBottomRight, 200, 200, 200, false, 0);

			// Far plane outline
			NDebugOverlay::Line(farTopLeft, farTopRight, 200, 200, 200, false, 0);
			NDebugOverlay::Line(farTopRight, farBottomRight, 200, 200, 200, false, 0);
			NDebugOverlay::Line(farBottomRight, farBottomLeft, 200, 200, 200, false, 0);
			NDebugOverlay::Line(farBottomLeft, farTopLeft, 200, 200, 200, false, 0);
		}
	}
}

std::shared_ptr<MapFlythroughs::Trigger> MapFlythroughs::FindTrigger(const char* const triggerName)
{
	for (auto trigger : m_Triggers)
	{
		if (!stricmp(trigger->m_Name.c_str(), triggerName))
			return trigger;
	}

	return nullptr;
}

std::shared_ptr<MapFlythroughs::Camera> MapFlythroughs::FindCamera(const char* const cameraName)
{
	for (auto camera : m_Cameras)
	{
		if (!stricmp(camera->m_Name.c_str(), cameraName))
			return camera;
	}

	return nullptr;
}

std::vector<std::shared_ptr<const MapFlythroughs::Camera>> MapFlythroughs::GetAlphabeticalCameras() const
{
	std::vector<std::shared_ptr<const Camera>> retVal;
	for (auto cam : m_Cameras)
		retVal.push_back(cam);

	for (size_t a = 0; a < retVal.size(); a++)
	{
		for (size_t b = a + 1; b < retVal.size(); b++)
		{
			if (stricmp(retVal[a]->m_Name.c_str(), retVal[b]->m_Name.c_str()) < 0)
				std::swap(retVal.begin() + a, retVal.begin() + b);
		}
	}

	return retVal;
}

void MapFlythroughs::SetupMirroredCameras()
{
	for (auto camera : m_Cameras)
	{
		if (camera->m_MirroredCameraName.empty())
			continue;

		const auto camToMirror = FindCamera(camera->m_MirroredCameraName.c_str());
		if (!camToMirror)
		{
			if (FindContainedString(m_MalformedCameras, camera->m_MirroredCameraName.c_str()))
				PluginWarning("Unable to mirror camera \"%s\" because its base \"%s\" is malformed in \"%s\".\n", camera->m_Name.c_str(), camera->m_MirroredCameraName.c_str(), m_ConfigFilename.c_str());
			else
				PluginWarning("Unable to mirror camera \"%s\" because the base camera definition \"%s\" cannot be found in \"%s\".\n", camera->m_Name.c_str(), camera->m_MirroredCameraName.c_str(), m_ConfigFilename.c_str());

			continue;
		}

		const Vector relativeBasePos = camToMirror->m_Pos - m_MapOrigin;
		camera->m_Pos = Vector(-relativeBasePos.x, -relativeBasePos.y, relativeBasePos.z) + m_MapOrigin;
		camera->m_DefaultAngle = QAngle(
			AngleNormalize(camToMirror->m_DefaultAngle.x),
			AngleNormalize(camToMirror->m_DefaultAngle.y + 180),
			AngleNormalize(camToMirror->m_DefaultAngle.z));
	}
}

bool MapFlythroughs::FindContainedString(const std::vector<std::string>& vec, const char* str)
{
	for (const std::string& x : vec)
	{
		if (!stricmp(x.c_str(), str))
			return true;
	}

	return false;
}