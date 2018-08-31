#include "CameraState.h"
#include "PluginBase/Entities.h"
#include "PluginBase/EntityOffsetIterator.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"
#include "Misc/CCvar.h"
#include "Misc/HLTVCameraHack.h"
#include "Modules/Camera/ICamera.h"
#include "Modules/Camera/OrbitCamera.h"
#include "Modules/Camera/PlayerCameraGroup.h"
#include "Modules/CameraTools.h"
#include "Modules/CameraSmooths.h"
#include "Modules/FOVOverride.h"

#include <cdll_int.h>
#include <client/c_basecombatweapon.h>
#include <client/c_baseplayer.h>
#include <client/c_baseviewmodel.h>
#include <client/input.h>
#include <con_nprint.h>
#include <model_types.h>
#include <toolframework/ienginetool.h>
#include <vprof.h>

#undef min
#undef max

static IClientEntityList* s_ClientEntityList;
static IEngineTool* s_EngineTool;
static HLTVCameraOverride* s_HLTVCamera;
static IVEngineClient* s_EngineClient;

static EntityOffset<ObserverMode> s_PlayerObserverModeOffset;
static EntityOffset<EHANDLE> s_PlayerObserverTargetOffset;
static EntityOffset<CHandle<C_BaseViewModel>[2]> s_PlayerViewModelsOffset;
static EntityOffset<CHandle<C_BaseCombatCharacter>> s_WeaponOwnerOffset;

MODULE_REGISTER(CameraState);

class EngineCamera final : public ICamera
{
public:
	EngineCamera()
	{
		m_Type = CameraType::Roaming;
		m_Origin.Init();
		m_FOV = 90;
		m_Angles.Init();
	}

private:
	void Reset() override {}
	void Update(float dt, uint32_t frame) override {}
	int GetAttachedEntity() const override { return 0; }
	bool IsCollapsible() const override { return false; }
	const char* GetDebugName() const override { return "EngineCamera"; }
};

static const auto s_DebugOrbitCamera = std::invoke([]()
{
	auto orbit = std::make_shared<OrbitCamera>();
	orbit->m_OrbitPoint.Init();
	orbit->m_Radius = 1000;
	orbit->m_ZOffset = 500;
	return orbit;
});

CameraState::CameraState() :
	ce_camerastate_clear_cameras("ce_camerastate_clear_cameras", []() { GetModule()->ClearCameras(); }),

	ce_camerastate_debug("ce_camerastate_debug", "0"),
	ce_camerastate_debug_cameras("ce_camerastate_debug_cameras", "0"),

	m_CalcViewPlayerHook(std::bind(&CameraState::CalcViewPlayerOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6)),
	m_CalcViewHLTVHook(std::bind(&CameraState::CalcViewHLTVOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)),

	m_OnPostInternalDrawViewmodelHook(std::bind(&CameraState::OnPostInternalDrawViewmodelOverride, this, std::placeholders::_1, std::placeholders::_2), true),
	m_DrawEconEntityAttachedModelsHook(std::bind(&CameraState::DrawEconEntityAttachedModelsOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), true),
	m_DrawViewmodelHook(std::bind(&CameraState::DrawViewmodelOverride, this, std::placeholders::_1, std::placeholders::_2), true),
	m_DrawBaseViewmodelHook(std::bind(&CameraState::DrawBaseViewmodelOverride, this, std::placeholders::_1, std::placeholders::_2), true),
	m_InternalDrawBaseViewmodelHook(std::bind(&CameraState::InternalDrawBaseViewmodelHook, this, std::placeholders::_1, std::placeholders::_2), true),
	m_DrawBaseAnimatingHook(std::bind(&CameraState::DrawBaseAnimatingOverride, this, std::placeholders::_1, std::placeholders::_2), true),
	m_PlayerDrawOverriddenViewmodelHook(std::bind(&CameraState::PlayerDrawOverriddenViewmodelOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), true),
	m_EconDrawViewmodelHook(std::bind(&CameraState::EconDrawViewmodelOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), true),
	m_EconIsOverridingViewmodelHook(std::bind(&CameraState::EconIsOverridingViewmodelOverride, this, std::placeholders::_1), true),
	m_UpdateAttachmentModelsHook(std::bind(&CameraState::UpdateAttachmentModelsOverride, this, std::placeholders::_1), true),
	m_DrawWeaponHook(std::bind(&CameraState::DrawWeaponOverride, this, std::placeholders::_1, std::placeholders::_2), true),

	m_CreateMoveHook(std::bind(&CameraState::CreateMoveOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)),

	m_EngineCamera(std::make_shared<EngineCamera>()),
	m_RoamingCamera(std::make_shared<RoamingCamera>())
{
	m_LastSpecTarget = 0;
	m_LastSpecMode = OBS_MODE_NONE;
	m_LastSpecMode = m_DesiredSpecMode = ObserverMode::OBS_MODE_ROAMING;
	m_LastSpecTarget = m_DesiredSpecTarget = 0;

	m_ActiveCamera = m_EngineCamera;
}

