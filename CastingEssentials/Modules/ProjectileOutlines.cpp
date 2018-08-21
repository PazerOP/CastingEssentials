#include "ProjectileOutlines.h"
#include "PluginBase/Entities.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/TFDefinitions.h"

#include <client/c_baseentity.h>
#include <toolframework/ienginetool.h>
#include <vprof.h>

// smh windows
#undef IGNORE

MODULE_REGISTER(ProjectileOutlines);


EntityOffset<bool> ProjectileOutlines::s_GlowDisabledOffset;
EntityOffset<int> ProjectileOutlines::s_GlowModeOffset;
EntityOffset<EHANDLE> ProjectileOutlines::s_GlowTargetOffset;
EntityOffset<Color> ProjectileOutlines::s_GlowColorOffset;

EntityOffset<TFGrenadePipebombType> ProjectileOutlines::s_PipeTypeOffset;
const ClientClass* ProjectileOutlines::s_RocketType;

ProjectileOutlines::ProjectileOutlines() :
	ce_projectileoutlines_rockets("ce_projectileoutlines_rockets", "0", FCVAR_NONE, "Enable projectile outlines for rockets.",
		[](IConVar*, const char*, float) { GetModule()->UpdateEnabled(); }),
	ce_projectileoutlines_pills("ce_projectileoutlines_pills", "0", FCVAR_NONE, "Enable projectile outlines for pills.",
		[](IConVar*, const char*, float) { GetModule()->UpdateEnabled(); }),
	ce_projectileoutlines_stickies("ce_projectileoutlines_stickies", "0", FCVAR_NONE, "Enable projectile outlines for stickies.",
		[](IConVar*, const char*, float) { GetModule()->UpdateEnabled(); }),

	ce_projectileoutlines_color_blu("ce_projectileoutlines_color_blu", "125 169 197 255", FCVAR_NONE,
		"The color used for outlines of BLU team's projectiles.",
		[](IConVar* var, const char* pOld, float) { GetModule()->ColorChanged(static_cast<ConVar*>(var), pOld); }),
	ce_projectileoutlines_color_red("ce_projectileoutlines_color_red", "189 55 55 255", FCVAR_NONE,
		"The color used for outlines of RED team's projectiles.",
		[](IConVar* var, const char* pOld, float) { GetModule()->ColorChanged(static_cast<ConVar*>(var), pOld); }),
	ce_projectileoutlines_fade_start("ce_projectileoutlines_fade_start", "-1", FCVAR_NONE,
		"Distance from the camera at which projectile outlines begin fading out."),
	ce_projectileoutlines_fade_end("ce_projectileoutlines_fade_end", "-1", FCVAR_NONE,
		"Distance from the camera at which projectile outlines finish fading out."),

	ce_projectileoutlines_mode("ce_projectileoutlines_mode", "1", FCVAR_NONE,
		"Modes:"
		"\n\t0: always"
		"\n\t1: only when occluded"
		"\n\t2: only when model is visible", true, 0, true, 2),

	m_BaseEntityInitHook(std::bind(&ProjectileOutlines::InitDetour, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))
{
	ColorChanged(&ce_projectileoutlines_color_blu, "");
	ColorChanged(&ce_projectileoutlines_color_red, "");
}

bool ProjectileOutlines::CheckDependencies()
{
	// CTFGlow
	{
		const auto ccGlow = Entities::GetClientClass("CTFGlow");

		s_GlowDisabledOffset = Entities::GetEntityProp<bool>(ccGlow, "m_bDisabled");
		s_GlowModeOffset = Entities::GetEntityProp<int>(ccGlow, "m_iMode");
		s_GlowTargetOffset = Entities::GetEntityProp<CHandle<C_BaseEntity>>(ccGlow, "m_hTarget");
		s_GlowColorOffset = Entities::GetEntityProp<Color>(ccGlow, "m_glowColor");
	}

	// CTFGrenadePipebombProjectile
	{
		const auto ccPipe = Entities::GetClientClass("CTFGrenadePipebombProjectile");

		s_PipeTypeOffset = Entities::GetEntityProp<TFGrenadePipebombType>(ccPipe, "m_iType");
	}

	// CTFRocket
	s_RocketType = Entities::GetClientClass("CTFProjectile_Rocket");

	return true;
}

Color ProjectileOutlines::CalcProjectileColor(IClientEntity* baseEntity, IClientEntity* glowEntity) {

	const TFTeam team = Entities::GetEntityTeamSafe(baseEntity);
	if (team == TFTeam::Blue)
		return m_ColorBlu;
	else if (team == TFTeam::Red)
		return m_ColorRed;
	else
		return Color(0, 255, 0, 255);
}

