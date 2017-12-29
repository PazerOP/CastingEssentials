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
	ce_loadout_reload_settings = new ConCommand("ce_loadout_reload_settings", []() { GetModule()->ReloadSettings(); }, "Reload settings for the loadout icons panels from the resource file.");

	memset(m_ActiveWeapons, 0, sizeof(m_ActiveWeapons));
	memset(m_Weapons, 0, sizeof(m_Weapons));
}

bool LoadoutIcons::CheckDependencies()
{
	// Development
	return true;
}

void LoadoutIcons::OnTick(bool ingame)
{
	if (ingame && ce_loadout_enabled->GetBool())
	{
		GatherWeapons();
		DrawIcons();
	}
	else
	{
		DeleteIconPanels();
	}
}

void LoadoutIcons::ReloadSettings()
{
	// Just delete all the panels, they'll be re-added automatically next frame
	DeleteIconPanels();
}

void LoadoutIcons::DeleteIconPanels()
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

		for (int loadoutPanelIndex = 0; loadoutPanelIndex < g_pVGuiPanel->GetChildCount(playerPanel); loadoutPanelIndex++)
		{
			vgui::VPANEL loadoutPanel = g_pVGuiPanel->GetChild(playerPanel, loadoutPanelIndex);
			const char* loadoutPanelName = g_pVGuiPanel->GetName(loadoutPanel);
			bool isMatch = false;
			for (int i = 0; i < arraysize(LOADOUT_IMG_NAMES); i++)
			{
				if (!stricmp(LOADOUT_IMG_NAMES[i], loadoutPanelName))
				{
					isMatch = true;
					break;
				}
			}

			if (!isMatch)
				continue;

			vgui::Panel* realLoadoutPanel = g_pVGuiPanel->GetPanel(loadoutPanel, "CastingEssentials");
			realLoadoutPanel->DeletePanel();
			loadoutPanelIndex--;
		}
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
		m_ActiveWeapons[playerIndex] = activeWeapon ? ItemSchema::GetModule()->GetBaseItemID(GetWeaponDefinitionIndex(activeWeapon->Get())) : -1;

		for (int weaponIndex = 0; weaponIndex < 3; weaponIndex++)
		{
			C_BaseCombatWeapon* weapon = player->GetWeapon(weaponIndex);
			if (!weapon)
				continue;

			m_Weapons[playerIndex][weaponIndex] = ItemSchema::GetModule()->GetBaseItemID(GetWeaponDefinitionIndex(weapon));
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

		const int playerIndex = atoi(&playerPanelName[11]);	// Skip over the "playerpanel" part

		vgui::EditablePanel* player = assert_cast<vgui::EditablePanel*>(g_pVGuiPanel->GetPanel(playerPanel, "ClientDLL"));
		if (!PlayerPanelHasIcons(player))
			PlayerPanelInitIcons(player);

		PlayerPanelUpdateIcons(player);
	}
}

bool LoadoutIcons::PlayerPanelHasIcons(vgui::EditablePanel* player)
{
	for (int i = 0; i < arraysize(LOADOUT_IMG_NAMES); i++)
	{
		if (!player->FindChildByName(LOADOUT_IMG_NAMES[i]))
			return false;
	}

	return true;
}

void LoadoutIcons::PlayerPanelInitIcons(vgui::EditablePanel* player)
{
	std::unique_ptr<vgui::EditablePanel> tempPanel(new vgui::EditablePanel(player, "LoadoutIconsTemp"));
	tempPanel->LoadControlSettings("Resource/UI/LoadoutIcons.res");

	for (int i = 0; i < tempPanel->GetChildCount(); i++)
	{
		vgui::Panel* childPanel = tempPanel->GetChild(i);
		const char* childPanelName = childPanel->GetName();

		bool shouldAdd = false;
		// Only add ones that are in our array
		for (int n = 0; n < arraysize(LOADOUT_IMG_NAMES); n++)
		{
			if (!stricmp(LOADOUT_IMG_NAMES[n], childPanelName))
			{
				shouldAdd = true;
				break;
			}
		}

		if (!shouldAdd)
			continue;

		if (player->FindChildByName(childPanelName))
			continue;	// Skip panels that are already there

		// Move the child panel into the player panel
		childPanel->SetParent(player);
		i--;
	}
}

void LoadoutIcons::PlayerPanelUpdateIcons(vgui::EditablePanel* player)
{
	vgui::ImagePanel* item1Img = dynamic_cast<vgui::ImagePanel*>(player->FindChildByName(LOADOUT_IMG_NAMES[0]));
	Assert(item1Img);
	if (!item1Img)
		return;

	const int playerIndex = GetPlayerIndex(player);
	if (playerIndex < 0 || playerIndex >= MAX_PLAYERS)
	{
		item1Img->SetVisible(false);
		return;
	}

	const int weapon = m_ActiveWeapons[playerIndex];

	if (weapon < 0)
	{
		item1Img->SetVisible(false);
	}
	else
	{
		char materialBuffer[32];
		sprintf_s(materialBuffer, "loadout_icons/%i", weapon);

		item1Img->SetImage(materialBuffer);
		item1Img->SetVisible(true);
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