bool CameraState::CheckDependencies()
{
	if (!CheckDependency(Interfaces::GetClientEntityList(), s_ClientEntityList))
		return false;
	if (!CheckDependency(Interfaces::GetEngineTool(), s_EngineTool))
		return false;
	if (!CheckDependency(Interfaces::GetHLTVCamera(), s_HLTVCamera))
		return false;
	if (!CheckDependency(Interfaces::GetEngineClient(), s_EngineClient))
		return false;

	Entities::GetEntityProp(s_PlayerObserverModeOffset, "CBasePlayer", "m_iObserverMode");
	Entities::GetEntityProp(s_PlayerObserverTargetOffset, "CBasePlayer", "m_hObserverTarget");
	Entities::GetEntityProp(s_PlayerViewModelsOffset, "CTFPlayer", "m_hViewModel[0]");
	Entities::GetEntityProp(s_WeaponOwnerOffset, "CBaseCombatWeapon", "m_hOwner");

	return true;
}

ObserverMode CameraState::GetEngineObserverMode()
{
	if (s_EngineClient->IsHLTV())
		return s_HLTVCamera->GetMode();

	if (const auto localPlayer = Player::GetLocalPlayer())
		return localPlayer->GetObserverMode();

	Assert(!"Unable to determine current observer mode");
	return OBS_MODE_NONE;
}
IClientEntity* CameraState::GetEngineObserverTarget(bool attachedModesOnly)
{
	if (attachedModesOnly)
	{
		if (auto mode = GetEngineObserverMode(); mode != ObserverMode::OBS_MODE_CHASE && mode != ObserverMode::OBS_MODE_IN_EYE)
			return nullptr;
	}

	if (s_EngineClient->IsHLTV())
		return s_HLTVCamera->GetPrimaryTarget();
	else if (auto local = Player::GetLocalPlayer())
		return local->GetObserverTarget();

	return nullptr;
}

ObserverMode CameraState::GetLocalObserverMode() const
{
	if (m_ActiveCamera == m_EngineCamera)
	{
		if (s_EngineClient->IsHLTV())
			return s_HLTVCamera->GetMode();

		if (const auto localPlayer = Player::GetLocalPlayer())
			return localPlayer->GetObserverMode();
	}
	else if (m_ActiveCamera)
	{
		return ToObserverMode(m_ActiveCamera->GetCameraType());
	}

	Assert(!"Unable to determine current observer mode");
	return OBS_MODE_NONE;
}

IClientEntity* CameraState::GetLocalObserverTarget(bool attachedModesOnly) const
{
	if (m_ActiveCamera)
	{
		if (attachedModesOnly)
		{
			if (auto mode = GetLocalObserverMode(); mode != ObserverMode::OBS_MODE_CHASE && mode != ObserverMode::OBS_MODE_IN_EYE)
				return nullptr;
		}

		return s_ClientEntityList->GetClientEntity(m_ActiveCamera->GetAttachedEntity());
	}
	else
		return GetEngineObserverTarget();
}

void CameraState::SetCamera(const CameraPtr& camera)
{
	m_LastCameraGroupCamera.reset();
	m_CurrentCameraGroup.reset();
	SetCameraInternal(camera);
}

void CameraState::SetCameraSmoothed(CameraPtr camera)
{
	m_LastCameraGroupCamera.reset();
	m_CurrentCameraGroup.reset();
	SetCameraSmoothedInternal(std::move(camera));
}

void CameraState::SetCameraGroup(const CameraGroupPtr& group)
{
	m_CurrentCameraGroup = group;
	group->GetBestCamera(m_ActiveCamera);
	m_LastCameraGroupCamera = m_ActiveCamera;
}

void CameraState::SetCameraGroupSmoothed(const CameraGroupPtr& group)
{
	m_CurrentCameraGroup = group;

	CameraPtr newCam = m_ActiveCamera;

	group->GetBestCamera(newCam);
	if (newCam != m_ActiveCamera)
		SetCameraSmoothedInternal(newCam);

	m_LastCameraGroupCamera = newCam;
}

