#include "ProjectileOutlines.h"
#include "PluginBase/Entities.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/TFDefinitions.h"

#include <PolyHook.h>

#include <toolframework/ienginetool.h>

// smh windows
#undef IGNORE

ProjectileOutlines::ProjectileOutlines()
{
	m_Init = false;
	m_BaseEntityInitHook = 0;

	m_RocketsEnabled = new ConVar("ce_projectileoutlines_rockets", "0", FCVAR_NONE, "Enable projectile outlines for rockets.");
	m_PillsEnabled = new ConVar("ce_projectileoutlines_pills", "0", FCVAR_NONE, "Enable projectile outlines for pills.");
	m_StickiesEnabled = new ConVar("ce_projectileoutlines_stickies", "0", FCVAR_NONE, "Enable projectile outlines for stickies.");

	ce_projectileoutlines_color_blu = new ConVar("ce_projectileoutlines_color_blu", "125 169 197 255", FCVAR_NONE, "The color used for outlines of BLU team's projectiles.", &ColorChanged);

	ce_projectileoutlines_color_red = new ConVar("ce_projectileoutlines_color_red", "189 55 55 255", FCVAR_NONE, "The color used for outlines of RED team's projectiles.", &ColorChanged);

	ce_projectileoutlines_fade_start = new ConVar("ce_projectileoutlines_fade_start", "-1", FCVAR_NONE, "Distance from the camera at which projectile outlines begin fading out.");
	ce_projectileoutlines_fade_end = new ConVar("ce_projectileoutlines_fade_end", "-1", FCVAR_NONE, "Distance from the camera at which projectile outlines finish fading out.");

	// For some reason this isn't hooked up, wtf valve
	ce_projectileoutlines_mode = new ConVar("ce_projectileoutlines_mode", "1", FCVAR_UNREGISTERED, "Modes:\n    0: always\n    1: only when occluded\n    2: only when model is visible", true, 0, true, 2);
}

ProjectileOutlines::~ProjectileOutlines()
{
	if (m_BaseEntityInitHook && GetHooks()->RemoveHook<C_BaseEntity_Init>(m_BaseEntityInitHook, __FUNCSIG__))
		m_BaseEntityInitHook = 0;

	Assert(!m_BaseEntityInitHook);
}

