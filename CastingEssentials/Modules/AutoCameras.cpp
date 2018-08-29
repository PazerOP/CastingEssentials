#include "AutoCameras.h"
#include "Misc/DebugOverlay.h"
#include "Modules/CameraState.h"
#include "Modules/CameraTools.h"
#include "Modules/FOVOverride.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"

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
#include <vprof.h>

#include <cstring>
#include <iomanip>
#include <regex>
#include <sstream>

MODULE_REGISTER(AutoCameras);

#undef min
#undef max

#undef MAX_FOV

static const auto MIN_FOV = std::nextafter(-180.0f, 0.0f);
static const auto MAX_FOV = std::nextafter(180.0f, 0.0f);

AutoCameras::AutoCameras() :
	ce_cameratrigger_begin("ce_autocamera_trigger_begin", [](const CCommand& args) { GetModule()->BeginCameraTrigger(); }, nullptr, FCVAR_UNREGISTERED),
	ce_cameratrigger_end("ce_autocamera_trigger_end", [](const CCommand& args) { GetModule()->EndCameraTrigger(); }, nullptr, FCVAR_UNREGISTERED),

	ce_autocamera_begin_storyboard("ce_autocamera_begin_storyboard", [](const CCommand& args) { GetModule()->BeginStoryboard(args); }, nullptr, FCVAR_UNREGISTERED),

	ce_autocamera_create("ce_autocamera_create", [](const CCommand& args) { GetModule()->MarkCamera(args); }),
	ce_autocamera_goto("ce_autocamera_goto", [](const CCommand& args) { GetModule()->GotoCamera(args); }, nullptr, 0, &AutoCameras::GotoCameraCompletion),
	ce_autocamera_goto_mode("ce_autocamera_goto_mode", "3", FCVAR_NONE, "spec_mode to use for ce_autocamera_goto and ce_autocamera_cycle.", true, 0, true, NUM_OBSERVER_MODES - 1),

	ce_autocamera_cycle_debug("ce_autocamera_cycle_debug", "0"),
	ce_autocamera_cycle("ce_autocamera_cycle", [](const CCommand& args) { GetModule()->CycleCamera(args); },
		"\nUsage: ce_autocamera_cycle <next/prev> [group]. Cycles forwards or backwards logically through the available autocameras. The behavior is as follows:\n"
		"\tIf already spectating from the position of an autocamera, progress forwards/backwards through the autocameras based on file order.\n"
		"\tIf our current position doesn't match any autocamera definintions, try to find the nearest/furthest autocamera that has line of sight (LOS) to our current position.\n"
		"\tIf no autocameras have LOS to our current position, find the nearest/furthest autocamera with our current position in its view frustum.\n"
		"\tIf our position is not in any autocamera's view frustum, find the nearest/furthest autocamera to our current position."),

	ce_autocamera_spec_player("ce_autocamera_spec_player", [](const CCommand& args) { GetModule()->SpecPlayer(args); },
		"Switches to an autocamera with the best view of the current player. Usage: ce_autocamera_spec_player [group]."),
	ce_autocamera_spec_player_fallback("ce_autocamera_spec_player_fallback", "firstperson", FCVAR_NONE,
		"Behavior to fall back to when using ce_autocamera_spec_player and no autocameras make it through the filters. Valid values: firstperson/thirdperson/none."),
	ce_autocamera_spec_player_fov("ce_autocamera_spec_player_fov", "75", FCVAR_NONE,
		"FOV to use when checking if a player is in view of an autocamera. -1 to disable check."),
	ce_autocamera_spec_player_dist("ce_autocamera_spec_player_dist", "2000", FCVAR_NONE, "Maximum distance a player can be from an autocamera."),
	ce_autocamera_spec_player_los("ce_autocamera_spec_player_los", "10", FCVAR_NONE,
		"Hammer unit size of the 3x3x3 cube of points use for LOS check. -1 to disable."),
	ce_autocamera_spec_player_debug("ce_autocamera_spec_player_debug", "0"),

	ce_autocamera_reload_config("ce_autocamera_reload_config", []() { GetModule()->LoadConfig(); },
		"Reloads the autocamera definitions file (located in /tf/addons/castingessentials/autocameras/<mapname>.vdf)"),

	ce_autocamera_show_triggers("ce_autocamera_show_triggers", "0", FCVAR_UNREGISTERED, "Shows all triggers on the map."),
	ce_autocamera_show_cameras("ce_autocamera_show_cameras", "0", FCVAR_NONE,
		"\n\t1 = Shows all cameras on the map.\n"
		"\t2 = Shows view frustums as well.")
{
	m_CreatingCameraTrigger = false;
	m_CameraTriggerStart.Init();
}