static const char* IsDrawing(C_BaseAnimating* ent)
{
	if (ent->IsEFlagSet(EF_NODRAW))
		return "EF_NODRAW";

	if (!ent->ShouldDraw())
		return "ShouldDraw() == false";

	if (!ent->IsVisible())
		return "IsVisible() == false";

	if (!ent->m_bReadyToDraw)
		return "Not ready to draw";

	if (!ent->GetModelPtr())
		return "No model pointer";

	if (!ent->GetModelIndex())
		return "No model index";

	if (!ent->GetModel())
		return "No model_t";

	if (!ent->GetModelInstance())
		return "No model instance";

	if (ent->IsDynamicModelLoading())
		return "Dynamic model loading";

	return "true";
}

bool CameraState::CalcView(Vector& origin, QAngle& angles, float& fov)
{
	UpdateFromCameraGroup();

	Assert(m_ActiveCamera);
	if (!m_ActiveCamera)
		return false;

	auto& callbacks = CameraStateCallbacks::GetCallbacksParent();

	const auto lastMode = m_ActiveCamera->GetCameraType();
	m_ActiveCamera->TryUpdate(s_EngineTool->ClientFrameTime(), s_EngineTool->HostFrameCount());
	const auto newMode = m_ActiveCamera->GetCameraType();

	if (m_ActiveCamera->IsCollapsible())
	{
		auto prevCamera = m_ActiveCamera;
		auto newCamera = prevCamera;
		if (ICamera::TryCollapse(newCamera))
		{
			callbacks.CameraCollapsed(prevCamera, newCamera);
			SetCameraInternal(newCamera);
		}
	}

	origin = m_ActiveCamera->GetOrigin();
	angles = m_ActiveCamera->GetAngles();
	fov = m_ActiveCamera->GetFOV();

	Assert(origin.IsValid());
	Assert(angles.IsValid());
	Assert(std::isfinite(fov));

	return true;
}

void CameraState::UpdateViewmodels()
{
	// These are junk and/or uninteresting to us
	static const RecvTable* excludeTables[] =
	{
		Entities::FindRecvTable("DT_AttributeList"),
		Entities::FindRecvTable("DT_CollisionProperty"),
		Entities::FindRecvTable("DT_EconEntity"),
		Entities::FindRecvTable("m_flPoseParameter"),
	};

	const auto mode = GetLocalObserverMode();
	auto localPlayer = Player::GetLocalPlayer();
	if (auto playerTarget = Player::AsPlayer(GetLocalObserverTarget()))
	{
		if (auto basePlayer = playerTarget->GetBasePlayer())
		{
			basePlayer->CalcViewModelView(m_ActiveCamera->GetOrigin(), m_ActiveCamera->GetAngles());

			auto& viewmodels = s_PlayerViewModelsOffset.GetValue(basePlayer);
			for (auto& vm : viewmodels)
			{
				auto vmEnt = vm.Get();
				if (!vmEnt)
					continue;

				Assert(vmEnt->IsViewModel());

				con_nprint_s info(0, -1);
				engine->Con_NXPrintf(GetConLine(info), "");

				auto minfo = Interfaces::GetModelInfoClient();

				const auto attachNum = vmEnt->GetNumBoneAttachments();
				for (int i = 0; i < attachNum; i++)
				{
					auto attachment = vmEnt->GetBoneAttachment(i);
					engine->Con_NXPrintf(GetConLine(info), "attachment %i: %s", i, minfo->GetModelName(attachment->GetModel()));

					Assert(attachment);
				}

				auto cc = vmEnt->GetClientClass();
				//bool isnodraw = IsDrawing(vmEnt);
				//vmEnt->UpdateVisibility();
				//bool isnodraw2 = IsDrawing(vmEnt);

				engine->Con_NXPrintf(GetConLine(info), "Viewmodel: %i (%s, %s), drawing? %s (%i)",
					vmEnt->entindex(), cc->GetName(), FirstNotNull(minfo->GetModelName(vmEnt->GetModel()), "(null)"),
					IsDrawing(vmEnt), m_LastDrawViewmodelResult);

				if (auto owningWep = vmEnt->GetOwningWeapon())
				{
					engine->Con_NXPrintf(GetConLine(info), "Owning weapon: %i (%s, %s), drawing? %s",
						owningWep->entindex(), owningWep->GetClientClass()->GetName(),
						FirstNotNull(minfo->GetModelName(owningWep->GetModel()), "(null)"), IsDrawing(owningWep));
				}

				//for (const auto& prop : EntityOffsetIterable<int>(cc->m_pRecvTable, std::begin(excludeTables), std::end(excludeTables)))
				//	engine->Con_NXPrintf(GetConLine(info), "%s: %i", prop.first.c_str(), prop.second.GetValue(vmEnt));

				engine->Con_NXPrintf(GetConLine(info), "");

				if (auto wep = vmEnt->GetWeapon())
				{
					m_DrawViewmodelEntity = vmEnt->entindex();
					m_DrawWeaponEntity = wep->entindex();

					//static_assert(false, "TODO: only call this when we absolutely need to.");
					wep->UpdateAttachmentModels();

					auto ccWep = wep->GetClientClass();
					auto isdrawing = IsDrawing(wep);
					//wep->UpdateVisibility();
					//isnodraw2 = IsDrawing(wep);

					engine->Con_NXPrintf(GetConLine(info), "Weapon: %i (%s, %s), drawing? %s (%i, %i)",
						wep->entindex(), ccWep->GetName(), FirstNotNull(minfo->GetModelName(wep->GetModel()), "(null)"),
						isdrawing, m_LastDrawWeaponResult, m_LastDrawBaseAnimatingResult);

					//for (const auto& prop : EntityOffsetIterable<int>(ccWep->m_pRecvTable, std::begin(excludeTables), std::end(excludeTables)))
					//	engine->Con_NXPrintf(GetConLine(info), "%s: %i", prop.first.c_str(), prop.second.GetValue(wep));
				}

				Assert(true);
			}
		}
	}
}