void ProjectileOutlines::OnTick(bool inGame)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);

	if (!ce_projectileoutlines_rockets.GetBool() && !ce_projectileoutlines_pills.GetBool() && !ce_projectileoutlines_stickies.GetBool())
		return;

	if (inGame)
	{
		IClientEntityList* const clientEntityList = Interfaces::GetClientEntityList();

		// Remove glows for dead entities
		{
			std::vector<int> toEraseList;
			for (const auto& glow : m_GlowEntities)
			{
				if (EHANDLE::FromIndex(glow.first).Get())
					continue;

				{
					C_BaseEntity* clientGlowEntity = glow.second.Get();
					if (clientGlowEntity)
						clientGlowEntity->Release();
				}

				toEraseList.push_back(glow.first);
			}

			for (auto toErase : toEraseList)
				m_GlowEntities.erase(toErase);
		}

		// Check for new glows
		for (const auto& newEntIndex : m_NewEntities)
		{
			IClientEntity* entity = clientEntityList->GetClientEntity(newEntIndex);
			if (!entity)
				continue;

			if (m_GlowEntities.find(entity->GetRefEHandle().ToInt()) != m_GlowEntities.end())
				continue;

			SoldierGlows(entity);
			DemoGlows(entity);
		}
		m_NewEntities.clear();

		// Update glows for existing entities
		for (const auto& glow : m_GlowEntities)
		{
			IClientEntity* baseEntity = EHANDLE::FromIndex(glow.first).Get();
			if (!baseEntity)
				continue;

			IClientEntity* glowEntity = glow.second.Get();
			if (!glowEntity)
				continue;

			const Vector viewPos = GetViewOrigin();
			const Vector entPos = baseEntity->GetAbsOrigin();
			const float dist = viewPos.DistTo(entPos);

			const byte alpha = (ce_projectileoutlines_fade_start.GetFloat() >= 0 && ce_projectileoutlines_fade_end.GetFloat() >= 0) ?
				Lerp(smoothstep(RemapValClamped(dist, ce_projectileoutlines_fade_start.GetFloat(), ce_projectileoutlines_fade_end.GetFloat(), 1, 0)), 0, 255) :
				255;

			auto newColor = CalcProjectileColor(baseEntity, glowEntity);
			newColor.a() = alpha;

			Color& curColor = s_GlowColorOffset.GetValue(glowEntity);
			if (newColor == curColor)
				continue;

			curColor = newColor;
			glowEntity->PostDataUpdate(DataUpdateType_t::DATA_UPDATE_DATATABLE_CHANGED);
		}
	}
}

void ProjectileOutlines::CreateGlowForEntity(IClientEntity* projectileEntity)
{
	C_BaseEntity* ent;
	{
		IClientNetworkable* networkable = GetHooks()->GetRawFunc<HookFunc::Global_CreateTFGlowObject>()(MAGIC_ENTNUM, MAGIC_SERIALNUM);
		ent = networkable->GetIClientUnknown()->GetBaseEntity();
	}

	{
		IClientEntity* glowEntity = ent;

		s_GlowDisabledOffset.GetValue(glowEntity) = false;
		s_GlowModeOffset.GetValue(glowEntity) = ce_projectileoutlines_mode.GetInt();
		s_GlowTargetOffset.GetValue(glowEntity).Set(projectileEntity->GetBaseEntity());
		s_GlowColorOffset.GetValue(glowEntity) = CalcProjectileColor(projectileEntity, glowEntity);
	}

	ent->PostDataUpdate(DataUpdateType_t::DATA_UPDATE_CREATED);
	ent->PostDataUpdate(DataUpdateType_t::DATA_UPDATE_DATATABLE_CHANGED);
	m_GlowEntities.emplace(projectileEntity->GetRefEHandle().ToInt(), ent);
}

void ProjectileOutlines::SoldierGlows(IClientEntity* entity)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (!ce_projectileoutlines_rockets.GetBool())
		return;

	// Only exact matches, we don't want to match anything that derives from rockets
	// (sentry rockets, dragon's fury, etc)
	if (entity->GetClientClass() != s_RocketType)
		return;

	CreateGlowForEntity(entity);
}

void ProjectileOutlines::DemoGlows(IClientEntity* entity)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	const bool pills = ce_projectileoutlines_pills.GetBool();
	const bool stickies = ce_projectileoutlines_stickies.GetBool();
	if (!pills && !stickies)
		return;

	TFGrenadePipebombType type;
	if (auto tryType = s_PipeTypeOffset.TryGetValue(entity))
		type = *tryType;
	else
		return;

	if (pills && type == TFGrenadePipebombType::Pill)
		CreateGlowForEntity(entity);
	else if (stickies && (type == TFGrenadePipebombType::Sticky || type == TFGrenadePipebombType::StickyJumper))
		CreateGlowForEntity(entity);
}

bool ProjectileOutlines::InitDetour(C_BaseEntity* pThis, int entnum, int iSerialNum)
{
	if (entnum == MAGIC_ENTNUM && iSerialNum == MAGIC_SERIALNUM)
	{
		GetHooks()->SetState<HookFunc::C_BaseEntity_Init>(Hooking::HookAction::SUPERCEDE);
		return pThis->InitializeAsClientEntity(nullptr, RENDER_GROUP_OTHER);
	}

	// Potential new entities for glow next OnTick()
	m_NewEntities.push_back(entnum);

	GetHooks()->SetState<HookFunc::C_BaseEntity_Init>(Hooking::HookAction::IGNORE);
	return true;
}

void ProjectileOutlines::ColorChanged(ConVar* var, const char* oldValue)
{
	Color scannedColor;
	if (!ColorFromConVar(*var, scannedColor))
		goto Usage;

	if (var == &ce_projectileoutlines_color_blu)
		m_ColorBlu = scannedColor;
	else if (var == &ce_projectileoutlines_color_red)
		m_ColorRed = scannedColor;

	return;

Usage:
	PluginWarning("Usage: %s <r> <g> <b> <a>\n", var->GetName());
	var->SetValue(oldValue);
}

void ProjectileOutlines::UpdateEnabled()
{
	m_BaseEntityInitHook.SetEnabled(
		ce_projectileoutlines_pills.GetBool() ||
		ce_projectileoutlines_rockets.GetBool() ||
		ce_projectileoutlines_stickies.GetBool());
}
