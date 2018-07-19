#pragma once

#include "Misc/CRefPtrFix.h"
#include "PluginBase/EntityOffset.h"
#include "PluginBase/Modules.h"

#define GLOWS_ENABLE
#include <client/glow_outline_effect.h>

#include <array>
#include <vector>

class C_BaseEntity;
class CGlowObjectManager;
class CViewSetup;
class CMatRenderContextPtr;
class IMaterial;
class ConCommand;
class CCommand;
enum OverrideType_t;
class Player;

class Graphics final : public Module<Graphics>
{
public:
	Graphics();
	~Graphics();

	const ConVar& GetDebugGlowConVar() const { return ce_graphics_debug_glow; }

	static bool CheckDependencies();

protected:
	void OnTick(bool inGame) override;

private:
	ConVar ce_graphics_disable_prop_fades;
	ConVar ce_graphics_debug_glow;
	ConVar ce_graphics_glow_silhouettes;
	ConVar ce_graphics_glow_intensity;
	ConVar ce_graphics_improved_glows;
	ConVar ce_graphics_fix_invisible_players;

	ConVar ce_outlines_mode;
	ConVar ce_outlines_players_override_red;
	ConVar ce_outlines_players_override_blue;
	ConVar ce_outlines_debug_stencil_out;
	ConVar ce_outlines_additive;
	ConVar ce_outlines_debug;
	ConVar ce_outlines_spy_visibility;
	ConVar ce_outlines_cull_frustum;
	ConVar ce_outlines_pvs_optimizations;

	ConCommand ce_infills_test;
	ConVar ce_infills_enable;
	ConVar ce_infills_additive;
	ConVar ce_infills_debug;
	ConVar ce_infills_hurt_red;
	ConVar ce_infills_hurt_blue;
	//ConVar* ce_infills_normal_direction;
	ConVar ce_infills_buffed_red;
	ConVar ce_infills_buffed_blue;
	//ConVar* ce_infills_buffed_direction;

	ConVar ce_infills_flicker_hertz;
	ConVar ce_infills_flicker_intensity;
	ConVar ce_infills_flicker_after_hurt_time;
	ConVar ce_infills_flicker_after_hurt_bias;
	ConVar ce_infills_fade_after_hurt_time;
	ConVar ce_infills_fade_after_hurt_bias;

	ConCommand ce_graphics_dump_shader_params;

	static bool IsDefaultParam(const char* paramName);
	static void DumpShaderParams(const CCommand& cmd);
	static int DumpShaderParamsAutocomplete(const char *partial, char commands[COMMAND_COMPLETION_MAXITEMS][COMMAND_COMPLETION_ITEM_LENGTH]);

	int m_ComputeEntityFadeHook;
	unsigned char ComputeEntityFadeOveride(C_BaseEntity* entity, float minDist, float maxDist, float fadeScale);

	int m_ApplyEntityGlowEffectsHook;
	void ApplyEntityGlowEffectsOverride(CGlowObjectManager* pThis, const CViewSetup* pSetup, int nSplitScreenSlot, CMatRenderContextPtr& pRenderContext, float flBloomScale, int x, int y, int w, int h);

	int m_ForcedMaterialOverrideHook;
	void ForcedMaterialOverrideOverride(IMaterial* material, OverrideType_t overrideType);

	void DrawGlowAlways(int nSplitScreenSlot, CMatRenderContextPtr& pRenderContext) const;
	void DrawGlowOccluded(int nSplitScreenSlot, CMatRenderContextPtr& pRenderContext) const;
	void DrawGlowVisible(int nSplitScreenSlot, CMatRenderContextPtr& pRenderContext) const;

	enum class InfillType
	{
		Hurt,
		Buffed,

		Count
	};

	enum class GlowMode
	{
		Never,
		Occluded = (1 << 0),
		Unoccluded = (1 << 1),
		Always = (Occluded | Unoccluded)
	};

	struct Infill
	{
		bool m_Active = false;
		Color m_Color;
		Vector2D m_RectMin = Vector2D(-1, -1);
		Vector2D m_RectMax = Vector2D(-1, -1);
	};

	friend class CGlowObjectManager;
	struct ExtraGlowData
	{
		ExtraGlowData(CGlowObjectManager::GlowObjectDefinition_t* base);

		CGlowObjectManager::GlowObjectDefinition_t* m_Base;

		bool m_ShouldOverrideGlowColor;
		Vector m_GlowColorOverride;

		uint8_t m_StencilIndex;

		GlowMode m_Mode;

		bool AnyInfillsActive() const;

		// Applies the glow color settings to the rendering system
		void ApplyGlowColor() const;

		std::array<Infill, (size_t)InfillType::Count> m_Infills;

		// This list is refreshed every frame and only used within a single "entry"
		// into our glow system, so it's ok to use pointers here rather than EHANDLES
		std::vector<C_BaseEntity*> m_MoveChildren;
	};
	std::vector<ExtraGlowData> m_ExtraGlowData;
	const CViewSetup* m_View;

	void ResetPlayerHurtTimes();

	void BuildMoveChildLists();
	ExtraGlowData* FindExtraGlowData(int entindex);
	void BuildExtraGlowData(CGlowObjectManager* glowMgr, bool& anyAlways, bool& anyOccluded, bool& anyUnoccluded);
	void EnforceSpyVisibility(Player& player, CGlowObjectManager::GlowObjectDefinition_t& outline) const;

	bool WorldToScreenMat(const VMatrix& worldToScreen, const Vector& world, Vector2D& screen);

	static void GetAABBCorner(const Vector& mins, const Vector& maxs, uint_fast8_t cornerIndex, Vector& corner);
	static void GetRotatedBBCorners(const Vector& origin, const QAngle& angles, const Vector& mins, const Vector& maxs, Vector corners[8]);

	bool Test_PlaneHitboxesIntersect(C_BaseAnimating* animating, const Frustum_t& viewFrustum, const VMatrix& worldToScreen, Vector2D& screenMins, Vector2D& screenMaxs);

	float ApplyInfillTimeEffects(float lastHurtTime);
	void DrawInfills(CMatRenderContextPtr& pRenderContext);

	static EntityOffset<CHandle<C_BaseEntity>> s_MoveParent;
	static EntityTypeChecker s_TFViewModelType;
};