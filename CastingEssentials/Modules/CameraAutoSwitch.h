#pragma once

#include <igameevents.h>
#include "PluginBase/Modules.h"

#include <memory>

class CCommand;
class ConCommand;
class ConVar;
class IConVar;

class CameraAutoSwitch final : public Module<CameraAutoSwitch>, IGameEventListener2
{
public:
	CameraAutoSwitch();
	virtual ~CameraAutoSwitch();

	static bool CheckDependencies();

private:
	ConVar *enabled;
	ConVar *m_SwitchToKiller;
	ConVar *killer_delay;
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
