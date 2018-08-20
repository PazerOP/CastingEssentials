#pragma once

#include "PluginBase/Hook.h"
#include "PluginBase/Modules.h"

#include <convar.h>
#include <igameevents.h>
#include <shareddefs.h>

#include <vector>

class C_BaseCombatCharacter;
class CAccountPanel;
class CDamageAccountPanel;
class C_TFPlayer;
class IHandleEntity;
using trace_t = class CGameTrace;

class HitEvents final : public Module<HitEvents>
{
public:
	HitEvents();

	static bool CheckDependencies() { return true; }
	static constexpr __forceinline const char* GetModuleName() { return "Player Hit Events"; }

protected:
	void LevelInit() override;
	void LevelShutdown() override;

private:
	std::vector<IGameEvent*> m_EventsToIgnore;

	void UpdateEnabledState();

	void FireGameEventOverride(CDamageAccountPanel* pThis, IGameEvent* event);

	bool m_OverrideUTILTraceline;
	void UTILTracelineOverride(const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask, const IHandleEntity* ignore, int collisionGroup, trace_t* ptr);

	bool DamageAccountPanelShouldDrawOverride(CDamageAccountPanel* pThis);

	Hook<HookFunc::CDamageAccountPanel_FireGameEvent> m_FireGameEventHook;
	Hook<HookFunc::Global_UTIL_TraceLine> m_UTILTracelineHook;
	Hook<HookFunc::CDamageAccountPanel_ShouldDraw> m_DamageAccountPanelShouldDrawHook;

	CAccountPanel* m_LastDamageAccount;

	ConVar ce_hitevents_enabled;
	ConVar ce_hitevents_dmgnumbers_los;
};