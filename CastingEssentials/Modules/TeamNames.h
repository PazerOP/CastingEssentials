#pragma once
#include "PluginBase/Modules.h"

class ConVar;
class ConCommand;
class IConVar;

class TeamNames final : public Module
{
public:
	TeamNames();

	static TeamNames* GetModule() { return Modules().GetModule<TeamNames>(); }
	static const char* GetModuleName() { return Modules().GetModuleName<TeamNames>().c_str(); }

private:

	struct TeamConvars
	{
		enum Enum
		{
			Blue,
			Red,

			Count,
		};
	};

	ConVar* m_OriginalCvars[TeamConvars::Count];
	ConVar* m_OverrideCvars[TeamConvars::Count];

	ConCommand* ce_teamnames_swap;
	void SwapTeamNames();

	std::string m_LastServerValues[(int)TeamConvars::Count];
	template<TeamConvars::Enum team> static void OriginalChangeCallback(IConVar* var, const char* oldValue, float flOldValue);
	template<TeamConvars::Enum team> static void OverrideChangeCallback(IConVar* var, const char* oldValue, float flOldValue);

	static void GlobalChangeCallback(IConVar* var, const char* oldValue, float flOldValue);
};