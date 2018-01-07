#pragma once

#include "PluginBase/Modules.h"

#define GLOWS_ENABLE
#include <client/glow_outline_effect.h>

#include <vector>

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

	ConVar* ce_outlines_players_override_red;
	ConVar* ce_outlines_players_override_blue;
	ConVar* ce_outlines_debug_stencil_out;
	ConVar* ce_outlines_additive;

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

	void DrawGlowAlways(CUtlVector<CGlowObjectManager::GlowObjectDefinition_t>& glowObjectDefinitions,
		int nSplitScreenSlot, CMatRenderContextPtr& pRenderContext) const;
	void DrawGlowOccluded(CUtlVector<CGlowObjectManager::GlowObjectDefinition_t>& glowObjectDefinitions,
		int nSplitScreenSlot, CMatRenderContextPtr& pRenderContext) const;
	void DrawGlowVisible(CUtlVector<CGlowObjectManager::GlowObjectDefinition_t>& glowObjectDefinitions,
		int nSplitScreenSlot, CMatRenderContextPtr& pRenderContext) const;

	friend class CGlowObjectManager;
	struct ExtraGlowData
	{
		ExtraGlowData();

		bool m_ShouldOverrideGlowColor;
		Vector m_GlowColorOverride;

		bool m_InfillEnabled;
		uint8_t m_StencilIndex;
	};
	std::vector<ExtraGlowData> m_ExtraGlowData;

	void BuildExtraGlowData(CGlowObjectManager* glowMgr);
};