void ProjectileOutlines::OnTick(bool inGame)
{
	if (inGame)
	{
		if (!m_Init)
		{
			ColorChanged(ce_projectileoutlines_color_blu, "", 0);
			ColorChanged(ce_projectileoutlines_color_red, "", 0);
			m_BaseEntityInitHook = GetHooks()->AddHook<C_BaseEntity_Init>(std::bind(&InitDetour, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
			m_Init = true;
		}

		IClientEntityList* const clientEntityList = Interfaces::GetClientEntityList();

		// Remove glows for dead entities
		{
			std::vector<EHANDLE> toEraseList;
			for (const auto& glow : m_GlowEntities)
			{
				if (glow.first.Get())
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

		// Add new glows
		for (int i = Interfaces::GetEngineTool()->GetMaxClients(); i <= clientEntityList->GetHighestEntityIndex(); i++)
		{
			IClientEntity* entity = clientEntityList->GetClientEntity(i);
			if (!entity)
				continue;

			if (m_GlowEntities.find(CHandle<C_BaseEntity>(entity->GetBaseEntity())) != m_GlowEntities.end())
				continue;

			SoldierGlows(entity);
			DemoGlows(entity);
		}

		// Update glows for existing entities
		for (const auto& glow : m_GlowEntities)
		{
			IClientEntity* baseEntity = glow.first.Get();
			if (!baseEntity)
				continue;

			IClientEntity* glowEntity = glow.second.Get();
			if (!glowEntity)
				continue;

			const Vector viewPos = GetViewOrigin();
			const Vector entPos = baseEntity->GetAbsOrigin();
			const float dist = viewPos.DistTo(entPos);

			const byte alpha = (ce_projectileoutlines_fade_start->GetFloat() >= 0 && ce_projectileoutlines_fade_end->GetFloat() >= 0) ?
				Lerp(smoothstep(RemapValClamped(dist, ce_projectileoutlines_fade_start->GetFloat(), ce_projectileoutlines_fade_end->GetFloat(), 1, 0)), 0, 255) :
				255;

			Color* glowColor = Entities::GetEntityProp<Color*>(glowEntity, { "m_glowColor" });
			if (!glowColor)
				continue;

			Color newColor(glowColor->r(), glowColor->g(), glowColor->b(), alpha);
			if (newColor == *glowColor)
				continue;

			*glowColor = newColor;
			glowEntity->PostDataUpdate(DataUpdateType_t::DATA_UPDATE_DATATABLE_CHANGED);
		}
	}
}

CHandle<C_BaseEntity> ProjectileOutlines::CreateGlowForEntity(IClientEntity* projectileEntity)
{
	EHANDLE handle;
	{
		IClientNetworkable* networkable = GetHooks()->GetRawFunc_Global_CreateTFGlowObject()(MAGIC_ENTNUM, MAGIC_SERIALNUM);
		handle = networkable->GetIClientUnknown()->GetBaseEntity();
	}

	{
		IClientEntity* glowEntity = handle.Get();
		*Entities::GetEntityProp<bool*>(glowEntity, { "m_bDisabled" }) = false;
		*Entities::GetEntityProp<int*>(glowEntity, { "m_iMode" }) = ce_projectileoutlines_mode->GetInt();
		Entities::GetEntityProp<EHANDLE*>(glowEntity, { "m_hTarget" })->Set(projectileEntity->GetBaseEntity());

		Color* color = Entities::GetEntityProp<Color*>(glowEntity, { "m_glowColor" });
		TFTeam* team = Entities::GetEntityTeam(projectileEntity);
		if (team && *team == TFTeam::Blue)
			*color = m_ColorBlu;
		else if (team && *team == TFTeam::Red)
			*color = m_ColorRed;
		else
			color->SetColor(0, 255, 0, 255);
	}

	handle->PostDataUpdate(DataUpdateType_t::DATA_UPDATE_CREATED);
	handle->PostDataUpdate(DataUpdateType_t::DATA_UPDATE_DATATABLE_CHANGED);
	return handle;
}

void ProjectileOutlines::SoldierGlows(IClientEntity* entity)
{
	if (!m_RocketsEnabled->GetBool())
		return;
	
	if (!Entities::CheckEntityBaseclass(entity, "TFProjectile_Rocket"))
		return;

	m_GlowEntities.insert(std::make_pair<EHANDLE, EHANDLE>(entity->GetBaseEntity(), CreateGlowForEntity(entity)));
}

void ProjectileOutlines::DemoGlows(IClientEntity* entity)
{
	const bool pills = m_PillsEnabled->GetBool();
	const bool stickies = m_StickiesEnabled->GetBool();
	if (!pills && !stickies)
		return;

	if (!Entities::CheckEntityBaseclass(entity, "TFProjectile_Pipebomb"))
		return;

	const TFGrenadePipebombType type = *Entities::GetEntityProp<TFGrenadePipebombType*>(entity, { "m_iType" });

	if (pills && type == TFGrenadePipebombType::Pill)
		m_GlowEntities.insert(std::make_pair<EHANDLE, EHANDLE>(entity->GetBaseEntity(), CreateGlowForEntity(entity)));
	else if (stickies && type == TFGrenadePipebombType::Sticky)
		m_GlowEntities.insert(std::make_pair<EHANDLE, EHANDLE>(entity->GetBaseEntity(), CreateGlowForEntity(entity)));
}

bool ProjectileOutlines::InitDetour(C_BaseEntity* pThis, int entnum, int iSerialNum)
{
	if (entnum == MAGIC_ENTNUM && iSerialNum == MAGIC_SERIALNUM)
	{
		GetHooks()->SetState<C_BaseEntity_Init>(Hooking::HookAction::SUPERCEDE);
		return pThis->InitializeAsClientEntity(nullptr, RENDER_GROUP_OTHER);
	}

	GetHooks()->SetState<C_BaseEntity_Init>(Hooking::HookAction::IGNORE);
	return true;
}

void ProjectileOutlines::ColorChanged(IConVar* var, const char* oldValue, float flOldValue)
{
	Assert(var);
	if (!var)
		return;
	
	ConVar* convar = dynamic_cast<ConVar*>(var);
	Assert(cvar);
	if (!cvar)
	{
		PluginWarning("%s: Failed to cast %s to a ConVar\n", __FUNCSIG__, var->GetName());
		goto Revert;
	}

	int r, g, b, a;
	const auto readArguments = sscanf_s(convar->GetString(), "%i %i %i %i", &r, &g, &b, &a);
	if (readArguments != 4)
		goto Usage;

	if (r < 0 || r > 255)
	{
		PluginWarning("Red value out of range!\n");
		goto Usage;
	}
	if (g < 0 || g > 255)
	{
		PluginWarning("Green value out of range!\n");
		goto Usage;
	}
	if (b < 0 || b > 255)
	{
		PluginWarning("Blue value out of range!\n");
		goto Usage;
	}
	if (a < 0 || a > 255)
	{
		PluginWarning("Alpha value out of range!\n");
		goto Usage;
	}

	// Home stretch
	{
		const Color scannedColor(r, g, b, a);

		if (!stricmp(var->GetName(), GetModule()->ce_projectileoutlines_color_blu->GetName()))
			GetModule()->m_ColorBlu = scannedColor;
		else if (!stricmp(var->GetName(), GetModule()->ce_projectileoutlines_color_red->GetName()))
			GetModule()->m_ColorRed = scannedColor;
		else
			Error("[CastingEssentials] %s: Somehow we caught a FnChangeCallback_t for an IConVar named %s???", __FUNCSIG__, var->GetName());

		return;
	}

Usage:
	PluginWarning("Usage: %s <r> <g> <b> <a>\n", convar->GetName(), convar->GetName());
Revert:
	var->SetValue(oldValue);
}