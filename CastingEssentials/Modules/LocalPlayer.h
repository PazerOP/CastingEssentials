#pragma once
#include "PluginBase/HookManager.h"
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

	Hook<HookFunc::Global_GetLocalPlayerIndex> m_GetLocalPlayerIndexHook;

	ConVar ce_localplayer_enabled;
	ConVar ce_localplayer_player;
	ConCommand ce_localplayer_set_current_target;
	ConVar ce_localplayer_track_spec_target;
	void SetToCurrentTarget();
	void ToggleEnabled(const ConVar *var);
};