#pragma once

#include "PluginBase/Modules.h"

#include <convar.h>

#include <memory>

class LocalPlayer : public Module<LocalPlayer>
{
public:
	LocalPlayer();

	static bool CheckDependencies();

private:
	void OnTick(bool inGame) override;

	int GetLocalPlayerIndexOverride();

	int m_GetLocalPlayerIndexHookID;

	ConVar ce_localplayer_enabled;
	ConVar ce_localplayer_player;
	ConCommand ce_localplayer_set_current_target;
	ConVar ce_localplayer_track_spec_target;
	void SetToCurrentTarget();
	void ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue);
};