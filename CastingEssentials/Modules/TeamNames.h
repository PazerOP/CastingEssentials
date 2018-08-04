#pragma once
#include "PluginBase/Modules.h"

#include <convar.h>

class TeamNames final : public Module<TeamNames>
{
public:
	TeamNames();
	static constexpr __forceinline const char* GetModuleName() { return "Team Names"; }

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
	ConVar m_OverrideCvars[TeamConvars::Count];

	ConCommand ce_teamnames_swap;
	void SwapTeamNames();

	std::string m_LastServerValues[(int)TeamConvars::Count];
	std::string m_LastOverrideValues[TeamConvars::Count];

	void OnTick(bool inGame) override;
};