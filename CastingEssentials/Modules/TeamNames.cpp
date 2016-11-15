#include "TeamNames.h"

#include <convar.h>

// Hi we're TOTALLY not hijacking a friend class that's eventually only declared in a cpp file
class CCvar
{
public:
	static Hooking::Internal::MemberFnPtr<ConVar, void, const char*> GetInternalSetValueFn() { return &ConVar::InternalSetValue; }
	static void SetParent(ConVar& cVar, ConVar* newParent) { cVar.m_pParent = newParent; }

	static const char* GetLocalName(ConCommandBase* ccmd) { return ccmd->m_pszName; }

	CCvar() = delete;
	CCvar(const CCvar&) = delete;
};

class StandinConVar final : public ConVar
{
public:
	StandinConVar() = delete;
	StandinConVar(const StandinConVar&) = delete;
	StandinConVar operator=(const StandinConVar&) = delete;

	const char* GetName() const override { return CCvar::GetLocalName(m_OriginalParent); }

private:
	ConVar* m_OriginalParent;
};

TeamNames::TeamNames()
{
	m_OriginalCvars[(int)TeamConvars::Blue] = g_pCVar->FindVar("mp_tournament_bluteamname");
	m_OriginalCvars[(int)TeamConvars::Red] = g_pCVar->FindVar("mp_tournament_redteamname");

	m_BlueTeamNameHooker.reset(new VirtualHook<TeamConvars::Blue>(m_OriginalCvars[(int)TeamConvars::Blue], CCvar::GetInternalSetValueFn()));
	m_BlueTeamNameHooker->AddHook(std::bind(&TeamNames::SetValueDetour<TeamConvars::Blue>, this, std::placeholders::_1));

	m_OverrideCvars[(int)TeamConvars::Blue] = new ConVar("ce_teamnames_blu", "", FCVAR_NONE, "Overrides mp_tournament_bluteamname.");
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

void TeamNames::SetValueDetour(Hooking::IGroupHook* hook, ConVar* originalCvar, ConVar* overrideCvar, const char* newValue)
{
	if (!strcmp(newValue, overrideCvar->GetString()))
	{
		hook->SetState(Hooking::HookAction::SUPERCEDE);
		return;
	}
}