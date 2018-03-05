#pragma once

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

class HitEvents final : public Module<HitEvents>, IGameEventListener2
{
public:
	HitEvents();

	void OnTick(bool bInGame) override;

	static bool CheckDependencies() { return true; }

protected:
	void FireGameEvent(IGameEvent* event) override;

private:
	std::vector<IGameEvent*> m_EventsToIgnore;

	void EnableToggle();

	void DisplayDamageFeedbackOverride(CDamageAccountPanel* pThis, C_TFPlayer* pAttacker, C_BaseCombatCharacter* pVictim, int iDamageAmount, int iHealth, bool unknown);

	bool m_OverrideUTILTraceline;
	void UTILTracelineOverride(const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask, const IHandleEntity* ignore, int collisionGroup, trace_t* ptr);

	void* OnAccountValueChangedOverride(CAccountPanel* pThis, int unknown, int healthDelta, int deltaType);

	bool m_OverrideGetVectorInScreenSpace;
	bool GetVectorInScreenSpaceOverride(Vector pos, int& x, int& y, Vector* vecOffset);

	void AccountPanelPaintOverride(CAccountPanel* pThis);

	bool DamageAccountPanelShouldDrawOverride(CDamageAccountPanel* pThis);

	int m_DisplayDamageFeedbackHook;
	int m_UTILTracelineHook;
	int m_OnAccountValueChangedHook;
	int m_GetVectorInScreenSpaceHook;
	int m_AccountPanelPaintHook;
	int m_DamageAccountPanelShouldDrawHook;

	CAccountPanel* m_LastDamageAccount;

	ConVar ce_hitevents_enabled;
	ConVar ce_hitevents_debug;

	IGameEvent* TriggerPlayerHurt(int playerEntIndex, int damage);
};