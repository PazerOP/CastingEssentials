#include "LocalPlayer.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "Controls/StubPanel.h"

#include <client/c_baseentity.h>
#include <client/c_baseplayer.h>
#include <client/c_baseanimating.h>
#include <cdll_int.h>
#include <convar.h>
#include <debugoverlay_shared.h>
#include <vprof.h>
#include <toolframework/ienginetool.h>
#include <model_types.h>

#include <functional>

class LocalPlayer::TickPanel final : public virtual vgui::StubPanel
{
public:
	TickPanel(std::function<void()> setFunction)
	{
		m_SetToCurrentTarget = setFunction;
	}

	void OnTick() override;
private:
	std::function<void()> m_SetToCurrentTarget;
};



LocalPlayer::LocalPlayer()
{
	m_GetLocalPlayerIndexHookID = 0;

	enabled = new ConVar("ce_localplayer_enabled", "0", FCVAR_NONE, "enable local player override", [](IConVar *var, const char *pOldValue, float flOldValue) { GetModule()->ToggleEnabled(var, pOldValue, flOldValue); });
	player = new ConVar("ce_localplayer_player", "0", FCVAR_NONE, "player index to set as the local player");
	set_current_target = new ConCommand("ce_localplayer_set_current_target", []() { GetModule()->SetToCurrentTarget(); }, "set the local player to the current spectator target", FCVAR_NONE);
	track_spec_target = new ConVar("ce_localplayer_track_spec_target", "0", FCVAR_NONE, "have the local player value track the spectator target", [](IConVar *var, const char *pOldValue, float flOldValue) { GetModule()->ToggleTrackSpecTarget(var, pOldValue, flOldValue); });
}

