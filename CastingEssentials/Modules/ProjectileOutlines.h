#pragma once
#include "PluginBase/Modules.h"

#include <ehandle.h>
#include <client/c_baseentity.h>

#include <map>

class ProjectileOutlines final : public Module
{
public:
	ProjectileOutlines();

	static ProjectileOutlines* GetModule() { return Modules().GetModule<ProjectileOutlines>(); }
	static const char* GetModuleName() { return Modules().GetModuleName<ProjectileOutlines>().c_str(); }

private:
	ConVar* m_RocketsEnabled;
	ConVar* m_PillsEnabled;
	ConVar* m_StickiesEnabled;

	ConVar* ce_projectileoutlines_color_blu;
	ConVar* ce_projectileoutlines_color_red;

	void OnTick(bool inGame) override;

	void SoldierGlows(IClientEntity* entity);
	void DemoGlows(IClientEntity* entity);

	CHandle<C_BaseEntity> CreateGlowForEntity(IClientEntity* ent);
	std::map<EHANDLE, EHANDLE> m_GlowEntities;

	bool m_Init;
	static void ColorChanged(IConVar* var, const char* oldValue, float flOldValue);
	Color m_ColorRed;
	Color m_ColorBlu;
};