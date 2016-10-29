#pragma once
#include "PluginBase/Modules.h"

struct player_info_s;
class ConVar;
class IConVar;

class PlayerAliases final : public Module
{
public:
	PlayerAliases();
	virtual ~PlayerAliases() = default;

	static bool CheckDependencies();
	static PlayerAliases* GetModule() { return Modules().GetModule<PlayerAliases>(); }
	static const char* GetModuleName() { return Modules().GetModuleName<PlayerAliases>().c_str(); }

private:
	bool GetPlayerInfoOverride(int ent_num, player_info_s* pInfo);

	std::map<CSteamID, std::string> m_CustomAliases;
	int m_GetPlayerInfoHook;

	static void FindAndReplaceInString(std::string& str, const std::string& find, const std::string& replace);
	
	const std::string& GetAlias(const CSteamID& player, const std::string& gameAlias) const;

	ConVar* m_Enabled;
	ConVar* m_FormatBlue;
	ConVar* m_FormatRed;

	static void StaticToggleEnabled(IConVar* var, const char* oldValue, float fOldValue);
	void ToggleEnabled(IConVar* var, const char* oldValue, float fOldValue);
};