bool LocalPlayer::CheckDependencies()
{
	bool ready = true;

	if (!Interfaces::GetClientDLL())
	{
		PluginWarning("Required interface IBaseClientDLL for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Interfaces::GetEngineClient())
	{
		PluginWarning("Required interface IVEngineClient for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Player::IsConditionsRetrievalAvailable())
	{
		PluginWarning("Required player condition retrieval for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Interfaces::GetHLTVCamera())
	{
		PluginWarning("Required interface C_HLTVCamera for module %s not available!\n", GetModuleName());
		ready = false;
	}

	try
	{
		GetHooks()->GetFunc<Global_GetLocalPlayerIndex>();
	}
	catch (bad_pointer)
	{
		PluginWarning("Required function GetLocalPlayerIndex for module %s not available!\n", GetModuleName());
		ready = false;
	}

	return ready;
}

void LocalPlayer::ToggleTrackSpecTarget(IConVar *var, const char *pOldValue, float flOldValue)
{
	if (track_spec_target->GetBool())
	{
		if (!m_Panel)
			m_Panel.reset(new TickPanel(std::bind(&LocalPlayer::SetToCurrentTarget, this)));
	}
	else
	{
		if (m_Panel)
			m_Panel.reset();
	}
}

int LocalPlayer::GetLocalPlayerIndexOverride()
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (enabled->GetBool() && Player::IsValidIndex(player->GetInt()))
	{
		Player* localPlayer = Player::GetPlayer(player->GetInt(), __FUNCSIG__);
		if (localPlayer)
		{
			GetHooks()->GetHook<Global_GetLocalPlayerIndex>()->SetState(Hooking::HookAction::SUPERCEDE);
			return player->GetInt();
		}
	}

	GetHooks()->GetHook<Global_GetLocalPlayerIndex>()->SetState(Hooking::HookAction::IGNORE);
	return 0;
}

void LocalPlayer::ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue)
{
	if (enabled->GetBool())
	{
		if (!m_GetLocalPlayerIndexHookID)
		{
			m_GetLocalPlayerIndexHookID = GetHooks()->GetHook<Global_GetLocalPlayerIndex>()->AddHook(std::bind(&LocalPlayer::GetLocalPlayerIndexOverride, this));
		}
	}
	else
	{
		if (m_GetLocalPlayerIndexHookID && GetHooks()->GetHook<Global_GetLocalPlayerIndex>()->RemoveHook(m_GetLocalPlayerIndexHookID, __FUNCSIG__))
			m_GetLocalPlayerIndexHookID = 0;

		Assert(!m_GetLocalPlayerIndexHookID);
	}
}

void LocalPlayer::SetToCurrentTarget()
{
	Player* localPlayer = Player::GetPlayer(Interfaces::GetEngineClient()->GetLocalPlayer(), __FUNCSIG__);

	if (localPlayer)
	{
		if (localPlayer->GetObserverMode() == OBS_MODE_FIXED ||
			localPlayer->GetObserverMode() == OBS_MODE_IN_EYE ||
			localPlayer->GetObserverMode() == OBS_MODE_CHASE)
		{
			Player* targetPlayer = Player::AsPlayer(localPlayer->GetObserverTarget());

			if (targetPlayer)
			{
				player->SetValue(targetPlayer->GetEntity()->entindex());
				return;
			}
		}
	}

	player->SetValue(Interfaces::GetEngineClient()->GetLocalPlayer());
}

#include "PluginBase/Entities.h"

static int TFPlayerDrawModelOverride(C_TFPlayer* pPlayer, int flags)
{
	auto hook = GetHooks()->GetHook<C_TFPlayer_DrawModel>();
	hook->SetState(Hooking::HookAction::IGNORE);

	extern bool g_RenderingGlowModel;
	if (g_RenderingGlowModel)
		return 0;

	if (!pPlayer)
		return 0;

	// C_TFPlayer uses multiple inheritance in 2017 TF2. We don't have the 2017 class declaration
	// available, so we just have to guess the offset.
	IClientUnknown* unknown = reinterpret_cast<IClientUnknown*>((size_t*)pPlayer - 1);
	if (!unknown)
		return 0;

	auto entity = unknown->GetIClientEntity();
	if (!entity)
		return 0;

	const int entindex = entity->entindex();
	if (entindex != 10)
		return 0;
	//if (entindex < 1 || entindex > Interfaces::GetEngineClient()->GetMaxClients())
	//	return 0;

	auto networkable = unknown->GetClientNetworkable();
	if (!networkable)
		return 0;

	auto animating = dynamic_cast<C_BaseAnimating*>(unknown->GetBaseEntity());
	if (!animating)
		return 0;

	if (!Entities::CheckEntityBaseclass(networkable, "TFPlayer"))
		return 0;

	Player* player = Player::GetPlayer(entindex, __FUNCSIG__);
	if (!player)
		return 0;

	static int s_PlayerDrawCounts[MAX_PLAYERS];
	static int s_LastFrameIndex = -1;
	const auto frameindex = Interfaces::GetEngineTool()->HostFrameCount();
	if (frameindex != s_LastFrameIndex)
	{
		memset(s_PlayerDrawCounts, 0, sizeof(s_PlayerDrawCounts));
		s_LastFrameIndex = frameindex;
	}

	const auto drawIndex = s_PlayerDrawCounts[entindex]++;

	hook->SetState(Hooking::HookAction::SUPERCEDE);

	int result = hook->GetOriginal()(pPlayer, flags);

	char buffer[256];
	sprintf_s(buffer,
		"Flags: %i, Result: %i, Model: %p, ShouldDraw: %s",
		flags, result, animating->GetModel(),
		animating->ShouldDraw() ? "true" : "false");

	NDebugOverlay::Text(player->GetEyePosition() + Vector(0, 0, 10) * drawIndex, buffer, false, NDEBUG_PERSIST_TILL_NEXT_SERVER);

	if (result == 0)
		PluginMsg("Skipped player draw\n");

	return result;
}

static int s_ExpectedDrawModelCount = 0;
static int s_ActualDrawModelCount = 0;
static int BaseAnimatingDrawModelOverride(C_BaseAnimating* animating, int flags)
{
	auto hook = GetHooks()->GetHook<C_BaseAnimating_DrawModel>();
	hook->SetState(Hooking::HookAction::IGNORE);

	extern bool g_RenderingGlowModel;
	if (g_RenderingGlowModel)
		return 0;

	if (!animating)
		return 0;

	auto unknown = animating->GetIClientUnknown();
	if (!unknown)
		return 0;

	auto entity = unknown->GetIClientEntity();
	if (!entity)
		return 0;

	const int entindex = entity->entindex();
	if (entindex != 10)
		return 0;
	//if (entindex < 1 || entindex > Interfaces::GetEngineClient()->GetMaxClients())
	//	return 0;

	auto networkable = unknown->GetClientNetworkable();
	if (!networkable)
		return 0;

	if (!Entities::CheckEntityBaseclass(networkable, "TFPlayer"))
		return 0;

	Player* player = Player::GetPlayer(entindex, __FUNCSIG__);
	if (!player)
		return 0;

	s_ExpectedDrawModelCount++;

	static int s_PlayerDrawCounts[MAX_PLAYERS];
	static int s_LastFrameIndex = -1;
	const auto frameindex = Interfaces::GetEngineTool()->HostFrameCount();
	if (frameindex != s_LastFrameIndex)
	{
		memset(s_PlayerDrawCounts, 0, sizeof(s_PlayerDrawCounts));
		s_LastFrameIndex = frameindex;
	}

	const auto drawIndex = s_PlayerDrawCounts[entindex]++;

	hook->SetState(Hooking::HookAction::SUPERCEDE);

	int result = hook->GetOriginal()(animating, flags);

	char buffer[256];
	sprintf_s(buffer,
		"Flags: %i, Result: %i, Model: %p, ShouldDraw: %s",
		flags, result, animating->GetModel(),
		animating->ShouldDraw() ? "true" : "false");

	//NDebugOverlay::Text(player->GetEyePosition() + Vector(0, 0, 10) * drawIndex, buffer, false, NDEBUG_PERSIST_TILL_NEXT_SERVER);

	if (result == 0)
		PluginMsg("Skipped player draw\n");

	return result;
}

static int DrawModelOverride(IClientRenderable* renderable, int flags)
{
	auto hook = GetHooks()->GetHook<IClientRenderable_DrawModel>();
	hook->SetState(Hooking::HookAction::IGNORE);

	extern bool g_RenderingGlowModel;
	if (g_RenderingGlowModel)
		return 0;

	if (!renderable)
		return 0;

	auto unknown = renderable->GetIClientUnknown();
	if (!unknown)
		return 0;

	auto entity = unknown->GetIClientEntity();
	if (!entity)
		return 0;

	const int entindex = entity->entindex();
	if (entindex != 10)
		return 0;
	//if (entindex < 1 || entindex > Interfaces::GetEngineClient()->GetMaxClients())
	//	return 0;

	auto networkable = unknown->GetClientNetworkable();
	if (!networkable)
		return 0;

	if (!Entities::CheckEntityBaseclass(networkable, "TFPlayer"))
		return 0;

	Player* player = Player::GetPlayer(entindex, __FUNCSIG__);
	if (!player)
		return 0;

	static int s_PlayerDrawCounts[MAX_PLAYERS];
	static int s_LastFrameIndex = -1;
	const auto frameindex = Interfaces::GetEngineTool()->HostFrameCount();
	if (frameindex != s_LastFrameIndex)
	{
		memset(s_PlayerDrawCounts, 0, sizeof(s_PlayerDrawCounts));
		s_LastFrameIndex = frameindex;
	}

	const auto drawIndex = s_PlayerDrawCounts[entindex]++;

	hook->SetState(Hooking::HookAction::SUPERCEDE);

	int result = hook->GetOriginal()(renderable, flags);

	char buffer[256];
	sprintf_s(buffer,
		"Flags: %i, Result: %i, Model: %p, ShouldDraw: %s",
		flags, result, renderable->GetModel(),
		renderable->ShouldDraw() ? "true" : "false");

	//NDebugOverlay::Text(player->GetEyePosition() + Vector(0, 0, 10) * drawIndex, buffer, false, NDEBUG_PERSIST_TILL_NEXT_SERVER);

	return result;
}

static void DrawOpaqueRenderableOverride(IClientRenderable* pEnt, bool bTwoPass, ERenderDepthMode DepthMode, int nDefaultFlags)
{
	auto hook = GetHooks()->GetHook<Global_DrawOpaqueRenderable>();
	hook->SetState(Hooking::HookAction::SUPERCEDE);
	hook->GetOriginal()(pEnt, bTwoPass, DepthMode, nDefaultFlags);
}

static void DrawTranslucentRenderableOverride(IClientRenderable* pEnt, bool bTwoPass, bool bShadowDepth, bool bIgnoreDepth)
{
	auto hook = GetHooks()->GetHook<Global_DrawTranslucentRenderable>();
	hook->SetState(Hooking::HookAction::SUPERCEDE);
	hook->GetOriginal()(pEnt, bTwoPass, bShadowDepth, bIgnoreDepth);
}

static int C_BaseAnimating_InternalDrawModelOverride(C_BaseAnimating* pThis, int flags)
{
	auto hook = GetHooks()->GetHook<C_BaseAnimating_InternalDrawModel>();
	hook->SetState(Hooking::HookAction::SUPERCEDE);
	int result = hook->GetOriginal()(pThis, flags);

	extern bool g_RenderingGlowModel;
	auto unknown = pThis->GetIClientUnknown();
	auto entity = unknown ? unknown->GetIClientEntity() : nullptr;
	const int entindex = entity ? entity->entindex() : -1;
	auto networkable = unknown->GetClientNetworkable();

	return result;
}

void LocalPlayer::TickPanel::OnTick()
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (Interfaces::GetEngineClient()->IsInGame())
		m_SetToCurrentTarget();

	//static std::map<int, std::shared_ptr<IClientRenderable_DrawModel>> s_DrawModelHooks;
	//static std::unique_ptr<IClientRenderable_DrawModel> s_DrawModelHook;
	static bool s_AddedHooks = false;
	if (!s_AddedHooks)
	{
		for (Player* player : Player::Iterable())
		{
			if (!player)
				continue;

			char buffer[256];
#if 0
			sprintf_s(buffer, "Local player: %s", player == Player::GetLocalPlayer() ? "true" : "false");
			NDebugOverlay::Text(player->GetAbsOrigin(), buffer, false, NDEBUG_PERSIST_TILL_NEXT_SERVER);

			sprintf_s(buffer, "ShouldDraw(): %s", player->GetBaseEntity()->ShouldDraw() ? "true" : "false");
			NDebugOverlay::Text(player->GetAbsOrigin() + Vector(0, 0, 10), buffer, false, NDEBUG_PERSIST_TILL_NEXT_SERVER);
#endif

			auto animating = player->GetBaseAnimating();

			auto hook = GetHooks()->GetHook<IClientRenderable_DrawModel>();
			//s_DrawModelHook.reset(new IClientRenderable_DrawModel());

			hook->AttachHook(std::make_shared<IClientRenderable_DrawModel::Inner>((IClientRenderable*)animating, &IClientRenderable::DrawModel));

			hook->AddHook(std::bind(&DrawModelOverride, std::placeholders::_1, std::placeholders::_2));

			GetHooks()->AddHook<C_BaseAnimating_DrawModel>(std::bind(&BaseAnimatingDrawModelOverride, std::placeholders::_1, std::placeholders::_2));
			GetHooks()->AddHook<C_BaseAnimating_InternalDrawModel>(std::bind(&C_BaseAnimating_InternalDrawModelOverride, std::placeholders::_1, std::placeholders::_2));
			GetHooks()->AddHook<C_TFPlayer_DrawModel>(std::bind(&TFPlayerDrawModelOverride, std::placeholders::_1, std::placeholders::_2));
			GetHooks()->AddHook<Global_DrawOpaqueRenderable>(std::bind(&DrawOpaqueRenderableOverride, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
			GetHooks()->AddHook<Global_DrawTranslucentRenderable>(std::bind(&DrawTranslucentRenderableOverride, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
			//GetHooks()->GetHook<C_TFPlayer_DrawModel>()->InitHook();

			s_AddedHooks = true;
			break;

			//sprintf_s(buffer, "Nodraw? %s", (animating->GetEffects() & EF_NODRAW) ? "true" : "false");
			//NDebugOverlay::Text(player->GetEyePosition() + Vector(0, 0, 10), buffer, false, NDEBUG_PERSIST_TILL_NEXT_SERVER);
		}
	}

	engine->Con_NPrintf(0, "ShouldDrawLocalPlayer: %s", HookManager::GetRawFunc_C_BasePlayer_ShouldDrawLocalPlayer() ? "true" : "false");
}