class C_TFViewModel : public C_BaseViewModel {};
class C_TFPlayer : public C_BasePlayer {};

static constexpr Color ENTER(128, 255, 128);
static constexpr Color EXIT(255, 128, 128);

static bool s_InDrawViewmodel = false;
int CameraState::DrawViewmodelOverride(C_TFViewModel* pThis, int flags)
{
	if (!(flags & STUDIO_RENDER))
		return 0;

	m_DrawViewmodelHook.SetState(Hooking::HookAction::SUPERCEDE);

	auto hack = reinterpret_cast<C_TFViewModel*>(((int*)pThis) - 1);
	auto cc = hack->GetClientClass();
	auto minfo = Interfaces::GetModelInfoClient();

	Con_Printf(ENTER, "C_TFViewModel::DrawModel - %i (%s, %s)", hack->entindex(), cc->GetName(), minfo->GetModelName(hack->GetModel()));
	auto pusher = CreateVariablePusher(s_InDrawViewmodel, true);
	auto retVal = m_DrawViewmodelHook.GetOriginal()(pThis, flags);
	Con_Printf(EXIT, "C_TFViewModel::DrawModel");

	return retVal;
}

int CameraState::DrawBaseViewmodelOverride(C_BaseViewModel* pThis, int flags)
{
	if (!(flags & STUDIO_RENDER))
		return 0;

	m_DrawBaseViewmodelHook.SetState(Hooking::HookAction::SUPERCEDE);

	auto hack = reinterpret_cast<C_BaseViewModel*>(((int*)pThis) - 1);
	auto cc = hack->GetClientClass();
	auto minfo = Interfaces::GetModelInfoClient();

	Con_Printf(ENTER, "C_BaseViewModel::DrawModel - %i (%s, %s)", hack->entindex(), cc->GetName(), minfo->GetModelName(hack->GetModel()));
	m_LastDrawViewmodelResult = m_DrawBaseViewmodelHook.GetOriginal()(pThis, flags);
	Con_Printf(EXIT, "C_BaseViewModel::DrawModel");

	return m_LastDrawViewmodelResult;
}

int CameraState::InternalDrawBaseViewmodelHook(C_BaseViewModel* pThis, int flags)
{
	if (!(flags & STUDIO_RENDER))
		return 0;

	m_InternalDrawBaseViewmodelHook.SetState(Hooking::HookAction::SUPERCEDE);

	auto hack = pThis;//reinterpret_cast<C_BaseAnimating*>(((int*)pThis) - 1);
	auto cc = hack->GetClientClass();
	auto minfo = Interfaces::GetModelInfoClient();

	Con_Printf(ENTER, "C_BaseViewModel::InternalDrawModel - %i (%s, %s)", hack->entindex(), cc->GetName(), minfo->GetModelName(hack->GetModel()));
	auto retVal = m_InternalDrawBaseViewmodelHook.GetOriginal()(pThis, flags);
	Con_Printf(EXIT, "C_BaseViewModel::InternalDrawModel");

	return retVal;
}

