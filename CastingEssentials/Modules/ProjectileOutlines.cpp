#include "ProjectileOutlines.h"
#include "PluginBase/Entities.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/TFDefinitions.h"

#include <PolyHook.h>

#include <toolframework/ienginetool.h>

ProjectileOutlines::ProjectileOutlines()
{
	m_Init = false;

	m_RocketsEnabled = new ConVar("ce_projectileoutlines_rockets", "0", FCVAR_NONE, "Enable projectile outlines for rockets.");
	m_PillsEnabled = new ConVar("ce_projectileoutlines_pills", "0", FCVAR_NONE, "Enable projectile outlines for pills.");
	m_StickiesEnabled = new ConVar("ce_projectileoutlines_stickies", "0", FCVAR_NONE, "Enable projectile outlines for stickies.");

	// 88 133 162 255
	ce_projectileoutlines_color_blu = new ConVar("ce_projectileoutlines_color_blu", "125 169 197 255", FCVAR_NONE, "The color used for outlines of BLU team's projectiles.", &ColorChanged);

	// 184 56 59 255
	ce_projectileoutlines_color_red = new ConVar("ce_projectileoutlines_color_red", "189 55 55 255", FCVAR_NONE, "The color used for outlines of RED team's projectiles.", &ColorChanged);
}

void ProjectileOutlines::OnTick(bool inGame)
{
	if (!m_Init)
	{
		ColorChanged(ce_projectileoutlines_color_blu, "", 0);
		ColorChanged(ce_projectileoutlines_color_red, "", 0);
		m_Init = true;
	}

	if (inGame)
	{
		IClientEntityList* const clientEntityList = Interfaces::GetClientEntityList();

		// Remove glows for dead entities
		{
			std::vector<EHANDLE> toEraseList;
			for (const auto& glow : m_GlowEntities)
			{
				if (glow.first.Get())
					continue;

				C_BaseEntity* clientGlowEntity = glow.second.Get();
				clientGlowEntity->Release();
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
	}
}

CHandle<C_BaseEntity> ProjectileOutlines::CreateGlowForEntity(IClientEntity* projectileEntity)
{
	EHANDLE handle;
	{
		static constexpr int MAGIC_ENTNUM = 0x141BCF9B;
		static constexpr int MAGIC_SERIALNUM = 0x0FCAD8B9;

		static std::unique_ptr<PLH::Detour> detour;
		if (!detour)
		{
			Hooking::Internal::LocalDetourFnPtr<C_BaseEntity, bool, int, int> detourFn = [](C_BaseEntity* pThis, void*, int entnum, int iSerialNum)
			{
				if (entnum == MAGIC_ENTNUM && iSerialNum == MAGIC_SERIALNUM)
					return pThis->InitializeAsClientEntity(nullptr, RENDER_GROUP_OTHER);
				else
					detour->GetOriginal<Hooking::Internal::LocalFnPtr<C_BaseEntity, bool, int, int>>()(pThis, entnum, iSerialNum);
			};

			detour.reset(new PLH::Detour);
			detour->SetupHook((BYTE*)HookManager::GetRawFunc_C_BaseEntity_Init(), (BYTE*)detourFn);
			if (!detour->Hook())
			{
				detour.reset();
				return nullptr;
			}
		}

		IClientNetworkable* networkable = GetHooks()->GetFunc<Global_CreateTFGlowObject>()(MAGIC_ENTNUM, MAGIC_SERIALNUM);
		handle = networkable->GetIClientUnknown()->GetBaseEntity();
	}

	{
		IClientEntity* glowEntity = handle.Get();
		*Entities::GetEntityProp<bool*>(glowEntity, { "m_bDisabled" }) = false;
		*Entities::GetEntityProp<int*>(glowEntity, { "m_iMode" }) = 0;
		Entities::GetEntityProp<EHANDLE*>(glowEntity, { "m_hTarget" })->Set(projectileEntity->GetBaseEntity());

		Color* color = Entities::GetEntityProp<Color*>(glowEntity, { "m_glowColor" });
		TFTeam* team = Entities::GetEntityTeam(projectileEntity);
		if (team && *team == TFTeam::Blue)
			*color = m_ColorBlu;
		else if (team && *team == TFTeam::Red)
			*color = m_ColorRed;
		else
			color->SetColor(255, 255, 255, 255);
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