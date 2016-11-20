#pragma once
#include "PluginBase/Modules.h"

#include <ehandle.h>
#include <client/c_baseentity.h>

#include <map>

class ProjectileOutlines final : public Module
{
public:
	ProjectileOutlines();
	~ProjectileOutlines();

	static ProjectileOutlines* GetModule() { return Modules().GetModule<ProjectileOutlines>(); }
	static const char* GetModuleName() { return Modules().GetModuleName<ProjectileOutlines>().c_str(); }

private:
	ConVar* m_RocketsEnabled;
	ConVar* m_PillsEnabled;
	ConVar* m_StickiesEnabled;

	ConVar* ce_projectileoutlines_mode;

	ConVar* ce_projectileoutlines_color_blu;
	ConVar* ce_projectileoutlines_color_red;

	ConVar* ce_projectileoutlines_fade_start;
	ConVar* ce_projectileoutlines_fade_end;

	void OnTick(bool inGame) override;

	void SoldierGlows(IClientEntity* entity);
	void DemoGlows(IClientEntity* entity);

	CHandle<C_BaseEntity> CreateGlowForEntity(IClientEntity* ent);
	std::map<EHANDLE, EHANDLE> m_GlowEntities;

	// Some random numbers I generated
	static constexpr int MAGIC_ENTNUM = 0x141BCF9B;
	static constexpr int MAGIC_SERIALNUM = 0x0FCAD8B9;

	int m_BaseEntityInitHook;
	static bool InitDetour(C_BaseEntity* pThis, int entnum, int iSerialNum);

	bool m_Init;
	static void ColorChanged(IConVar* var, const char* oldValue, float flOldValue);
	Color m_ColorRed;
	Color m_ColorBlu;
};