int CameraState::DrawBaseAnimatingOverride(C_BaseAnimating* pThis, int flags)
{
	if (!(flags & STUDIO_RENDER))
		return 0;

	auto hack = reinterpret_cast<C_BaseAnimating*>(((int*)pThis) - 1);
	auto cc = hack->GetClientClass();
	auto minfo = Interfaces::GetModelInfoClient();

	/*if (hack->entindex() == m_DrawViewmodelEntity)
	{
		//m_DrawViewmodelHook.SetState(Hooking::HookAction::SUPERCEDE);
		auto pusher = CreateVariablePusher(s_InDrawViewmodel, true);
		m_DrawBaseAnimatingHook.SetState(Hooking::HookAction::SUPERCEDE);
		return m_DrawBaseAnimatingHook.GetOriginal()(pThis, flags);
	}
	else*/ if (s_InDrawViewmodel)
	{
		//Con_Printf("C_BaseAnimating::DrawModel: %i (%s, %s)", hack->entindex(), cc->GetName(), minfo->GetModelName(hack->GetModel()));
		m_DrawBaseAnimatingHook.SetState(Hooking::HookAction::SUPERCEDE);
		Con_Printf(ENTER, "C_BaseAnimating::DrawModel - %i (%s, %s)", hack->entindex(), cc->GetName(), minfo->GetModelName(hack->GetModel()));
		m_LastDrawBaseAnimatingResult = m_DrawBaseAnimatingHook.GetOriginal()(pThis, flags);
		Con_Printf(EXIT, "C_BaseAnimating::DrawModel");
		return m_LastDrawBaseAnimatingResult;
	}

	return 0;
}

void CameraState::DrawEconEntityAttachedModelsOverride(C_BaseAnimating* pBase, C_EconEntity* pModels, ClientModelRenderInfo_t const* info, int mode)
{
	m_DrawEconEntityAttachedModelsHook.SetState(Hooking::HookAction::SUPERCEDE);
	return;
}

bool CameraState::OnPostInternalDrawViewmodelOverride(C_TFViewModel* pThis, ClientModelRenderInfo_t* info)
{
	m_OnPostInternalDrawViewmodelHook.SetState(Hooking::HookAction::SUPERCEDE);
	return 0;
}

int CameraState::PlayerDrawOverriddenViewmodelOverride(C_TFPlayer* pThis, C_BaseViewModel* viewmodel, int flags)
{
	m_PlayerDrawOverriddenViewmodelHook.SetState(Hooking::HookAction::SUPERCEDE);
	return 0;
}

int CameraState::EconDrawViewmodelOverride(C_EconEntity* pThis, C_BaseViewModel* viewmodel, int flags)
{
	//m_EconDrawViewmodelHook.SetState(Hooking::HookAction::SUPERCEDE);
	return 0;
}

bool CameraState::EconIsOverridingViewmodelOverride(C_EconEntity* pThis)
{
	//m_EconIsOverridingViewmodelHook.SetState(Hooking::HookAction::SUPERCEDE);
	return false;

	auto ent = pThis->m_Something.Get();
	auto ccEnt = ent ? ent->GetClientClass() : nullptr;
	auto outer = pThis->m_AttributeManager.m_hOuter.Get();
	auto outerCC = outer ? outer->GetClientClass() : nullptr;
	auto teamNum = pThis->GetTeamNumber();

	auto original = m_EconIsOverridingViewmodelHook.GetOriginal()(pThis);
	return false;
}

bool CameraState::UpdateAttachmentModelsOverride(C_EconEntity* pThis)
{
	auto entindex = pThis->entindex();
	if (entindex != m_DrawWeaponEntity && entindex != m_DrawViewmodelEntity)
		return false;

	m_UpdateAttachmentModelsHook.SetState(Hooking::HookAction::SUPERCEDE);
	Msg("C_EconEntity::UpdateAttachmentModels() @ %i on %s (%i)\n", s_EngineTool->HostFrameCount(),
		pThis->GetClientClass()->GetName(), entindex);

	auto retVal = m_UpdateAttachmentModelsHook.GetOriginal()(pThis);
	return retVal;
}

int CameraState::DrawWeaponOverride(C_BaseCombatWeapon* pThis, int flags)
{
	m_DrawWeaponHook.SetState(Hooking::HookAction::SUPERCEDE);
	return 0;
}

