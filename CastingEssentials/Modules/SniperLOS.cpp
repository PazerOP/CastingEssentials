#include "Modules/CameraState.h"
#include "Modules/SniperLOS.h"

#include "PluginBase/Entities.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"

#include <client/c_basecombatweapon.h>
#include <icliententity.h>
#include <shared/util_shared.h>
#include <tier2/beamsegdraw.h>
#include <toolframework/ienginetool.h>

MODULE_REGISTER(SniperLOS);

EntityTypeChecker SniperLOS::s_SniperRifleType;

SniperLOS::SniperLOS() :
	ce_sniperlos_enabled("ce_sniperlos_enabled", "0", FCVAR_NONE, "Enables drawing beams representing sniper line of sight.",
		[](IConVar* var, const char*, float) { GetModule()->ToggleEnabled(static_cast<ConVar*>(var)); }),
	ce_sniperlos_width("ce_sniperlos_width", "5", FCVAR_NONE, "Width of the sniper line of sight beam."),

	ce_sniperlos_color_blu("ce_sniperlos_color_blu", "125 169 197 255", FCVAR_NONE, "RGBA color for the blue sniper line of sight beams.",
		[](IConVar* var, const char*, float) { ColorFromConVar(*static_cast<ConVar*>(var), GetModule()->m_BeamColorBlue); }),
	ce_sniperlos_color_red("ce_sniperlos_color_red", "185 55 55 255", FCVAR_NONE, "RGBA color for the red sniper line of sight beams.",
		[](IConVar* var, const char*, float) { ColorFromConVar(*static_cast<ConVar*>(var), GetModule()->m_BeamColorRed); })
{
	// Initialize with defaults
	ColorFromConVar(ce_sniperlos_color_blu, m_BeamColorBlue);
	ColorFromConVar(ce_sniperlos_color_red, m_BeamColorRed);
}

bool SniperLOS::CheckDependencies()
{
	s_SniperRifleType = Entities::GetTypeChecker("CTFSniperRifle");

	return true;
}

void SniperLOS::ToggleEnabled(const ConVar* var)
{
	if (var->GetBool())
	{
		if (!m_LOSRenderer)
			m_LOSRenderer.emplace();
	}
	else
		m_LOSRenderer.reset();
}

void SniperLOS::LOSRenderer::PostRender()
{
	CMatRenderContextPtr pRenderContext(materials);
	IMaterial* beamMaterial = materials->FindMaterial("castingessentials/sniperlos/beam", TEXTURE_GROUP_OTHER, true);

	const auto curtime = Interfaces::GetEngineTool()->ClientTime();
	const float beamWidth = GetModule()->ce_sniperlos_width.GetFloat();
	const auto& beamColorBlue = GetModule()->m_BeamColorBlue;
	const auto& beamColorRed = GetModule()->m_BeamColorRed;
	const auto specTarget = CameraState::GetModule()->GetLocalObserverTarget(true);

	for (const auto& player : Player::Iterable())
	{
		auto activeWep = player->GetActiveWeapon();
		if (!activeWep || !s_SniperRifleType.Match(activeWep))
			continue;

		if (!player->CheckCondition(TFCond::TFCond_Zoomed))
			continue;

		if (player->GetEntity() == specTarget)
			continue;

		const auto team = player->GetTeam();
		if (team != TFTeam::Blue && team != TFTeam::Red)
			continue;

		const auto eyeAngles = player->GetEyeAngles();
		const auto eyePos = player->GetEyePosition();

		Vector eyeForward;
		AngleVectors(eyeAngles, &eyeForward);

		trace_t tr;
		UTIL_TraceLine(eyePos, eyePos + eyeForward * 10000, MASK_SHOT, player->GetEntity(), COLLISION_GROUP_NONE, &tr);

		CBeamSegDraw beam;
		beam.Start(pRenderContext, 2, beamMaterial);

		BeamSeg_t seg;
		seg.m_flAlpha = 1;
		seg.m_flTexCoord = 0;
		seg.m_flWidth = beamWidth;
		seg.m_vColor = ColorToVector(team == TFTeam::Red ? beamColorRed : beamColorBlue);
		seg.m_vPos = tr.startpos;
		beam.NextSeg(&seg);

		seg.m_vPos = tr.endpos;
		beam.NextSeg(&seg);

		beam.End();
	}
}
