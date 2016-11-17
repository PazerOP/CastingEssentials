#include "MapFlythroughs.h"
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
	ce_cameratrigger_begin = new ConCommand("ce_autocamera_trigger_begin", [](const CCommand& args) { GetModule()->BeginCameraTrigger(); });
	ce_cameratrigger_end = new ConCommand("ce_autocamera_trigger_end", [](const CCommand& args) { GetModule()->EndCameraTrigger(); });

	ce_autocamera_begin_storyboard = new ConCommand("ce_autocamera_begin_storyboard", [](const CCommand& args) { GetModule()->BeginStoryboard(args); });

	ce_autocamera_mark_camera = new ConCommand("ce_autocamera_mark_camera", [](const CCommand& args) { GetModule()->MarkCamera(args); });
	ce_autocamera_goto_camera = new ConCommand("ce_autocamera_goto_camera", [](const CCommand& args) { GetModule()->GotoCamera(args); }, nullptr, 0, &MapFlythroughs::GotoCameraCompletion);

	ce_autocamera_reload_config = new ConCommand("ce_autocamera_reload_config", [](const CCommand& args) { GetModule()->LoadConfig(); });

	ce_autocamera_show_triggers = new ConVar("ce_autocamera_show_triggers", "0", FCVAR_NONE, "Shows all triggers on the map.");

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
	m_Triggers.clear();
	m_Cameras.clear();
	m_Storyboards.clear();

	std::string mapName(bspName);
	mapName.erase(mapName.size() - 4, 4);	// remove .bsp
	mapName.erase(0, 5);					// remove /maps/

	KeyValuesAD kv("AutoCameras");
	const std::string filename = strprintf("addons/castingessentials/autocameras/%s.vdf", mapName.c_str());
	if (!kv->LoadFromFile(Interfaces::GetFileSystem(), filename.c_str()))
	{
		PluginWarning("Unable to load autocameras from %s!\n", filename.c_str());
		return;
	}

	for (KeyValues* trigger = kv->GetFirstTrueSubKey(); trigger; trigger = trigger->GetNextTrueSubKey())
	{
		if (stricmp("trigger", trigger->GetName()))
			continue;

		if (!LoadTrigger(trigger, filename.c_str()))
			return;
	}

	for (KeyValues* camera = kv->GetFirstTrueSubKey(); camera; camera = camera->GetNextTrueSubKey())
	{
		if (stricmp("camera", camera->GetName()))
			continue;

		if (!LoadCamera(camera, filename.c_str()))
			return;
	}

	for (KeyValues* storyboard = kv->GetFirstTrueSubKey(); storyboard; storyboard = storyboard->GetNextTrueSubKey())
	{
		if (stricmp("storyboard", storyboard->GetName()))
			continue;

		if (!LoadStoryboard(storyboard, filename.c_str()))
			return;
	}

	PluginMsg("Loaded autocameras from %s.\n", filename.c_str());
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
			return false;
		}
		if (!ParseVector(newTrigger->m_Mins, mins))
		{
			Warning("Failed to parse mins \"%s\" in \"%s\" on trigger named \"%s\"!\n", mins, filename, name);
			return false;
		}
	}

	// maxs
	{
		const char* const maxs = trigger->GetString("maxs", nullptr);
		if (!maxs)
		{
			Warning("Missing required value \"maxs\" in \"%s\" on trigger named \"%s\"!\n", filename, name);
			return false;
		}
		if (!ParseVector(newTrigger->m_Maxs, maxs))
		{
			Warning("Failed to parse maxs \"%s\" in \"%s\" on trigger named \"%s\"!\n", maxs, filename, name);
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

	// pos
	{
		const char* const pos = camera->GetString("pos", nullptr);
		if (!pos)
		{
			Warning("Missing required value \"pos\" in \"%s\" on camera named \"%s\"!\n", filename, name);
			return false;
		}
		if (!ParseVector(newCamera->m_Pos, pos))
		{
			Warning("Failed to parse pos \"%s\" in \"%s\" on camera named \"%s\"!\n", pos, filename, name);
			return false;
		}
	}

	// ang
	{
		const char* const ang = camera->GetString("ang", nullptr);
		if (!ang)
		{
			Warning("Missing required value \"ang\" in \"%s\" on camera named \"%s\"!\n", filename, name);
			return false;
		}
		if (!ParseAngle(newCamera->m_DefaultAngle, ang))
		{
			Warning("Failed to parrse ang \"%s\" in \"%s\" on camera named \"%s\"!\n", ang, filename, name);
			return false;
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
				return false;
		}
		else if (!stricmp("action", element->GetName()))
		{
			if (!LoadAction(newElement, element, name, filename))
				return false;
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

void MapFlythroughs::GotoCamera(const CCommand& args)
{
	CameraTools* const ctools = CameraTools::GetModule();
	if (!ctools)
	{
		PluginWarning("%s: %s module unavailable!\n", ce_autocamera_goto_camera->GetName(), CameraTools::GetModuleName());
		return;
	}

	if (args.ArgC() != 2)
	{
		Warning("Usage: %s <camera name>\n", ce_autocamera_goto_camera->GetName());
		return;
	}

	const char* const name = args.Arg(1);
	auto const cam = FindCamera(name);
	if (!cam)
	{
		Warning("%s: Unable to find camera with name \"%s\"!\n", ce_autocamera_goto_camera->GetName(), name);;
		return;
	}

	ctools->SpecPosition(cam->m_Pos, cam->m_DefaultAngle);
}

int MapFlythroughs::GotoCameraCompletion(const char* const partial, char commands[COMMAND_COMPLETION_MAXITEMS][COMMAND_COMPLETION_ITEM_LENGTH])
{
	std::cmatch match;
	if (!std::regex_search(partial, match, std::regex("\\s*ce_autocamera_goto_camera \\s*\"?(.*?)\"?\\s*$", std::regex_constants::icase)))
		return 0;
	
	MapFlythroughs* const mod = GetModule();

	std::vector<std::shared_ptr<Camera>> _cameras;
	std::vector<std::shared_ptr<Camera>>* cameras;
	if (match[1].length() < 1)
		cameras = &mod->m_Cameras;
	else
	{
		const char* const matchStr = match[1].first;
		for (size_t i = 0; i < mod->m_Cameras.size(); i++)
		{
			if (!stristr(mod->m_Cameras[i]->m_Name.c_str(), matchStr))
				continue;

			_cameras.push_back(mod->m_Cameras[i]);
		}

		cameras = &_cameras;
	}

	size_t i;
	for (i = 0; i < COMMAND_COMPLETION_MAXITEMS && i < cameras->size(); i++)
	{
		strcpy_s(commands[i], COMMAND_COMPLETION_ITEM_LENGTH, mod->ce_autocamera_goto_camera->GetName());
		const auto& nameStr = cameras->at(i)->m_Name;
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