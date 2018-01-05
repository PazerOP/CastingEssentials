#pragma once

#include "PluginBase/Modules.h"

class C_BaseEntity;
class CGlowObjectManager;
class CViewSetup;
class CMatRenderContextPtr;
class IMaterial;
class ConCommand;
class CCommand;
enum OverrideType_t;

class Graphics final : public Module<Graphics>
{
public:
	Graphics();
	~Graphics();

	ConVar* GetDebugGlowConVar() const { return ce_graphics_debug_glow; }

protected:
	void OnTick(bool inGame) override;

private:
	ConVar* ce_graphics_disable_prop_fades;
	ConVar* ce_graphics_debug_glow;
	ConVar* ce_graphics_glow_silhouettes;
	ConVar* ce_graphics_glow_intensity;
	ConVar* ce_graphics_improved_glows;
	ConVar* ce_graphics_fix_invisible_players;
	ConVar* ce_graphics_glow_l4d;

	ConCommand* ce_graphics_dump_shader_params;

	static bool IsDefaultParam(const char* paramName);
	static void DumpShaderParams(const CCommand& cmd);
	static int DumpShaderParamsAutocomplete(const char *partial, char commands[64][64]);

	int m_ComputeEntityFadeHook;
	unsigned char ComputeEntityFadeOveride(C_BaseEntity* entity, float minDist, float maxDist, float fadeScale);

	int m_ApplyEntityGlowEffectsHook;
	void ApplyEntityGlowEffectsOverride(CGlowObjectManager* pThis, const CViewSetup* pSetup, int nSplitScreenSlot, CMatRenderContextPtr& pRenderContext, float flBloomScale, int x, int y, int w, int h);

	int m_ForcedMaterialOverrideHook;
	void ForcedMaterialOverrideOverride(IMaterial* material, OverrideType_t overrideType);
};