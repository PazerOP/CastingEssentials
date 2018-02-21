#pragma once

#include "PluginBase/Modules.h"

#include <convar.h>
#include <igameevents.h>

class CameraAutoSwitch final : public Module<CameraAutoSwitch>, IGameEventListener2
{
public:
	CameraAutoSwitch();
	virtual ~CameraAutoSwitch();

	static bool CheckDependencies();

private:
	ConVar ce_cameraautoswitch_enabled;
	ConVar ce_cameraautoswitch_killer;
	ConVar ce_cameraautoswitch_killer_delay;
	void ToggleKillerEnabled(IConVar *var, const char *pOldValue, float flOldValue);

	void QueueSwitchToPlayer(int player, int fromPlayer, float delay);
	bool m_AutoSwitchQueued;
	int m_AutoSwitchFromPlayer;
	int m_AutoSwitchToPlayer;
	float m_AutoSwitchTime;

	void FireGameEvent(IGameEvent *event) override;
	void OnPlayerDeath(IGameEvent* event);

	void OnTick(bool inGame) override;
};