static bool GetView(Vector* pos, QAngle* ang, float* fov)
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

	if (fov)
		*fov = UnscaleFOVByAspectRatio(view.fov, view.m_flAspectRatio);

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

void AutoCameras::OnTick(bool ingame)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (ingame)
	{
		if (m_CreatingCameraTrigger)
			NDebugOverlay::Box(Vector(0, 0, 0), m_CameraTriggerStart, GetCrosshairTarget(), 255, 255, 128, 64, 0);

		while (m_ActiveStoryboard)
		{
			if (m_ActiveStoryboardElement)
			{
				m_ActiveStoryboard = nullptr;
				break;
			}

			if (!m_ActiveStoryboardElement->m_Trigger)
			{
				ExecuteStoryboardElement(*m_ActiveStoryboardElement, nullptr);
				m_ActiveStoryboardElement = m_ActiveStoryboardElement->m_Next.get();
			}
			else
			{
				std::vector<C_BaseEntity*> entities;
				CheckTrigger(*m_ActiveStoryboardElement->m_Trigger, entities);

				if (entities.size())
				{
					ExecuteStoryboardElement(*m_ActiveStoryboardElement, entities.front());
					m_ActiveStoryboardElement = m_ActiveStoryboardElement->m_Next.get();
				}

				break;
			}
		}

		if (ce_autocamera_show_triggers.GetBool())
			DrawTriggers();

		if (ce_autocamera_show_cameras.GetBool())
			DrawCameras();
	}
}

void AutoCameras::LevelInit()
{
	LoadConfig();
}

void AutoCameras::LoadConfig()
{
	const char* const mapName = Interfaces::GetEngineClient()->GetLevelName();
	LoadConfig(mapName);
}

void AutoCameras::LoadConfig(const char* bspName)
{
	debugoverlay->ClearAllOverlays();

	m_MalformedTriggers.clear();
	m_Triggers.clear();
	m_MalformedCameras.clear();
	m_Cameras.clear();
	m_CameraGroups.clear();
	m_MalformedStoryboards.clear();
	m_Storyboards.clear();

	m_LastActiveCamera = nullptr;
	m_ActiveStoryboard = nullptr;
	m_ActiveStoryboardElement = nullptr;

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

	if (auto cameraGroups = kv->FindKey("CameraGroups"))
	{
		for (KeyValues* group = cameraGroups->GetFirstTrueSubKey(); group; group = group->GetNextTrueSubKey())
			LoadCameraGroup(group);
	}

	CreateDefaultCameraGroup();

	for (KeyValues* storyboard = kv->GetFirstTrueSubKey(); storyboard; storyboard = storyboard->GetNextTrueSubKey())
	{
		if (stricmp("storyboard", storyboard->GetName()))
			continue;

		LoadStoryboard(storyboard, m_ConfigFilename.c_str());
	}

	SetupMirroredCameras();

	PluginMsg("Loaded autocameras from %s.\n", m_ConfigFilename.c_str());
}

