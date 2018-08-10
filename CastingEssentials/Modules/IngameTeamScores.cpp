#include "IngameTeamScores.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/TFDefinitions.h"
#include "PluginBase/TFTeamResource.h"

#include <client/iclientmode.h>
#include <toolframework/ienginetool.h>
#include <vgui/IVGui.h>
#include <vgui_controls/EditablePanel.h>
#include <vprof.h>

MODULE_REGISTER(IngameTeamScores);

class IngameTeamScores::ScorePanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE(ScorePanel, vgui::EditablePanel);

public:
	ScorePanel(vgui::Panel* parent, const char* panelName);
	virtual ~ScorePanel() { }

	void OnTick() override;
};

IngameTeamScores::IngameTeamScores() :
	ce_teamscores_enabled("ce_teamscores_enabled", "0", FCVAR_NONE, "Enable ingame team score display."),
	ce_teamscores_reload("ce_teamscores_reload", []() { GetModule()->ReloadSettings(); }, "Reload settings for the team scores panel from the .res file."),

	ce_teamscores_delta_blu("ce_teamscores_delta_blu", "0", FCVAR_NONE, "Delta for blu team score."),
	ce_teamscores_delta_red("ce_teamscores_delta_red", "0", FCVAR_NONE, "Delta for red team score.")
{
	for (int i = 0; i < CUSTOM_TEXT_COUNT; i++)
	{
		sprintf_s(ce_teamscores_text_names[i], "ce_teamscores_text%i", i + 1);

		// Help string is expected to be static data
		sprintf_s(ce_teamscores_text_helpstrings[i], "Text to display in the %%customtext%i%% dialog variable.", i + 1);

		ce_teamscores_text[i] = std::make_unique<ConVar>(ce_teamscores_text_names[i], "", FCVAR_NONE, ce_teamscores_text_helpstrings[i]);
	}
}

void IngameTeamScores::ReloadSettings()
{
	if (!ce_teamscores_enabled.GetBool())
	{
		Warning("%s must be enabled with %s 1 before using %s.\n", GetModuleName(), ce_teamscores_enabled.GetName(), ce_teamscores_reload.GetName());
		return;
	}

	if (m_Panel)
		m_Panel->LoadControlSettings("Resource/UI/TeamScorePanel.res");
	else
	{
		PluginWarning("Failed to LoadControlSettings for ingame team scores panel! m_Panel was null\n");
	}
}

void IngameTeamScores::OnTick(bool inGame)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (inGame && ce_teamscores_enabled.GetBool())
	{
		if (!m_Panel)
		{
			try
			{
				vgui::Panel *viewport = Interfaces::GetClientMode()->GetViewport();

				if (viewport)
					m_Panel.reset(new ScorePanel(viewport, "TeamScorePanel"));
				else
				{
					PluginWarning("Could not get IClientMode viewport to attach TeamScorePanel!\n");
					m_Panel.reset();
				}
			}
			catch (bad_pointer)
			{
				PluginWarning("Could not initialize TeamScorePanel: unable to get IClientMode!\n");
				m_Panel.reset();
			}
		}
	}
	else if (m_Panel)
		m_Panel.reset();
}

IngameTeamScores::ScorePanel::ScorePanel(vgui::Panel* parent, const char* panelName) : BaseClass(parent, panelName)
{
	LoadControlSettings("Resource/UI/TeamScorePanel.res");

	// 1 tick per second should be perfectly fine for this panel
	g_pVGui->AddTickSignal(GetVPanel(), 1000);
}

void IngameTeamScores::ScorePanel::OnTick()
{
	BaseClass::OnTick();

	auto teams = TFTeamResource::GetTeamResource();
	if (!teams)
		return;

	int redScore = teams->GetTeamScore(TFTeam::Red) + GetModule()->ce_teamscores_delta_red.GetInt();
	int blueScore = teams->GetTeamScore(TFTeam::Blue) + GetModule()->ce_teamscores_delta_blu.GetInt();

	SetDialogVariable("redteamscore", redScore);
	SetDialogVariable("blueteamscore", blueScore);

	for (int i = 0; i < CUSTOM_TEXT_COUNT; i++)
	{
		char dialogVarName[32];
		sprintf_s(dialogVarName, "customtext%i", i);

		SetDialogVariable(dialogVarName, GetModule()->ce_teamscores_text[i]->GetString());
	}

	static ConVarRef mp_tournament_blueteamname("mp_tournament_blueteamname");
	static ConVarRef mp_tournament_redteamname("mp_tournament_redteamname");

	SetDialogVariable("blueteamname", mp_tournament_blueteamname.GetString());
	SetDialogVariable("redteamname", mp_tournament_redteamname.GetString());
}