#pragma once
#include "Hooking/GroupVirtualHook.h"
#include "PluginBase/Modules.h"

class ConVar;
class ConCommand;

class TeamNames final : public Module
{
public:
	TeamNames();

	static TeamNames* GetModule() { return Modules().GetModule<TeamNames>(); }
	static const char* GetModuleName() { return Modules().GetModuleName<TeamNames>().c_str(); }

private:
	enum class TeamConvars
	{
		Blue,
		Red,

		Count,
	};

	ConVar* m_OriginalCvars[(int)TeamConvars::Count];
	ConVar* m_OverrideCvars[(int)TeamConvars::Count];

	ConCommand* ce_teamnames_swap;
	void SwapTeamNames();

	void ChangeDetour(Hooking::IGroupHook* hook, ConVar* originalCvar, ConVar* overrideCvar, const char* newValue);
	template<TeamConvars team> void ChangeDetour(const char* newValue) 
	{
		Hooking::IGroupHook* hook = nullptr;
		if (team == TeamConvars::Red)
		{
			hook = m_RedTeamNameHooker.get();
		}
		else if (team == TeamConvars::Blue)
		{
			hook = m_BlueTeamNameHooker.get();
		}
		else
			Error("[CastingEssentials] Out of range TeamConvars value %i", team);

		ChangeDetour(hook, m_OriginalCvars[team], m_OverrideCvars[team], newValue);
	}

	template<TeamConvars team> using VirtualHook = Hooking::GroupVirtualHook<TeamConvars, team, false, ConVar, void, const char*>;

	std::unique_ptr<VirtualHook<TeamConvars::Blue>> m_BlueTeamNameHooker;
	std::unique_ptr<VirtualHook<TeamConvars::Red>> m_RedTeamNameHooker;
	
	int m_Hooks[(int)TeamConvars::Count];

	std::string m_LastServerValues[(int)TeamConvars::Count];
};