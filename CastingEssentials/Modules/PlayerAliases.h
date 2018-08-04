#pragma once
#include "PluginBase/HookManager.h"
#include "PluginBase/Modules.h"

#include <convar.h>

struct player_info_s;

class PlayerAliases final : public Module<PlayerAliases>
{
public:
	PlayerAliases();
	virtual ~PlayerAliases() = default;

	static bool CheckDependencies();
	static constexpr __forceinline const char* GetModuleName() { return "Player Aliases"; }

private:
	bool GetPlayerInfoOverride(int ent_num, player_info_s* pInfo);

	std::map<CSteamID, std::string> m_CustomAliases;
	Hook<HookFunc::IVEngineClient_GetPlayerInfo> m_GetPlayerInfoHook;

	static void FindAndReplaceInString(std::string& str, const std::string& find, const std::string& replace);

	const std::string& GetAlias(const CSteamID& player, const std::string& gameAlias) const;

	ConVar ce_playeraliases_enabled;
	ConVar ce_playeraliases_format_blu;
	ConVar ce_playeraliases_format_red;

	ConCommand ce_playeraliases_format_swap;
	void SwapTeamFormats();

	ConCommand ce_playeraliases_list;
	void PrintPlayerAliases();

	ConCommand ce_playeraliases_add;
	void AddPlayerAlias(const CCommand& command);
	ConCommand ce_playeraliases_remove;
	void RemovePlayerAlias(const CCommand& command);

	void ToggleEnabled(const ConVar* var);
};