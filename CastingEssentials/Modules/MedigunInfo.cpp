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
#include "PluginBase/Entities.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"
#include "PluginBase/TFPlayerResource.h"

#include <convar.h>
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui_controls/EditablePanel.h"
#include <client/iclientmode.h>
#include <KeyValues.h>
#include <client/c_basecombatweapon.h>

#include <vector>

#include <memdbgon.h>

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
	float level;
	TFMedigun medigun;
	bool released;
	TFResistType resistType;
	TFTeam team;
};

MedigunInfo::MedigunInfo()
{
	enabled = new ConVar("ce_mediguninfo_enabled", "0", FCVAR_NONE, "enable medigun info");
	reload_settings = new ConCommand("ce_mediguninfo_reload_settings", []() { GetModule()->ReloadSettings(); }, "reload settings for the medigun info HUD from the resource file", FCVAR_NONE);
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

	if (!Entities::RetrieveClassPropOffset("CWeaponMedigun", { "m_iItemDefinitionIndex" }))
	{
		PluginWarning("Required property m_iItemDefinitionIndex for CWeaponMedigun for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Entities::RetrieveClassPropOffset("CWeaponMedigun", { "m_bChargeRelease" }))
	{
		PluginWarning("Required property m_bChargeRelease for CWeaponMedigun for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Entities::RetrieveClassPropOffset("CWeaponMedigun", { "m_nChargeResistType" }))
	{
		PluginWarning("Required property m_nChargeResistType for CWeaponMedigun for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Entities::RetrieveClassPropOffset("CWeaponMedigun", { "m_flChargeLevel" }))
	{
		PluginWarning("Required property m_flChargeLevel for CWeaponMedigun for module %s not available!\n", GetModuleName());
		ready = false;
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
	}

	return ready;
}

void MedigunInfo::OnTick(bool inGame)
{
	if (inGame && enabled->GetBool())
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
					PluginWarning("[%s] Could not get IClientMode viewport to attach main MedigunInfo panel!\n");
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
	else if (m_MainPanel)
		m_MainPanel.reset();
}

void MedigunInfo::ReloadSettings()
{
	if (!enabled->GetBool())
	{
		Warning("%s must be enabled with %s 1 before using %s.\n", GetModuleName(), enabled->GetName(), reload_settings->GetName());
		return;
	}

	m_MainPanel->LoadControlSettings("Resource/UI/MedigunInfo.res");
}

MedigunInfo::MainPanel::MainPanel(vgui::Panel *parent, const char *panelName) : vgui::EditablePanel(parent, panelName)
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
	vgui::EditablePanel::OnTick();

	size_t bluMediguns = 0;
	size_t redMediguns = 0;

	for (Player* player : Player::Iterable())
	{
		Assert(player);
		if (!player)
		{
			PluginWarning("null player in %s!\n", __FUNCSIG__);
			continue;
		}

		if (!player->IsValid())
		{
			PluginWarning("Invalid player in %s!\n", __FUNCSIG__);
			continue;
		}

		TFClassType cls = player->GetClass();
		if (cls != TFClassType::Medic)
			continue;

		TFTeam team = player->GetTeam();
		if (team != TFTeam::Blue && team != TFTeam::Red)
			continue;

		for (int weaponIndex = 0; weaponIndex < MAX_WEAPONS; weaponIndex++)
		{
			C_BaseCombatWeapon *weapon = player->GetWeapon(weaponIndex);

			if (weapon && Entities::CheckEntityBaseclass(weapon, "WeaponMedigun"))
			{
				MedigunPanel *medigunPanel = nullptr;
				int x, y;

				if (team == TFTeam::Red)
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
				else if (team == TFTeam::Blue)
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

				int itemDefinitionIndex = *Entities::GetEntityProp<int *>(weapon, { "m_iItemDefinitionIndex" });
				TFMedigun type = TFMedigun::Unknown;
				if (itemDefinitionIndex == 29 || itemDefinitionIndex == 211 || itemDefinitionIndex == 663 || itemDefinitionIndex == 796 || itemDefinitionIndex == 805 || itemDefinitionIndex == 885 || itemDefinitionIndex == 894 || itemDefinitionIndex == 903 || itemDefinitionIndex == 912 || itemDefinitionIndex == 961 || itemDefinitionIndex == 970 || itemDefinitionIndex == 15008 || itemDefinitionIndex == 15010 || itemDefinitionIndex == 15025 || itemDefinitionIndex == 15039 || itemDefinitionIndex == 15050)
				{
					type = TFMedigun::MediGun;
				}
				else if (itemDefinitionIndex == 35)
				{
					type = TFMedigun::Kritzkrieg;
				}
				else if (itemDefinitionIndex == 411)
				{
					type = TFMedigun::QuickFix;
				}
				else if (itemDefinitionIndex == 998)
				{
					type = TFMedigun::Vaccinator;
				}

				float level = *Entities::GetEntityProp<float *>(weapon, { "m_flChargeLevel" });

				medigunInfo->SetBool("alive", player->IsAlive());
				medigunInfo->SetInt("charges", type == TFMedigun::Vaccinator ? int(floor(level * 4.0f)) : int(floor(level)));
				medigunInfo->SetFloat("level", level);
				medigunInfo->SetInt("medigun", (int)type);
				medigunInfo->SetBool("released", *Entities::GetEntityProp<bool *>(weapon, { "m_bChargeRelease" }));
				medigunInfo->SetInt("resistType", *Entities::GetEntityProp<int *>(weapon, { "m_nChargeResistType" }));
				medigunInfo->SetInt("team", (int)team);

				PostMessage(medigunPanel, medigunInfo);
				medigunPanel->SetPos(x, y);

				break;
			}
		}
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

	SetDialogVariable("charge", int(floor(level * 100.0f)));
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
	KeyValues *conditions = new KeyValues("conditions");

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