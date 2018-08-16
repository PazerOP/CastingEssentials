/*
*  mediguninfo.cpp
*  StatusSpec project
*
*  Copyright (c) 2014-2015 Forward Command Post
*  BSD 2-Clause License
*  http://opensource.org/licenses/BSD-2-Clause
*
*/

#include "MedigunInfo.h"
#include "Modules/HUDHacking.h"
#include "PluginBase/Entities.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"
#include "PluginBase/TFPlayerResource.h"

#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include <vgui/ILocalize.h>
#include "vgui_controls/EditablePanel.h"
#include "vgui_controls/ImagePanel.h"
#include <client/iclientmode.h>
#include <KeyValues.h>
#include <client/c_basecombatweapon.h>
#include <vprof.h>

#include <vector>

#include <memdbgon.h>

MODULE_REGISTER(MedigunInfo);

EntityOffset<bool> MedigunInfo::s_ChargeRelease;
EntityOffset<TFResistType> MedigunInfo::s_ChargeResistType;
EntityOffset<float> MedigunInfo::s_ChargeLevel;

class MedigunInfo::MainPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE(MainPanel, vgui::EditablePanel);

public:
	MainPanel(vgui::Panel *parent, const char *panelName);
	virtual ~MainPanel() { }

	virtual void ApplySettings(KeyValues *inResourceData);
	virtual void LoadControlSettings(const char *dialogResourceName, const char *pathID = NULL, KeyValues *pPreloadedKeyValues = NULL, KeyValues *pConditions = NULL);
	virtual void OnTick();

private:
	int bluBaseX;
	int bluBaseY;
	int bluOffsetX;
	int bluOffsetY;
	int redBaseX;
	int redBaseY;
	int redOffsetX;
	int redOffsetY;

	std::vector<MedigunPanel *> bluMedigunPanels;
	std::vector<MedigunPanel *> redMedigunPanels;
};

class MedigunInfo::MedigunPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE(MedigunPanel, vgui::EditablePanel);

public:
	MedigunPanel(vgui::Panel *parent, const char *panelName) : vgui::EditablePanel(parent, panelName) { }
	virtual ~MedigunPanel() { }

private:
	MESSAGE_FUNC_PARAMS(OnMedigunInfoUpdate, "MedigunInfo", attributes);
	MESSAGE_FUNC_PARAMS(OnReloadControlSettings, "ReloadControlSettings", data);

	bool alive;
	int charges;
	int charge;
	float level;
	TFMedigun medigun;
	bool released;
	TFResistType resistType;
	TFTeam team;
};

MedigunInfo::MedigunInfo() :
	ce_mediguninfo_separate_enabled("ce_mediguninfo_separate_enabled", "0", FCVAR_NONE, "Enable separated medigun panels."),
	ce_mediguninfo_separate_reload("ce_mediguninfo_separate_reload", []() { GetModule()->ReloadSettings(); },
		"Reload settings for the separated medigun panels from the .res file.")
{
}

