#include "LoadoutIcons.h"
#include "ItemSchema.h"
#include "PluginBase/Entities.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"

#include <client/c_basecombatweapon.h>
#include <client/iclientmode.h>
#include <vgui/IVGui.h>
#include "vgui_controls/EditablePanel.h"
#include "vgui_controls/ImagePanel.h"

LoadoutIcons::LoadoutIcons()
{
	ce_loadout_enabled = new ConVar("ce_loadout_enabled", "0", FCVAR_NONE, "Enable weapon icons inside player panels in the specgui.");
	ce_loadout_filter_active_red = new ConVar("ce_loadout_filter_active_red", "255 255 255 255", FCVAR_NONE, "drawcolor_override for red team's active loadout items.");
	ce_loadout_filter_active_blu = new ConVar("ce_loadout_filter_active_blu", "255 255 255 255", FCVAR_NONE, "drawcolor_override for blu team's active loadout items.");
	ce_loadout_filter_inactive_red = new ConVar("ce_loadout_filter_inactive_red", "255 255 255 255", FCVAR_NONE, "drawcolor_override for red team's inactive loadout items.");
	ce_loadout_filter_inactive_blu = new ConVar("ce_loadout_filter_inactive_blu", "255 255 255 255", FCVAR_NONE, "drawcolor_override for blu team's inactive loadout items.");

	memset(m_Weapons, 0, sizeof(m_Weapons));
	memset(m_ActiveWeaponIndices, 0, sizeof(m_ActiveWeaponIndices));
}