void CameraState::CalcViewPlayerOverride(C_TFPlayer* pThis, Vector& origin, QAngle& angles, float& zNear, float& zFar, float& fov)
{
	// If we're here, we're NOT in HLTV.
	if (CalcView(origin, angles, fov))
	{
		s_PlayerObserverModeOffset.GetValue(pThis) = GetLocalObserverMode();

		auto target = GetLocalObserverTarget();
		auto targetEnt = target ? target->GetBaseEntity() : nullptr;
		s_PlayerObserverTargetOffset.GetValue(pThis) = targetEnt;

		UpdateViewmodels();

		if (const auto tick = s_EngineTool->ClientTick(); tick >= m_NextPlayerPosUpdateTick)
		{
			m_NextPlayerPosUpdateTick = tick + UPDATE_POS_TICK_INTERVAL;

			char fullCmdBuf[512];
			fullCmdBuf[0] = '\0';

			char buf[128];
			if (auto player = Player::AsPlayer(target))
			{
				sprintf_s(buf, "spec_player \"#%i\"", player->GetUserID());
				strcat_s(fullCmdBuf, buf);
				engine->ServerCmd(buf);
			}

			static ConVarRef cl_spec_mode("cl_spec_mode");
			if (auto mode = GetLocalObserverMode(); mode != cl_spec_mode.GetInt())
			{
				sprintf_s(buf, "spec_mode \"%i\"", GetLocalObserverMode());
				strcat_s(fullCmdBuf, buf);
				engine->ServerCmd(buf);
			}

			static ConVarRef sv_cheats("sv_cheats");
			if (false && sv_cheats.GetBool())
			{
				sprintf_s(buf, "spec_goto \"%f %f %f %f %f %f\";", origin.x, origin.y, origin.z, angles.x, angles.y, angles.z);
				strcat_s(fullCmdBuf, buf);
			}

			//if (fullCmdBuf[0])
			//	engine->ServerCmd(fullCmdBuf);
		}

		m_CalcViewPlayerHook.SetState(Hooking::HookAction::SUPERCEDE);
	}
}

void CameraState::CalcViewHLTVOverride(C_HLTVCamera* pThis, Vector& origin, QAngle& angles, float& fov)
{
	// If we're here, we ARE in HLTV.
	if (CalcView(origin, angles, fov))
	{
		auto camera = static_cast<HLTVCameraOverride*>(pThis);

		camera->m_vCamOrigin = origin;
		camera->m_aCamAngle = angles;
		camera->m_flFOV = fov;
		camera->m_nCameraMode = (int)GetLocalObserverMode();

		auto target = GetLocalObserverTarget();
		camera->m_iTraget1 = target ? target->entindex() : 0;

		UpdateViewmodels();

		m_CalcViewHLTVHook.SetState(Hooking::HookAction::SUPERCEDE);
	}
}

void CameraState::CreateMoveOverride(CInput* pThis, int sequenceNumber, float inputSampleFrametime, bool active)
{
	m_CreateMoveHook.GetOriginal()(pThis, sequenceNumber, inputSampleFrametime, active);

	auto cmd = pThis->GetUserCmd(sequenceNumber);
	Assert(cmd);
	if (cmd)
		m_RoamingCamera->CreateMove(*cmd);
}

void CameraState::OnTick(bool inGame)
{
	if (!inGame)
		return;

	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);

	// Update spec mode/target
	if (m_DesiredSpecMode != m_LastSpecMode || m_DesiredSpecTarget != m_LastSpecTarget)
	{
		SpecStateChanged(m_DesiredSpecMode, m_DesiredSpecTarget);
		m_LastSpecMode = m_DesiredSpecMode;
		m_LastSpecTarget = m_DesiredSpecTarget;
	}

	if (ce_camerastate_debug.GetBool())
	{
		auto currentMode = GetLocalObserverMode();
		engine->Con_NPrintf(GetConLine(), "Current observer mode: %i (%s)", currentMode, s_ShortObserverModes[currentMode]);

		auto currentTarget = GetLocalObserverTarget();
		engine->Con_NPrintf(GetConLine(), "Current observer target: %i (%s)", currentTarget ? currentTarget->entindex() : 0,
			Player::AsPlayer(currentTarget) ? Player::AsPlayer(currentTarget)->GetName() : "none");

		auto engineMode = GetEngineObserverMode();
		engine->Con_NPrintf(GetConLine(), "Engine observer mode: %i (%s)", engineMode, s_ShortObserverModes[engineMode]);

		auto engineTarget = GetEngineObserverTarget();
		engine->Con_NPrintf(GetConLine(), "Engine observer target: %i (%s)", engineTarget ? engineTarget->entindex() : 0,
			Player::AsPlayer(engineTarget) ? Player::AsPlayer(engineTarget)->GetName() : "none");
	}

	if (ce_camerastate_debug_cameras.GetBool())
	{
		auto camera = m_ActiveCamera.get();
		engine->Con_NPrintf(GetConLine(), "Current camera type: %s", camera ? camera->GetDebugName() : "(null)");
	}
}

void CameraState::LevelInit()
{
	SetupHooks(true);
	m_NextPlayerPosUpdateTick = -1;

	// This only helps on listen servers, but it won't hurt us anywhere else
	if (auto found = cvar->FindVar("sv_force_transmit_ents"))
		found->SetValue(1);
}