bool MedigunInfo::CheckDependencies()
{
	bool ready = true;

	if (!g_pVGuiSchemeManager)
	{
		PluginWarning("Required interface vgui::ISchemeManager for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!g_pVGuiSurface)
	{
		PluginWarning("Required interface vgui::ISurface for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Interfaces::AreVguiLibrariesAvailable())
	{
		PluginWarning("Required VGUI library for module %s not available!\n", GetModuleName());
		ready = false;
	}

	{
		const auto medigunClass = Entities::GetClientClass("CWeaponMedigun");

		s_ChargeRelease = Entities::GetEntityProp<bool>(medigunClass, "m_bChargeRelease");
		s_ChargeResistType = Entities::GetEntityProp<TFResistType>(medigunClass, "m_nChargeResistType");
		s_ChargeLevel = Entities::GetEntityProp<float>(medigunClass, "m_flChargeLevel");
	}

	if (!Player::CheckDependencies())
	{
		Warning("Required player helper class for module %s not available!\n", GetModuleName());
		ready = false;
	}

	try
	{
		Interfaces::GetClientMode();
	}
	catch (bad_pointer)
	{
		PluginWarning("Module %s requires IClientMode, which cannot be verified at this time!\n", GetModuleName());
		ready = false;
	}

	return ready;
}

void MedigunInfo::LevelShutdown()
{
	m_MainPanel.reset();
}

void MedigunInfo::CollectMedigunData()
{
	m_MedigunPanelData.clear();

	for (Player* player : Player::Iterable())
	{
		if (player->GetClass() != TFClassType::Medic)
			continue;

		Data& medigunData = m_MedigunPanelData.insert(std::make_pair(player->entindex() - 1, Data())).first->second;
		medigunData.m_Alive = player->IsAlive();
		medigunData.m_Team = player->GetTeam();

		if (medigunData.m_Alive)
		{
			C_BaseCombatWeapon* medigun = player->GetMedigun(&medigunData.m_Type);
			if (!medigun || medigunData.m_Type == TFMedigun::Unknown)
			{
				PluginWarning("Medic %s has no medigun or their medigun was not recognized!\n", player->GetName());
				continue;
			}

			medigunData.m_Charge = s_ChargeLevel.GetValue(medigun);
			medigunData.m_Popped = s_ChargeRelease.GetValue(medigun);

			if (medigunData.m_Type == TFMedigun::Vaccinator)
				medigunData.m_ResistType = s_ChargeResistType.GetValue(medigun);
		}
	}
}

void MedigunInfo::OnTick(bool inGame)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);

	const bool separateEnabled = ce_mediguninfo_separate_enabled.GetBool();

	if (inGame && separateEnabled)
	{
		CollectMedigunData();

		if (separateEnabled)
		{
			if (!m_MainPanel)
			{
				try
				{
					vgui::Panel *viewport = Interfaces::GetClientMode()->GetViewport();

					if (viewport)
						m_MainPanel.reset(new MainPanel(viewport, "MedigunInfo"));
					else
					{
						PluginWarning("Could not get IClientMode viewport to attach main MedigunInfo panel!\n");
						m_MainPanel.reset();
					}
				}
				catch (bad_pointer)
				{
					PluginWarning("Could not initialize main MedigunInfo panel: unable to get IClientMode!\n");
					m_MainPanel.reset();
				}
			}

			if (m_MainPanel)
			{
				if (!m_MainPanel->IsEnabled())
					m_MainPanel->SetEnabled(true);
			}
		}
		else
			m_MainPanel.reset();
	}
	else if (m_MainPanel)
		m_MainPanel.reset();
}

void MedigunInfo::ReloadSettings()
{
	if (!ce_mediguninfo_separate_enabled.GetBool())
	{
		Warning("%s must be enabled with %s 1 before using %s.\n", GetModuleName(), ce_mediguninfo_separate_enabled.GetName(), ce_mediguninfo_separate_reload.GetName());
		return;
	}

	if (m_MainPanel)
		m_MainPanel->LoadControlSettings("Resource/UI/MedigunInfo.res");
	else
	{
		PluginWarning("Failed to LoadControlSettings for separated medigun panel! m_MainPanel was null\n");
	}
}

MedigunInfo::MainPanel::MainPanel(vgui::Panel *parent, const char *panelName) : BaseClass(parent, panelName)
{
	LoadControlSettings("Resource/UI/MedigunInfo.res");
	g_pVGui->AddTickSignal(GetVPanel());
}

void MedigunInfo::MainPanel::ApplySettings(KeyValues *inResourceData)
{
	vgui::EditablePanel::ApplySettings(inResourceData);

	int alignScreenWide, alignScreenTall;
	g_pVGuiSurface->GetScreenSize(alignScreenWide, alignScreenTall);

	int wide, tall;
	GetSize(wide, tall);

	GetPos(redBaseX, redBaseY);
	ComputePos(inResourceData->GetString("red_base_x"), redBaseX, wide, alignScreenWide, true);
	ComputePos(inResourceData->GetString("red_base_y"), redBaseY, tall, alignScreenTall, false);

	GetPos(bluBaseX, bluBaseY);
	ComputePos(inResourceData->GetString("blu_base_x"), bluBaseX, wide, alignScreenWide, true);
	ComputePos(inResourceData->GetString("blu_base_y"), bluBaseY, tall, alignScreenTall, false);

	redOffsetX = g_pVGuiSchemeManager->GetProportionalScaledValue(inResourceData->GetInt("red_offset_x"));
	redOffsetY = g_pVGuiSchemeManager->GetProportionalScaledValue(inResourceData->GetInt("red_offset_y"));
	bluOffsetX = g_pVGuiSchemeManager->GetProportionalScaledValue(inResourceData->GetInt("blu_offset_x"));
	bluOffsetY = g_pVGuiSchemeManager->GetProportionalScaledValue(inResourceData->GetInt("blu_offset_y"));
}

void MedigunInfo::MainPanel::LoadControlSettings(const char *dialogResourceName, const char *pathID, KeyValues *pPreloadedKeyValues, KeyValues *pConditions)
{
	vgui::EditablePanel::LoadControlSettings(dialogResourceName, pathID, pPreloadedKeyValues, pConditions);

	PostActionSignal(new KeyValues("ReloadControlSettings"));
}

void MedigunInfo::MainPanel::OnTick()
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	vgui::EditablePanel::OnTick();

	size_t bluMediguns = 0;
	size_t redMediguns = 0;

	for (const auto& pair : GetModule()->m_MedigunPanelData)
	{
		const auto& data = pair.second;

		MedigunPanel* medigunPanel;
		int x, y;

		if (data.m_Team == TFTeam::Red)
		{
			redMediguns++;

			if (redMediguns > redMedigunPanels.size())
			{
				medigunPanel = new MedigunPanel(this, "MedigunPanel");
				AddActionSignalTarget(medigunPanel);
				redMedigunPanels.push_back(medigunPanel);
			}
			else
			{
				medigunPanel = redMedigunPanels.at(redMediguns - 1);
			}

			x = redBaseX + (redOffsetX * (redMediguns - 1));
			y = redBaseY + (redOffsetY * (redMediguns - 1));
		}
		else if (data.m_Team == TFTeam::Blue)
		{
			bluMediguns++;

			if (bluMediguns > bluMedigunPanels.size())
			{
				medigunPanel = new MedigunPanel(this, "MedigunPanel");
				AddActionSignalTarget(medigunPanel);
				bluMedigunPanels.push_back(medigunPanel);
			}
			else
			{
				medigunPanel = bluMedigunPanels.at(bluMediguns - 1);
			}

			x = bluBaseX + (bluOffsetX * (bluMediguns - 1));
			y = bluBaseY + (bluOffsetY * (bluMediguns - 1));
		}
		else
			continue;	// We will never get here, but VS2015 won't shut up about potentially uninititialized local variables x and y

		KeyValues *medigunInfo = new KeyValues("MedigunInfo");

		medigunInfo->SetBool("alive", data.m_Alive);
		medigunInfo->SetInt("charges", data.m_Type == TFMedigun::Vaccinator ? int(floor(data.m_Charge * 4.0f)) : int(floor(data.m_Charge)));
		medigunInfo->SetFloat("level", data.m_Charge);
		medigunInfo->SetInt("medigun", (int)data.m_Type);
		medigunInfo->SetBool("released", data.m_Popped);
		medigunInfo->SetInt("resistType", (int)data.m_ResistType);
		medigunInfo->SetInt("team", (int)data.m_Team);

		PostMessage(medigunPanel, medigunInfo);
		medigunPanel->SetPos(x, y);
	}

	while (redMediguns < redMedigunPanels.size())
	{
		delete redMedigunPanels.back();
		redMedigunPanels.pop_back();
	}

	while (bluMediguns < bluMedigunPanels.size())
	{
		delete bluMedigunPanels.back();
		bluMedigunPanels.pop_back();
	}
}

void MedigunInfo::MedigunPanel::OnMedigunInfoUpdate(KeyValues *attributes)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	bool reloadSettings = (alive != attributes->GetBool("alive") || charges != attributes->GetInt("charges") || medigun != (TFMedigun)attributes->GetInt("medigun") || released != attributes->GetBool("released") || resistType != (TFResistType)attributes->GetInt("resistType") || team != (TFTeam)attributes->GetInt("team"));

	alive = attributes->GetBool("alive");
	charges = attributes->GetInt("charges");
	level = attributes->GetFloat("level");
	medigun = (TFMedigun)attributes->GetInt("medigun");
	released = attributes->GetBool("released");
	resistType = (TFResistType)attributes->GetInt("resistType");
	team = (TFTeam)attributes->GetInt("team");

	if (reloadSettings)
		OnReloadControlSettings(nullptr);

	// TODO: set up a custom message that doesn't spam a bunch of DialogVariables messages, forcing the panel to redraw 5 or 6 times.

	const int newCharge = int(floor(level * 100.0f));
	if (charge != newCharge || newCharge == 0)
		SetDialogVariable("charge", charge = newCharge);

	if (medigun == TFMedigun::Vaccinator)
	{
		SetDialogVariable("charges", int(floor(level * 4.0f)));
		SetDialogVariable("charge1", int(floor(level * 400.0f) - 0.0f) < 0 ? 0 : int(floor(level * 400.0f) - 0.0f));
		SetDialogVariable("charge2", int(floor(level * 400.0f) - 100.0f) < 0 ? 0 : int(floor(level * 400.0f) - 100.0f));
		SetDialogVariable("charge3", int(floor(level * 400.0f) - 200.0f) < 0 ? 0 : int(floor(level * 400.0f) - 200.0f));
		SetDialogVariable("charge4", int(floor(level * 400.0f) - 300.0f) < 0 ? 0 : int(floor(level * 400.0f) - 300.0f));
	}
}

