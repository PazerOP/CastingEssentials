#include "IngameTeamScores.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/TFDefinitions.h"
#include "PluginBase/TFTeamResource.h"

#include <client/iclientmode.h>
#include <vgui/IVGui.h>
#include <vgui_controls/EditablePanel.h>

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
	ce_teamscores_reload("ce_teamscores_reload", []() { GetModule()->ReloadSettings(); }, "Reload settings for the team scores panel from the .res file.")
{
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
	g_pVGui->AddTickSignal(GetVPanel());
}

void IngameTeamScores::ScorePanel::OnTick()
{
	BaseClass::OnTick();

	auto teams = TFTeamResource::GetTeamResource();
	if (!teams)
		return;

	int redScore = teams->GetTeamScore(TFTeam::Red);
	int blueScore = teams->GetTeamScore(TFTeam::Blue);

	SetDialogVariable("redteamscore", redScore);
	SetDialogVariable("blueteamscore", blueScore);

	static ConVarRef mp_tournament_blueteamname("mp_tournament_blueteamname");
	static ConVarRef mp_tournament_redteamname("mp_tournament_redteamname");

	SetDialogVariable("blueteamname", mp_tournament_blueteamname.GetString());
	SetDialogVariable("redteamname", mp_tournament_redteamname.GetString());
}