bool LoadoutIcons::CheckDependencies()
{
	bool ready = true;

	if (!g_pVGuiPanel)
	{
		PluginWarning("Required interface vgui::IPanel for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Interfaces::GetClientMode())
	{
		PluginWarning("Required interface IClientMode for module %s not available!\n", GetModuleName());
		ready = false;
	}

	try
	{
		HookManager::GetRawFunc_EditablePanel_GetDialogVariables();
		HookManager::GetRawFunc_ImagePanel_SetImage();
	}
	catch (bad_pointer ex)
	{
		PluginWarning("No signature match found for %s, required for module %s!\n", ex.what(), GetModuleName());
		ready = false;
	}

	return ready;
}

void LoadoutIcons::OnTick(bool ingame)
{
	if (ingame && ce_loadout_enabled->GetBool())
	{
		GatherWeapons();
		DrawIcons();
	}
}

int LoadoutIcons::GetWeaponDefinitionIndex(IClientNetworkable* networkable)
{
	if (!networkable)
		return -1;

	auto definitionIndex = Entities::GetEntityProp<int*>(networkable, "m_iItemDefinitionIndex");
	Assert(definitionIndex);
	if (!definitionIndex)
		return -1;

	return *definitionIndex;
}

vgui::VPANEL LoadoutIcons::GetSpecGUI()
{
	auto clientMode = Interfaces::GetClientMode();
	if (!clientMode)
		return 0;

	auto viewport = clientMode->GetViewport();
	if (!viewport)
		return 0;

	auto viewportPanel = viewport->GetVPanel();
	const auto viewportChildCount = g_pVGuiPanel->GetChildCount(viewportPanel);
	for (int specguiIndex = 0; specguiIndex < viewportChildCount; specguiIndex++)
	{
		vgui::VPANEL specguiPanel = g_pVGuiPanel->GetChild(viewportPanel, specguiIndex);
		if (!strcmp(g_pVGuiPanel->GetName(specguiPanel), "specgui"))
			return specguiPanel;
	}

	return 0;
}

void LoadoutIcons::GatherWeapons()
{
	for (Player* player : Player::Iterable())
	{
		const auto playerIndex = player->entindex() - 1;

		auto activeWeapon = Entities::GetEntityProp<EHANDLE*>(player->GetEntity(), "m_hActiveWeapon");
		m_Weapons[playerIndex][IDX_ACTIVE] = activeWeapon ? ItemSchema::GetModule()->GetBaseItemID(GetWeaponDefinitionIndex(activeWeapon->Get())) : -1;

		for (int weaponIndex = 0; weaponIndex < 5; weaponIndex++)
		{
			C_BaseCombatWeapon* weapon = player->GetWeapon(weaponIndex);
			if (!weapon)
				continue;

			auto currentID = ItemSchema::GetModule()->GetBaseItemID(GetWeaponDefinitionIndex(weapon));
			if (currentID == m_Weapons[playerIndex][IDX_ACTIVE])
				m_ActiveWeaponIndices[playerIndex] = weaponIndex;

			m_Weapons[playerIndex][weaponIndex] = currentID;
		}
	}
}

void LoadoutIcons::DrawIcons()
{
	vgui::VPANEL specguiPanel = GetSpecGUI();
	if (!specguiPanel)
		return;

	const auto specguiChildCount = g_pVGuiPanel->GetChildCount(specguiPanel);
	for (int playerPanelIndex = 0; playerPanelIndex < specguiChildCount; playerPanelIndex++)
	{
		vgui::VPANEL playerPanel = g_pVGuiPanel->GetChild(specguiPanel, playerPanelIndex);
		const char* playerPanelName = g_pVGuiPanel->GetName(playerPanel);
		if (strncmp(playerPanelName, "playerpanel", 11))	// Names are like "playerpanel13"
			continue;

		vgui::EditablePanel* player = assert_cast<vgui::EditablePanel*>(g_pVGuiPanel->GetPanel(playerPanel, "ClientDLL"));

		PlayerPanelUpdateIcons(player);
	}
}

void LoadoutIcons::PlayerPanelUpdateIcons(vgui::EditablePanel* playerPanel)
{
	const int playerIndex = GetPlayerIndex(playerPanel);

	// Force get the panel even though we're in a different module
	const auto playerVPANEL = playerPanel->GetVPanel();
	for (int i = 0; i < g_pVGuiPanel->GetChildCount(playerVPANEL); i++)
	{
		auto childVPANEL = g_pVGuiPanel->GetChild(playerVPANEL, i);
		auto childPanelName = g_pVGuiPanel->GetName(childVPANEL);

		auto iconPanel = g_pVGuiPanel->GetPanel(childVPANEL, "ClientDLL");

		for (int team = 0; team < arraysize(TEAM_NAMES); team++)
		{
			for (int iconIndex = 0; iconIndex < arraysize(LOADOUT_ICONS); iconIndex++)
			{
				char buffer[32];
				sprintf_s(buffer, "%s%s", LOADOUT_ICONS[iconIndex], TEAM_NAMES[team]);

				if (stricmp(childPanelName, buffer))
					continue;

				const auto weaponIndex = m_Weapons[playerIndex][iconIndex];

				if (playerIndex < 0 || playerIndex >= MAX_PLAYERS || weaponIndex < 0)
				{
					iconPanel->SetVisible(false);
				}
				else
				{
					char materialBuffer[32];
					sprintf_s(materialBuffer, "loadout_icons/%i", weaponIndex);

					// Dumb, evil, unsafe hacks
					auto hackImgPanel = reinterpret_cast<vgui::ImagePanel*>(iconPanel);

					HookManager::GetRawFunc_ImagePanel_SetImage()(hackImgPanel, materialBuffer);

					Color* m_FillColor = (Color*)(((DWORD*)hackImgPanel) + 94);
					Color* m_DrawColor = (Color*)(((DWORD*)hackImgPanel) + 95);

					if (iconIndex == IDX_ACTIVE || m_ActiveWeaponIndices[playerIndex] == iconIndex)
					{
						*m_DrawColor = ConVarGetColor(team == TEAM_RED ? *ce_loadout_filter_active_red : *ce_loadout_filter_active_blu);
					}
					else
					{
						*m_DrawColor = ConVarGetColor(team == TEAM_RED ? *ce_loadout_filter_inactive_red : *ce_loadout_filter_inactive_blu);
					}

					iconPanel->SetVisible(true);
				}

				break;
			}
		}
	}
}

int LoadoutIcons::GetPlayerIndex(vgui::EditablePanel* playerPanel)
{
	if (!playerPanel)
		return -1;

	auto dv = HookManager::GetRawFunc_EditablePanel_GetDialogVariables()(playerPanel);

	const char* playername = dv->GetString("playername");

	// Find the player
	for (Player* player : Player::Iterable())
	{
		if (!strcmp(player->GetName().c_str(), playername))
			return player->entindex() - 1;
	}

	return -1;
}
