#include "Killfeed.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"

#include <igameevents.h>
#include <vprof.h>
#include <client/iclientmode.h>

#include <locale>
#include <codecvt>

Killfeed::Killfeed() :
	ce_killfeed_continuous_update("ce_killfeed_continuous_update", "0", FCVAR_NONE, "Continually updates the killfeed background/icons based on the local player index.")
{
	m_DeathNoticePanel = nullptr;
}

Killfeed::DeathNoticePanelOverride* Killfeed::FindDeathNoticePanel()
{
	auto const clientmode = Interfaces::GetClientMode();
	vgui::Panel* const viewport = clientmode->GetViewport();

	const char* const moduleName = viewport->GetModuleName();

	const auto& children = viewport->GetChildren();
	for (int i = 0; i < children.Count(); i++)
	{
		auto const vChild = children[i];
		auto const child = g_pVGuiPanel->GetPanel(vChild, moduleName);
		if (!child)
			continue;

		auto const childName = child->GetName();
		if (!stricmp("HudDeathNotice", childName))
			return (DeathNoticePanelOverride*)dynamic_cast<CHudBaseDeathNotice*>(child);
	}

	return nullptr;
}

void Killfeed::OnTick(bool inGame)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (!inGame)
		return;

	if (!ce_killfeed_continuous_update.GetBool())
		return;

	if (!m_DeathNoticePanel)
	{
		m_DeathNoticePanel = FindDeathNoticePanel();
		if (!m_DeathNoticePanel)
			return;

		m_DeathNotices = (CUtlVector<DeathNoticeItem>*)((char*)m_DeathNoticePanel + DEATH_NOTICES_OFFSET);
	}

	static_assert(sizeof(DeathNoticeItem) == 408, "sizeof(DeathNoticeItem) doesn't match TF2!");

	auto const localPlayerIndex = GetHooks()->GetRawFunc_Global_GetLocalPlayerIndex()();
	Player* localPlayer = Player::GetPlayer(localPlayerIndex, __FUNCSIG__);
	auto const localPlayerUserID = localPlayer->GetUserID();

	for (int i = 0; i < m_DeathNotices->Count(); i++)
	{
		DeathNoticeItem& current = m_DeathNotices->Element(i);

		current.bLocalPlayerInvolved = false;

		Player* killer = Player::GetPlayerFromUserID(current.iKillerID);
		if (!killer)
			continue;

		const std::string& killerName = killer->GetName();
		if (!killerName.empty())
		{
			auto const killerNameLength = killerName.size();
			std::string killerAndAssisterName = current.Killer.szName;	// "Killer + Assister"
			killerAndAssisterName.erase(0, killerNameLength);

			// Now we should just have " + Assister"
			static constexpr const char plus[] = " + ";
			if (!strncmp(plus, killerAndAssisterName.c_str(), arraysize(plus) - 1))
			{
				// We started with " + ", so remove it
				killerAndAssisterName.erase(0, 3);

				// Now we should just have "Assister", try to find a player with that name
				for (Player* assister : Player::Iterable())
				{
					if (!strcmp(assister->GetName(), killerAndAssisterName.c_str()))
					{
						// Matches our local player
						if (assister->entindex() == localPlayerIndex)
							current.bLocalPlayerInvolved = true;

						break;
					}
				}
			}
		}

		current.bLocalPlayerInvolved =
			current.bLocalPlayerInvolved ||
			current.iKillerID == localPlayerUserID ||
			current.iVictimID == localPlayerUserID;

		auto const GetIcon = GetHooks()->GetRawFunc_CHudBaseDeathNotice_GetIcon();

		const EDeathNoticeIconFormat fmt = current.bLocalPlayerInvolved ? EDeathNoticeIconFormat::kDeathNoticeIcon_Inverted : EDeathNoticeIconFormat::kDeathNoticeIcon_Standard;

		auto const GetUpdatedIcon = [&](CHudTexture* texture)->CHudTexture*
		{
			if (!texture)
				return nullptr;

			bool updated = false;
			char buffer[256];	// 256 is used internally by CHudBaseDeathNotice::GetIcon

			if (fmt == EDeathNoticeIconFormat::kDeathNoticeIcon_Inverted)
			{
				if (!strncmp(texture->szShortName, "d_", 2))
				{
					strcpy(buffer, "dneg_");
					strcpy(buffer + 5, texture->szShortName + 2);
					updated = true;
				}
			}
			else
			{
				if (!strncmp(texture->szShortName, "dneg_", 5))
				{
					strcpy(buffer, "d_");
					strcpy(buffer + 2, texture->szShortName + 5);
					updated = true;
				}
			}

			if (updated)
				return GetIcon(buffer, EDeathNoticeIconFormat::kDeathNoticeIcon_Standard);	// I'm lazy
			else
				return texture;
		};

		current.iconDeath = GetUpdatedIcon(current.iconDeath);
		current.iconCritDeath = GetUpdatedIcon(current.iconCritDeath);
		current.iconPostKillerName = GetUpdatedIcon(current.iconPostKillerName);
		current.iconPostVictimName = GetUpdatedIcon(current.iconPostVictimName);
		current.iconPreKillerName = GetUpdatedIcon(current.iconPreKillerName);

		//GetHooks()->GetFunc<CHudBaseDeathNotice_GetIcon>()()
	}
}