bool AutoCameras::LoadCameraGroup(KeyValues* group)
{
	auto newGroup = std::make_unique<CameraGroup>();
	newGroup->m_Name = group->GetName();
	newGroup->m_Name.shrink_to_fit();

	for (KeyValues* camNameValue = group->GetFirstValue(); camNameValue; camNameValue = camNameValue->GetNextValue())
	{
		auto camName = camNameValue->GetName();
		auto found = FindCamera(camName);
		if (!found)
		{
			PluginWarning("CameraGroup \"%s\" in \"%s\" referenced unknown camera \"%s\"", newGroup->m_Name.c_str(), m_ConfigFilename.c_str(), camName);
			continue;
		}

		newGroup->m_Cameras.push_back(found);
	}

	newGroup->m_Cameras.shrink_to_fit();
	m_CameraGroups.push_back(std::move(newGroup));
	return true;
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

bool AutoCameras::LoadTrigger(KeyValues* const trigger, const char* const filename)
{
	auto newTrigger = std::make_unique<Trigger>();
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

	m_Triggers.push_back(std::move(newTrigger));
	return true;
}

bool AutoCameras::LoadCamera(KeyValues* const camera, const char* const filename)
{
	auto newCamera = std::make_unique<Camera>();
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

		// fov
		if (const char* const fov = camera->GetString("fov", nullptr); fov)
		{
			if (!TryParseFloat(fov, newCamera->m_FOV))
			{
				Warning("Failed to parse fov \"%s\" in \"%s\" on camera named \"%s\"!\n", fov, filename, name);
				m_MalformedCameras.push_back(name);
				return false;
			}
			else if (newCamera->m_FOV < MIN_FOV || newCamera->m_FOV > MAX_FOV)
			{
				Warning("FOV %f for camera \"%s\" in \"%s\" was out of range (valid range is [%f, %f]). Clamping.",
					newCamera->m_FOV, name, m_ConfigFilename.c_str(), MIN_FOV, MAX_FOV);

				newCamera->m_FOV = std::clamp<float>(newCamera->m_FOV, MIN_FOV, MAX_FOV);
			}
		}
		else
			newCamera->m_FOV = 0;
	}

	m_Cameras.push_back(std::move(newCamera));
	return true;
}

bool AutoCameras::LoadStoryboard(KeyValues* const storyboard, const char* const filename)
{
	auto newStoryboard = std::make_unique<Storyboard>();
	const char* const name = storyboard->GetString("name", nullptr);
	{
		if (!name)
		{
			Warning("Missing requied value \"name\" on a Storyboard in \"%s\"!\n", filename);
			return false;
		}
		newStoryboard->m_Name = name;
	}

	StoryboardElement* lastElement = nullptr;
	for (KeyValues* element = storyboard->GetFirstTrueSubKey(); element; element = element->GetNextTrueSubKey())
	{
		std::unique_ptr<StoryboardElement> newElement;
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
			lastElement = newElement.get();
			newStoryboard->m_FirstElement = std::move(newElement);
		}
		else
		{
			lastElement = newElement.get();
			lastElement->m_Next = std::move(newElement);
		}
	}

	m_Storyboards.push_back(std::move(newStoryboard));
	return true;
}

bool AutoCameras::LoadShot(std::unique_ptr<StoryboardElement>& shotOut, KeyValues* const shotKV, const char* const storyboardName, const char* const filename)
{
	std::unique_ptr<Shot> newShot(new Shot);

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

	shotOut = std::move(newShot);
	return true;
}

bool AutoCameras::LoadAction(std::unique_ptr<StoryboardElement>& actionOut, KeyValues* const actionKV, const char* const storyboardName, const char* const filename)
{
	std::unique_ptr<Action> newAction(new Action);

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

	actionOut = std::move(newAction);
	return true;
}

void AutoCameras::CheckTrigger(const Trigger& trigger, std::vector<C_BaseEntity*>& entities)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);

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

		if (IsBoxIntersectingBox(trigger.m_Mins, trigger.m_Maxs, mins, maxs))
		{
			C_BaseEntity* const baseEntity = clientEnt->GetBaseEntity();
			if (!baseEntity)
				continue;

			entities.push_back(baseEntity);
		}
	}
}

void AutoCameras::ExecuteStoryboardElement(const StoryboardElement& element, C_BaseEntity* const triggerer)
{
	// Shot
	switch (element.GetType())
	{
		case StoryboardElementType::Shot:
		{
			auto& cast = static_cast<const Shot&>(element);
			ExecuteShot(cast, triggerer);
			return;
		}

		case StoryboardElementType::Action:
		{
			auto& cast = static_cast<const Action&>(element);
			ExecuteAction(cast, triggerer);
			return;
		}
	}

	Warning("[%s] Failed to cast StoryboardElement for execution!\n", __FUNCSIG__);
	return;
}