void MedigunInfo::MedigunPanel::OnReloadControlSettings(KeyValues *attributes)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	KeyValues::AutoDelete conditions("conditions");

	if (alive)
		conditions->SetBool("player-alive", true);
	else
		conditions->SetBool("player-dead", true);

	// Set charges
	{
		char buffer[16];
		sprintf_s(buffer, "charges-%i", charges);
		conditions->SetBool(buffer, true);
	}

	if (medigun == TFMedigun::MediGun)
		conditions->SetBool("medigun-medigun", true);
	else if (medigun == TFMedigun::Kritzkrieg)
		conditions->SetBool("medigun-kritzkrieg", true);
	else if (medigun == TFMedigun::QuickFix)
		conditions->SetBool("medigun-quickfix", true);
	else if (medigun == TFMedigun::Vaccinator)
		conditions->SetBool("medigun-vaccinator", true);

	// Uber popped?
	if (released)
		conditions->SetBool("status-released", true);
	else
		conditions->SetBool("status-building", true);

	// Resist type?
	if (resistType == TFResistType::Bullet)
		conditions->SetBool("resist-bullet", true);
	else if (resistType == TFResistType::Explosive)
		conditions->SetBool("resist-explosive", true);
	else if (resistType == TFResistType::Fire)
		conditions->SetBool("resist-fire", true);

	if (team == TFTeam::Red)
		conditions->SetBool("team-red", true);
	else if (team == TFTeam::Blue)
		conditions->SetBool("team-blu", true);

	LoadControlSettings("Resource/UI/MedigunPanel.res", nullptr, nullptr, conditions);
}

