#pragma once

#include "PluginBase/Modules.h"

#include <convar.h>

class IngameTeamScores final : public Module<IngameTeamScores>
{
public:
	IngameTeamScores();
	static constexpr __forceinline const char* GetModuleName() { return "Ingame Team Scores"; }

private:
	void ReloadSettings();

	class ScorePanel;
	std::unique_ptr<ScorePanel> m_Panel;

	void OnTick(bool inGame) override;

	ConVar ce_teamscores_enabled;
	ConCommand ce_teamscores_reload;

	ConVar ce_teamscores_delta_blu;
	ConVar ce_teamscores_delta_red;

	static constexpr int CUSTOM_TEXT_COUNT = 4;

	std::unique_ptr<ConVar> ce_teamscores_text[CUSTOM_TEXT_COUNT];
	char ce_teamscores_text_names[CUSTOM_TEXT_COUNT][32];
	char ce_teamscores_text_helpstrings[CUSTOM_TEXT_COUNT][64];
};