void CameraState::LevelShutdown()
{
	SetupHooks(false);
}

void CameraState::SetupHooks(bool connect)
{
	m_CalcViewHLTVHook.SetEnabled(connect);
	m_CalcViewPlayerHook.SetEnabled(connect);
	//m_InToolModeHook.SetEnabled(connect);
	//m_IsThirdPersonCameraHook.SetEnabled(connect);
	//m_SetupEngineViewHook.SetEnabled(connect);

	m_CreateMoveHook.SetEnabled(connect);

	if (connect)
	{
		const auto OverrideCommand = [](const char* cmd, VariablePusher<FnCommandCallback_t>& pusher, FnCommandCallback_t newCallback)
		{
			if (auto found = cvar->FindCommand(cmd))
			{
				auto callback = CCvar::GetDispatchCallback(found);
				if (std::holds_alternative<FnCommandCallback_t*>(callback))
					pusher.Emplace(*std::get<FnCommandCallback_t*>(callback), newCallback);
				else
					Warning("%s: Failed to \"hook\" %s: command dispatch callback type has changed\n", GetModuleName(), found->GetName());
			}
			else
				Warning("%s: Failed to \"hook\" %s: could not find command\n", GetModuleName(), cmd);
		};

		// Add our (more) reliable "hooks"
		OverrideCommand("spec_mode", m_SpecModeDetour, [](const CCommand& cmd) { GetModule()->SpecModeDetour(cmd); });
		OverrideCommand("spec_player", m_SpecPlayerDetour, [](const CCommand& cmd) { GetModule()->SpecPlayerDetour(cmd); });
		OverrideCommand("spec_next", m_SpecNextDetour, [](const CCommand& cmd) { GetModule()->SpecNextDetour(cmd); });
		OverrideCommand("spec_prev", m_SpecPrevDetour, [](const CCommand& cmd) { GetModule()->SpecPrevDetour(cmd); });
	}
	else
	{
		m_SpecModeDetour.Clear();
		m_SpecPlayerDetour.Clear();
		m_SpecNextDetour.Clear();
		m_SpecPrevDetour.Clear();
	}
}

void CameraState::SpecPlayerDetour(const CCommand& cmd)
{
	if (cmd.ArgC() < 2)
		return Warning("Usage: %s <name/#index>\n", cmd[0]);

	int parsed;
	if (TryParseInteger(cmd[1], parsed))
		return SpecEntity(parsed);
	else
	{
		for (auto player : Player::Iterable())
		{
			if (stristr(player->GetName(), cmd[1]))
				return SpecEntity(player->GetEntity());
		}

		Warning("%s: Unable to find player with name containing \"%s\"\n", cmd[0], cmd[1]);
	}
}

void CameraState::SpecEntity(int entindex)
{
	SpecEntity(s_ClientEntityList->GetClientEntity(entindex));
}

void CameraState::SpecEntity(IClientEntity* ent)
{
	CameraStateCallbacks::GetCallbacksParent().SpecTargetChanged(m_DesiredSpecTarget.Get(), ent);
	m_DesiredSpecTarget = ent;
}

void CameraState::SpecNextEntity(bool backwards)
{
	// Based on logic from C_HLTVCamera::SpecNextPlayer
	int start = 1;

	const auto maxClients = s_EngineTool->GetMaxClients();

	if (m_DesiredSpecTarget.GetEntryIndex() > 0 && m_DesiredSpecTarget.GetEntryIndex() <= maxClients)
		start = m_DesiredSpecTarget.GetEntryIndex();

	int index = start;

	while (true)
	{
		// got next/prev player
		if (backwards)
			index--;
		else
			index++;

		// check bounds
		if (index < 1)
			index = maxClients;
		else if (index > maxClients)
			index = 1;

		if (index == start)
			return; // couldn't find a new valid player

		auto player = Player::GetPlayer(index);
		if (!player)
			continue;

		// Must be on a real team
		if (auto team = player->GetTeam(); team != TFTeam::Red && team != TFTeam::Blue)
			continue;

		// Ignore dead players
		if (!player->IsAlive())
			continue;

		SpecEntity(index);
	}
}