void AutoCameras::ExecuteShot(const Shot& shot, C_BaseEntity* const triggerer)
{
	const auto& camera = shot.m_Camera;

	CameraTools::GetModule()->SpecPosition(camera->m_Pos, camera->m_DefaultAngle);

	if (triggerer && shot.m_Target == Target::FirstPlayer)
	{
		auto const hltvcamera = Interfaces::GetHLTVCamera();
		//Interfaces::GetHLTVCamera()->
		hltvcamera->m_flLastAngleUpdateTime = -1;
		hltvcamera->m_iTraget1 = triggerer->entindex();
		hltvcamera->m_flInertia = 30;
	}
}

void AutoCameras::ExecuteAction(const Action& action, C_BaseEntity* const triggerer)
{
	//Assert(!"Not implemented");
}

void AutoCameras::BeginCameraTrigger()
{
	m_CameraTriggerStart = GetCrosshairTarget();
	m_CreatingCameraTrigger = true;
}

void AutoCameras::EndCameraTrigger()
{
	if (!m_CreatingCameraTrigger)
	{
		PluginWarning("Must use %s before using %s!\n", ce_cameratrigger_begin.GetName(), ce_cameratrigger_end.GetName());
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

void AutoCameras::BeginStoryboard(const CCommand& args)
{
	const auto& storyboard = m_Storyboards.front();
	m_ActiveStoryboard = storyboard.get();
	m_ActiveStoryboardElement = m_ActiveStoryboard->m_FirstElement.get();
}

void AutoCameras::MarkCamera(const CCommand& args)
{
	if (args.ArgC() != 2)
	{
		Warning("Usage: %s <name>\n", ce_autocamera_create.GetName());
		return;
	}

	Vector pos;
	QAngle ang;
	float fov;
	if (!GetView(&pos, &ang, &fov))
		return;

	std::stringstream camera;

	camera << "\tCamera {";

	// name
	{
		camera << " name ";

		const bool nameNeedsQuotes = std::strpbrk(args.Arg(1), "{}()'\": ");
		if (nameNeedsQuotes)
			camera << '"';

		camera << args.Arg(1);

		if (nameNeedsQuotes)
			camera << '"';
	}

	// pos
	{
		camera << " pos \"";

		camera << std::fixed << std::setprecision(1);
		camera << pos.x << ' ' << pos.y << ' ' << pos.z;

		camera << '"';
	}

	// ang
	{
		camera << " ang \"";

		camera << std::fixed << std::setprecision(1);
		camera << ang.x << ' ' << ang.y;

		if (ang.z < -0.1 || ang.z > 0.1)
			camera << ' ' << ang.z;

		camera << '"';
	}

	// fov
	{
		camera << " fov ";

		camera << std::fixed << std::setprecision(1);
		camera << fov;
	}

	camera << " }";

	ConColorMsg(Color(128, 255, 128, 255), "%s\n", camera.str().c_str());
}

void AutoCameras::CycleCamera(const CCommand& args)
{
	if (m_Cameras.size() < 1)
	{
		Warning("%s: No defined cameras for this map!\n", args.Arg(0));
		return;
	}

	if (args.ArgC() < 2 || args.ArgC() > 3)
		goto Usage;

	const char* const groupName = args.ArgC() > 2 ? args.Arg(2) : "";
	auto camGroup = FindCameraGroup(groupName);
	if (!camGroup)
	{
		Warning("%s: Unable to find group with name \"%s\"!\n", args.Arg(0), groupName);
		return;
	}

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
	const Vector camOrigin = CameraState::GetModule()->GetCurrentCamera()->GetOrigin();
	{
		QAngle dummy;
		Assert(CameraState::GetModule());
		if (CameraState::GetModule())
			CameraState::GetModule()->GetCurrentCamera()->GetOrigin();
	}

	// Check if we're already at a camera, in which case we just operate by file (group) order
	{
		const auto compareFunc = [&](const Camera* c) { return c->m_Pos.DistToSqr(camOrigin) < 100; };

		if (dir == NEXT)
		{
			auto found = std::find_if(camGroup->m_Cameras.begin(), camGroup->m_Cameras.end(), compareFunc);
			if (found != camGroup->m_Cameras.end())
			{
				auto next = std::next(found);
				if (next == camGroup->m_Cameras.end())
					return GotoCamera(**camGroup->m_Cameras.begin());
				else
					return GotoCamera(**next);
			}
		}
		else
		{
			auto found = std::find_if(camGroup->m_Cameras.rbegin(), camGroup->m_Cameras.rend(), compareFunc);
			if (found != camGroup->m_Cameras.rend())
			{
				auto next = std::next(found);
				if (next == camGroup->m_Cameras.rend())
					return GotoCamera(**camGroup->m_Cameras.rbegin());
				else
					return GotoCamera(**next);
			}
		}
	}

	const float aspectRatio = 16.0 / 9.0;	// Just blindly assume 16:9 for now

	const Camera* bestCameras[1];

	const Vector boxMins(camOrigin - Vector(0.5));
	const Vector boxMaxs(camOrigin + Vector(0.5));

	const ObserverMode gotoMode = (ObserverMode)ce_autocamera_goto_mode.GetInt();

	auto lastBest = std::partial_sort_copy(camGroup->m_Cameras.begin(), camGroup->m_Cameras.end(), std::begin(bestCameras), std::end(bestCameras),
		[&](const Camera* c1, const Camera* c2)
	{
		const auto dist1 = c1->m_Pos.DistToSqr(camOrigin);
		const auto dist2 = c2->m_Pos.DistToSqr(camOrigin);

		// Check if either of the cameras have our current position within their frustum
		{
			Frustum_t frustum1, frustum2;
			GeneratePerspectiveFrustum(c1->m_Pos, c1->m_DefaultAngle, 1, 100000, GetCameraFOV(*c1, gotoMode), aspectRatio, frustum1);
			GeneratePerspectiveFrustum(c2->m_Pos, c2->m_DefaultAngle, 1, 100000, GetCameraFOV(*c2, gotoMode), aspectRatio, frustum2);

			const bool inFrustum1 = R_CullBox(boxMins, boxMaxs, frustum1);
			const bool inFrustum2 = R_CullBox(boxMins, boxMaxs, frustum2);

			if (inFrustum1 && !inFrustum2)
				return true;
			else if (!inFrustum1 && inFrustum2)
				return false;
			else if (!inFrustum1 && !inFrustum2)
				return dist1 < dist2;
		}

		// Check if either of the cameras have LOS to the current point
		{
			CTraceFilterNoNPCsOrPlayer noPlayers(nullptr, COLLISION_GROUP_NONE);
			trace_t tr1, tr2;
			UTIL_TraceLine(c1->m_Pos, camOrigin, MASK_OPAQUE, &noPlayers, &tr1);
			UTIL_TraceLine(c2->m_Pos, camOrigin, MASK_OPAQUE, &noPlayers, &tr2);

			const bool hasLOS1 = tr1.fraction >= 1;
			const bool hasLOS2 = tr2.fraction >= 1;

			if (hasLOS1 && !hasLOS2)
				return true;
			else if (!hasLOS1 && hasLOS2)
				return false;
			else if (!hasLOS1 && !hasLOS2)
				return dist1 < dist2;
		}

		// Whoever is closest
		return dist1 < dist2;
	});

	if (lastBest == std::begin(bestCameras))
		Warning("%s: Unable to navigate to a camera for some reason.\n", args.Arg(0));
	else //if (dir == NEXT)
		GotoCamera(*bestCameras[0]);
	//else  // dir == PREV
	//	GotoCamera(*bestCameras[1]);

	return;

Usage:
	Warning("%s: Usage: %s <prev/next> [group]\n", args.Arg(0), args.Arg(0));
}

void AutoCameras::SpecPlayer(const CCommand& args)
{
	if (args.ArgC() < 2 || args.ArgC() > 3)
	{
		PluginWarning("Usage: %s <fixed/free/follow> [group]\n", args.Arg(0));
		return;
	}

#if 0
	if (auto currentMode = CameraState::GetModule()->GetLocalObserverMode(); currentMode != OBS_MODE_CHASE && currentMode != OBS_MODE_IN_EYE)
	{
		PluginWarning("%s: Not currently spectating a player in firstperson or thirdperson\n", args.Arg(0));
		return;
	}
#endif

	const auto camState = CameraState::GetModule();
	if (!camState)
	{
		PluginWarning("%s: Camera State module not loaded\n", args.Arg(0));
		return;
	}

	const char* camGroupName = args.ArgC() > 2 ? args.Arg(2) : "";
	auto camGroup = FindCameraGroup(camGroupName);
	if (!camGroup)
	{
		PluginWarning("%s: Unable to find camera group \"%s\"\n", args.Arg(0), args.Arg(2));
		return;
	}

	IClientEntity* observeTarget;
	{
		if (auto playerTarget = CameraState::GetModule()->GetLocalObserverTarget())
			observeTarget = playerTarget->GetBaseEntity();
		else if (auto lastTarget = camState->GetLastSpecTarget())
			observeTarget = lastTarget;
		else
		{
			PluginWarning("%s: Not currently spectating a player\n", args.Arg(0));
			return;
		}
	}

	ObserverMode mode;
	bool bFollow = false;
	if (!stricmp(args.Arg(1), "fixed"))
		mode = OBS_MODE_FIXED;
	else if (!stricmp(args.Arg(1), "free"))
		mode = OBS_MODE_ROAMING;
	else if (!stricmp(args.Arg(1), "follow"))
	{
		mode = OBS_MODE_FIXED;
		bFollow = true;
	}
	else
	{
		PluginWarning("%s: Unknown camera mode \"%s\"\n", args.Arg(0), args.Arg(1));
		return;
	}

	if (ce_autocamera_spec_player_debug.GetBool())
		Msg("Scoring cameras:\n");

	const auto observedOrigin = observeTarget->GetAbsOrigin();

	float bestCameraScore = -std::numeric_limits<float>::max();
	const Camera* bestCamera = nullptr;
	for (const auto& camera : camGroup->m_Cameras)
	{
		const auto cameraScore = ScoreSpecPlayerCamera(*camera, observedOrigin);

		if (ce_autocamera_spec_player_debug.GetBool() && cameraScore > 0)
			Msg("\tcam %s scored %f\n", camera->m_Name.c_str(), cameraScore);

		if (cameraScore > bestCameraScore)
		{
			bestCameraScore = cameraScore;
			bestCamera = camera;
		}
	}

	auto const hltvcamera = Interfaces::GetHLTVCamera();

	if (bestCameraScore <= 0)
	{
		if (ce_autocamera_spec_player_debug.GetBool())
			PluginMsg("%s: No best camera found\n", args.Arg(0));

		if (auto fallback = ce_autocamera_spec_player_fallback.GetString(); fallback[0])
		{
			if (!stricmp(fallback, "firstperson"))
				mode = OBS_MODE_IN_EYE;
			else if (!stricmp(fallback, "thirdperson"))
				mode = OBS_MODE_CHASE;
			else if (!stricmp(fallback, "none"))
				return;
			else
			{
				PluginWarning("%s: unknown fallback mode \"%s\"\n", ce_autocamera_spec_player_fallback.GetName(), fallback);
				return;
			}

			hltvcamera->SetMode(mode);
		}

		return;
	}

	Assert(bestCamera);

	const auto activeCam = camState->GetCurrentCamera();
	if (bestCamera != m_LastActiveCamera ||
		!VectorsAreEqual(activeCam->GetOrigin(), m_LastActiveCamera->m_Pos, 25) ||
		(!bFollow && !QAnglesAreEqual(activeCam->GetAngles(), m_LastActiveCamera->m_DefaultAngle)))
	{
		GotoCamera(*bestCamera, mode);

		if (bFollow)
		{
			// Since we just switched positions, snap angles to target player
			VectorAngles((observedOrigin - bestCamera->m_Pos).Normalized(), hltvcamera->m_aCamAngle);
			hltvcamera->m_flLastAngleUpdateTime = -1;
		}
	}

	if (bFollow)
		hltvcamera->SetPrimaryTarget(observeTarget->entindex());
}

float AutoCameras::ScoreSpecPlayerCamera(const Camera& camera, const Vector& position, const IHandleEntity* targetEnt) const
{
	float score = 1;

	const Vector toPosition(position - camera.m_Pos);

	if (const float fov = ce_autocamera_spec_player_fov.GetFloat(); score > 0 && fov > 0)
	{
		Vector camForward;
		AngleVectors(camera.m_DefaultAngle, &camForward);

		const float ang = Rad2Deg(std::acos(camForward.Dot(toPosition.Normalized())));

		score *= RemapVal(ang, 0, fov, 1, 0);
	}
	if (const float dist = ce_autocamera_spec_player_dist.GetFloat(); score > 0 && dist > 0)
	{
		const auto positionDist = toPosition.Length();
		constexpr auto minDistBegin = 256;
		constexpr auto minDistEnd = 128;
		score *= RemapValClamped(positionDist, minDistEnd, minDistBegin, 0, 1) * RemapVal(positionDist, 256, dist, 1, 0);
	}
	if (const float los = ce_autocamera_spec_player_los.GetFloat(); score > 0 && los > 0)
	{
		score *= CameraTools::CollisionTest3D(camera.m_Pos, position, los, targetEnt);
	}

	return score;
}

void AutoCameras::GotoCamera(const Camera& camera)
{
	GotoCamera(camera, (ObserverMode)ce_autocamera_goto_mode.GetInt());
}

void AutoCameras::GotoCamera(const Camera& camera, ObserverMode mode)
{
	CameraTools* const ctools = CameraTools::GetModule();
	if (!ctools)
	{
		PluginWarning("%s: Camera Tools module unavailable!\n", __FUNCSIG__);
		return;
	}

	Assert(mode == OBS_MODE_FIXED || mode == OBS_MODE_ROAMING);

	auto fov = camera.m_FOV;
	if (fov == 0)
	{
		if (auto fovOverride = FOVOverride::GetModule())
			fov = fovOverride->GetBaseFOV(mode);
		else
		{
			static ConVarRef fov_desired("fov_desired");
			fov = fov_desired.GetFloat();
		}
	}

	ctools->SpecPosition(camera.m_Pos, camera.m_DefaultAngle, mode, fov);
	m_LastActiveCamera = &camera;
}

void AutoCameras::GotoCamera(const CCommand& args)
{
	if (args.ArgC() != 2)
	{
		Warning("Usage: %s <camera name>\n", args.Arg(0));
		return;
	}

	const char* const name = args.Arg(1);
	auto const cam = FindCamera(name);
	if (!cam)
	{
		if (FindContainedString(m_MalformedCameras, name))
			Warning("%s: Unable to goto camera \"%s\" because its definition in \"%s\" is malformed.\n", args.Arg(0), name, m_ConfigFilename.c_str());
		else
			Warning("%s: Unable to find camera with name \"%s\"!\n", args.Arg(0), name);

		return;
	}

	GotoCamera(*cam);
}

int AutoCameras::GotoCameraCompletion(const char* const partial, char commands[COMMAND_COMPLETION_MAXITEMS][COMMAND_COMPLETION_ITEM_LENGTH])
{
	std::cmatch match;
	const std::string regexString = strprintf("\\s*%s\\s+\"?(.*?)\"?\\s*$", GetModule()->ce_autocamera_goto.GetName());

	// Interesting... this only works if you pass a const char* to std::regex's constructor, and not if you pass an std::string.
	if (!std::regex_search(partial, match, std::regex(regexString.c_str(), std::regex_constants::icase)))
		return 0;

	AutoCameras* const mod = GetModule();

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
		strcpy_s(commands[i], COMMAND_COMPLETION_ITEM_LENGTH, mod->ce_autocamera_goto.GetName());
		const auto& nameStr = cameras[i]->m_Name;
		strcat_s(commands[i], COMMAND_COMPLETION_ITEM_LENGTH, (std::string(" ") + nameStr).c_str());
	}
	return i;
}

float AutoCameras::GetCameraFOV(const Camera& camera, ObserverMode mode) const
{
	if (camera.m_FOV != 0)
		return camera.m_FOV;
	else if (auto fov = FOVOverride::GetModule())
		return fov->GetBaseFOV(mode);
	else
	{
		static ConVarRef fov_desired("fov_desired");
		return fov_desired.GetFloat();
	}
}

void AutoCameras::DrawTriggers()
{
	for (const auto& trigger : m_Triggers)
	{
		NDebugOverlay::Box(Vector(0, 0, 0), trigger->m_Mins, trigger->m_Maxs, 128, 255, 128, 64, 0);

		const Vector difference = VectorLerp(trigger->m_Mins, trigger->m_Maxs, 0.5);
		NDebugOverlay::Text(VectorLerp(trigger->m_Mins, trigger->m_Maxs, 0.5), trigger->m_Name.c_str(), false, 0);
	}
}

void AutoCameras::DrawCameras()
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	for (const auto& camera : m_Cameras)
	{
		Vector forward, up, right;
		AngleVectors(camera->m_DefaultAngle, &forward, &right, &up);

		// Draw cameras
		if (ce_autocamera_show_cameras.GetInt() > 0)
		{
			NDebugOverlay::BoxAngles(camera->m_Pos, Vector(-16, -16, -16), Vector(16, 16, 16), camera->m_DefaultAngle, 128, 128, 255, 64, 0);

			const Vector endPos = camera->m_Pos + (forward * 128);
			NDebugOverlay::Line(camera->m_Pos, endPos, 128, 128, 255, true, 0);
			NDebugOverlay::Cross3DOriented(endPos, camera->m_DefaultAngle, 16, 128, 128, 255, true, 0);

			NDebugOverlay::Text(camera->m_Pos, camera->m_Name.c_str(), false, 0);
		}

		// From http://stackoverflow.com/a/27872276
		// Draw camera frustums
		if (ce_autocamera_show_cameras.GetInt() > 1)
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

const AutoCameras::Trigger* AutoCameras::FindTrigger(const char* const triggerName) const
{
	for (const auto& trigger : m_Triggers)
	{
		if (!stricmp(trigger->m_Name.c_str(), triggerName))
			return trigger.get();
	}

	return nullptr;
}

const AutoCameras::CameraGroup* AutoCameras::FindCameraGroup(const char* groupName) const
{
	for (const auto& group : m_CameraGroups)
	{
		if (!stricmp(group->m_Name.c_str(), groupName))
			return group.get();
	}

	return nullptr;
}

void AutoCameras::CreateDefaultCameraGroup()
{
	Assert(!FindCameraGroup(""));

	auto newGroup = std::make_unique<CameraGroup>();

	for (const auto& camera : m_Cameras)
		newGroup->m_Cameras.push_back(camera.get());

	newGroup->m_Cameras.shrink_to_fit();
	m_CameraGroups.insert(m_CameraGroups.begin(), std::move(newGroup));
}

const AutoCameras::Camera* AutoCameras::FindCamera(const char* const cameraName) const
{
	for (const auto& camera : m_Cameras)
	{
		if (!stricmp(camera->m_Name.c_str(), cameraName))
			return camera.get();
	}

	return nullptr;
}

std::vector<const AutoCameras::Camera*> AutoCameras::GetAlphabeticalCameras() const
{
	std::vector<const Camera*> retVal;
	for (const auto& cam : m_Cameras)
		retVal.push_back(cam.get());

	std::sort(retVal.begin(), retVal.end(),
		[](const Camera* a, const Camera* b) { return stricmp(a->m_Name.c_str(), b->m_Name.c_str()); });

	return retVal;
}

void AutoCameras::SetupMirroredCameras()
{
	for (const auto& camera : m_Cameras)
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

		// bad programmer alert
		Camera* cameraEdit = const_cast<Camera*>(camera.get());

		const Vector relativeBasePos = camToMirror->m_Pos - m_MapOrigin;
		cameraEdit->m_Pos = Vector(-relativeBasePos.x, -relativeBasePos.y, relativeBasePos.z) + m_MapOrigin;
		cameraEdit->m_DefaultAngle = QAngle(
			AngleNormalize(camToMirror->m_DefaultAngle.x),
			AngleNormalize(camToMirror->m_DefaultAngle.y + 180),
			AngleNormalize(camToMirror->m_DefaultAngle.z));
	}
}

bool AutoCameras::FindContainedString(const std::vector<std::string>& vec, const char* str)
{
	for (const std::string& x : vec)
	{
		if (!stricmp(x.c_str(), str))
			return true;
	}

	return false;
}
