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

template<TeamNames::TeamConvars::Enum team> void TeamNames::OriginalChangeCallback(IConVar* var, const char* oldValue, float flOldValue)
{
	TeamNames* const m = GetModule();
	m->m_LastServerValues[team] = m->m_OriginalCvars[team]->GetString();

	if (!IsStringEmpty(m->m_OverrideCvars[team]->GetString()))
	{
		var->SetValue(m->m_OverrideCvars[team]->GetString());
		return;
	}
}

template<TeamNames::TeamConvars::Enum team> void TeamNames::OverrideChangeCallback(IConVar* var, const char* oldValue, float flOldValue)
{
	TeamNames* const m = GetModule();

	if (IsStringEmpty(m->m_OverrideCvars[team]->GetString()))
		m->m_OriginalCvars[team]->SetValue(m->m_LastServerValues[team].c_str());
	else
		m->m_OriginalCvars[team]->SetValue(m->m_OverrideCvars[team]->GetString());
}

void TeamNames::GlobalChangeCallback(IConVar* var, const char* oldValue, float flOldValue)
{
	TeamNames* const m = GetModule();

	if (var == m->m_OriginalCvars[TeamConvars::Blue])
		OriginalChangeCallback<TeamConvars::Blue>(var, oldValue, flOldValue);
	else if (var == m->m_OriginalCvars[TeamConvars::Red])
		OriginalChangeCallback<TeamConvars::Red>(var, oldValue, flOldValue);
}

TeamNames::TeamNames()
{
	m_OriginalCvars[(int)TeamConvars::Blue] = g_pCVar->FindVar("mp_tournament_blueteamname");
	m_OriginalCvars[(int)TeamConvars::Red] = g_pCVar->FindVar("mp_tournament_redteamname");
	
	g_pCVar->InstallGlobalChangeCallback(&TeamNames::GlobalChangeCallback);

	m_OverrideCvars[(int)TeamConvars::Blue] = new ConVar("ce_teamnames_blu", "", FCVAR_NONE, "Overrides mp_tournament_blueteamname.", &TeamNames::OverrideChangeCallback<TeamConvars::Blue>);
	m_OverrideCvars[(int)TeamConvars::Red] = new ConVar("ce_teamnames_red", "", FCVAR_NONE, "Overrides mp_tournament_redteamname.", &TeamNames::OverrideChangeCallback<TeamConvars::Red>);

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