void CameraState::SpecStateChanged(ObserverMode mode, IClientEntity* primaryTarget)
{
	if (auto local = Player::GetLocalPlayer(); local && local->GetTeam() != TFTeam::Spectator)
	{
		SetCamera(m_EngineCamera);
		return;
	}

	auto& callbacksParent = CameraStateCallbacks::GetCallbacksParent();

	if (mode == OBS_MODE_ROAMING)
	{
		m_RoamingCamera->m_InputEnabled = false;

		// Create a new roaming camera so we have keep momentum of our old camera, and have no momentum on our new.
		auto newCam = std::make_shared<RoamingCamera>();

		if (primaryTarget)
			newCam->GotoEntity(primaryTarget);

		CameraPtr newCamAdjusted = newCam;
		callbacksParent.SetupCameraTarget(mode, primaryTarget, newCamAdjusted);

		SetCameraSmoothed(newCamAdjusted);
		if (auto roamingCam = std::dynamic_pointer_cast<RoamingCamera>(newCamAdjusted))
			m_RoamingCamera = roamingCam;
		else
			m_RoamingCamera->m_InputEnabled = true;
	}
	else if (mode == OBS_MODE_IN_EYE || mode == OBS_MODE_CHASE)
	{
		if (auto playerCamGroup = dynamic_cast<PlayerCameraGroup*>(m_CurrentCameraGroup.get()))
		{
			if (playerCamGroup->GetPlayerEnt() == primaryTarget)
				return; // Let the camera group handle this itself
		}

		if (auto player = Player::AsPlayer(primaryTarget))
		{
			SetCameraGroupSmoothed(std::make_shared<PlayerCameraGroup>(*Player::AsPlayer(primaryTarget)));
			return;
		}
	}
}

ObserverMode CameraState::ToObserverMode(CameraType type)
{
	switch (type)
	{
		case CameraType::FirstPerson:    return ObserverMode::OBS_MODE_IN_EYE;
		case CameraType::ThirdPerson:    return ObserverMode::OBS_MODE_CHASE;
		case CameraType::Roaming:        return ObserverMode::OBS_MODE_ROAMING;
		case CameraType::Fixed:          return ObserverMode::OBS_MODE_FIXED;

		case CameraType::Smooth:         return ObserverMode::OBS_MODE_FIXED;
	}

	Assert(!"Unknown camera type");
	return ObserverMode::OBS_MODE_NONE;
}

void CameraState::SetCameraInternal(const CameraPtr& camera)
{
	if (camera)
	{
		m_ActiveCamera = camera;
		m_ActiveCamera->ApplySettings();
	}
	else
		m_ActiveCamera = m_EngineCamera;
}

void CameraState::SetCameraSmoothedInternal(CameraPtr camera)
{
	if (camera != m_ActiveCamera)
	{
		camera->ApplySettings();
		CameraStateCallbacks::GetCallbacksParent().SetupCameraSmooth(m_ActiveCamera, camera);
	}

	SetCameraInternal(camera);
}

void CameraState::UpdateFromCameraGroup()
{
	if (!m_CurrentCameraGroup)
		return;

	CameraPtr nextCam = m_LastCameraGroupCamera ? m_LastCameraGroupCamera : m_ActiveCamera;
	m_CurrentCameraGroup->GetBestCamera(nextCam);

	if (nextCam != m_LastCameraGroupCamera)
	{
		SetCameraSmoothedInternal(nextCam);
		m_LastCameraGroupCamera = std::move(nextCam);
	}
}

void CameraState::SpecModeDetour(const CCommand& cmd)
{
	auto newMode = ObserverMode(int(m_DesiredSpecMode) + 1);
	if (cmd.ArgC() > 1)
		newMode = ObserverMode(atoi(cmd[1]));

	switch (newMode)
	{
		default:
		case ObserverMode::OBS_MODE_NONE:
		case ObserverMode::OBS_MODE_DEATHCAM:
		case ObserverMode::OBS_MODE_FREEZECAM:
		case ObserverMode::OBS_MODE_FIXED:
		case ObserverMode::OBS_MODE_IN_EYE:
			newMode = ObserverMode::OBS_MODE_IN_EYE;
			break;

		case ObserverMode::OBS_MODE_CHASE:
			newMode = ObserverMode::OBS_MODE_CHASE;
			break;

		case ObserverMode::OBS_MODE_POI:
		case ObserverMode::OBS_MODE_ROAMING:
			newMode = ObserverMode::OBS_MODE_ROAMING;
			break;
	}

	if (newMode != m_DesiredSpecMode)
		CameraStateCallbacks::GetCallbacksParent().SpecModeChanged(m_DesiredSpecMode, newMode);

	m_DesiredSpecMode = newMode;

	//auto pusher = CreateVariablePusher(m_SwitchReason, ModeSwitchReason::SpecMode);
	//m_SwitchReason = ModeSwitchReason::SpecMode;

	//m_SpecModeDetour.GetOldValue()(cmd);
}
