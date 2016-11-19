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
	std::string m_LastOverrideValues[TeamConvars::Count];

	void OnTick(bool inGame) override;
};