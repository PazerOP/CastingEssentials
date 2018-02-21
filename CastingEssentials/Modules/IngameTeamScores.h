#pragma once

#include "PluginBase/Modules.h"

#include <convar.h>

class IngameTeamScores final : public Module<IngameTeamScores>
{
public:
	IngameTeamScores();

private:
	void ReloadSettings();

	class ScorePanel;
	std::unique_ptr<ScorePanel> m_Panel;

	void OnTick(bool inGame) override;

	ConVar ce_teamscores_enabled;
	ConCommand ce_teamscores_reload;
};