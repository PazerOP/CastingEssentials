#pragma once
#include "PluginBase/EntityOffset.h"
#include "PluginBase/Hook.h"
#include "PluginBase/Modules.h"

#include <convar.h>
#include <ehandle.h>

#include <unordered_map>
#include <vector>

class C_BaseEntity;
class IClientEntity;
class IConVar;
enum class TFGrenadePipebombType;

class ProjectileOutlines final : public Module<ProjectileOutlines>
{
public:
	ProjectileOutlines();

	static bool CheckDependencies();
	static constexpr __forceinline const char* GetModuleName() { return "Projectile Outlines"; }

private:
	ConVar ce_projectileoutlines_rockets;
	ConVar ce_projectileoutlines_pills;
	ConVar ce_projectileoutlines_stickies;

	ConVar ce_projectileoutlines_mode;

	ConVar ce_projectileoutlines_color_blu;
	ConVar ce_projectileoutlines_color_red;

	ConVar ce_projectileoutlines_fade_start;
	ConVar ce_projectileoutlines_fade_end;

	void OnTick(bool inGame) override;

	void SoldierGlows(IClientEntity* entity);
	void DemoGlows(IClientEntity* entity);
	Color CalcProjectileColor(IClientEntity* baseEntity, IClientEntity* glowEntity);

	void CreateGlowForEntity(IClientEntity* ent);
	std::unordered_map<int, CHandle<C_BaseEntity>> m_GlowEntities;

	// Some random numbers I generated
	static constexpr int MAGIC_ENTNUM = 0x141BCF9B;
	static constexpr int MAGIC_SERIALNUM = 0x0FCAD8B9;

	Hook<HookFunc::C_BaseEntity_Init> m_BaseEntityInitHook;
	bool InitDetour(C_BaseEntity* pThis, int entnum, int iSerialNum);

	// List of entities that had C_BaseEntity::Init() called on them since our last
	// pass through OnTick()
	std::vector<int> m_NewEntities;

	bool m_Init;
	void ColorChanged(ConVar* var, const char* oldValue);
	Color m_ColorRed;
	Color m_ColorBlu;

	void UpdateEnabled();

	static EntityOffset<bool> s_GlowDisabledOffset;
	static EntityOffset<int> s_GlowModeOffset;
	static EntityOffset<CHandle<C_BaseEntity>> s_GlowTargetOffset;
	static EntityOffset<Color> s_GlowColorOffset;

	static EntityOffset<TFGrenadePipebombType> s_PipeTypeOffset;
	static const ClientClass* s_RocketType;
};