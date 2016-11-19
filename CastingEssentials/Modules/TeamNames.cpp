#include "TeamNames.h"

#include <convar.h>

// Hi we're TOTALLY not hijacking a friend class that's eventually only declared in a cpp file
class CCvar
{
public:
	static FnChangeCallback_t GetChangeCallback(ConVar& cVar) { return cVar.m_pParent->m_fnChangeCallback; }
	static void SetChangeCallback(ConVar& cVar, FnChangeCallback_t callback) { cVar.m_pParent->m_fnChangeCallback = callback; }

	CCvar() = delete;
	CCvar(const CCvar&) = delete;
};

TeamNames::TeamNames()
{
	m_OriginalCvars[(int)TeamConvars::Blue] = g_pCVar->FindVar("mp_tournament_blueteamname");
	m_OriginalCvars[(int)TeamConvars::Red] = g_pCVar->FindVar("mp_tournament_redteamname");

	m_OverrideCvars[(int)TeamConvars::Blue] = new ConVar("ce_teamnames_blu", "", FCVAR_NONE, "Overrides mp_tournament_blueteamname.");
	m_OverrideCvars[(int)TeamConvars::Red] = new ConVar("ce_teamnames_red", "", FCVAR_NONE, "Overrides mp_tournament_redteamname.");

	ce_teamnames_swap = new ConCommand("ce_teamnames_swap", [](const CCommand& args) { GetModule()->SwapTeamNames(); }, "Swaps the values of ce_teamnames_blu and ce_teamnames_red.");
}

void TeamNames::SwapTeamNames()
{
	Assert(m_OverrideCvars[(int)TeamConvars::Blue]);
	Assert(m_OverrideCvars[(int)TeamConvars::Red]);

	if (!m_OverrideCvars[(int)TeamConvars::Blue] || !m_OverrideCvars[(int)TeamConvars::Red])
		return;

	SwapConVars(*m_OverrideCvars[(int)TeamConvars::Red], *m_OverrideCvars[(int)TeamConvars::Blue]);
}

void TeamNames::OnTick(bool inGame)
{
	for (TeamConvars::Enum t = (TeamConvars::Enum)0; t < TeamConvars::Count; (*((int*)&t))++)
	{
		const bool originalChanged = !!m_LastServerValues[t].compare(m_OriginalCvars[t]->GetString()) && !!m_LastOverrideValues[t].compare(m_OriginalCvars[t]->GetString());
		if (originalChanged)
			m_LastServerValues[t] = m_OriginalCvars[t]->GetString();

		const bool newOverride = !!m_LastOverrideValues[t].compare(m_OverrideCvars[t]->GetString());
		if (newOverride)
			m_LastOverrideValues[t] = m_OverrideCvars[t]->GetString();

		if (IsStringEmpty(m_OverrideCvars[t]->GetString()))
			m_OriginalCvars[t]->SetValue(m_LastServerValues[t].c_str());
		else
			m_OriginalCvars[t]->SetValue(m_OverrideCvars[t]->GetString());
	}
}