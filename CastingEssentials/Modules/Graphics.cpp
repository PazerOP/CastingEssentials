#include "Graphics.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Entities.h"

#include <convar.h>
#include <debugoverlay_shared.h>

#define GLOWS_ENABLE
#include <client/glow_outline_effect.h>
#include <view_shared.h>
#include <materialsystem/itexture.h>
#include <materialsystem/imaterial.h>
#include <materialsystem/imaterialvar.h>
#include <model_types.h>
#include <shaderapi/ishaderapi.h>
#include <vprof.h>

#include <algorithm>
#include <random>

#undef min
#undef max

static CGlowObjectManager* s_LocalGlowObjectManager;

// Should we use a hook to disable IStudioRender::ForcedMaterialOverride?
static bool s_DisableForcedMaterialOverride = false;

Graphics::Graphics()
{
	ce_graphics_disable_prop_fades = new ConVar("ce_graphics_disable_prop_fades", "0", FCVAR_UNREGISTERED, "Enable/disable prop fading.");
	ce_graphics_debug_glow = new ConVar("ce_graphics_debug_glow", "0", FCVAR_UNREGISTERED);
	ce_graphics_glow_silhouettes = new ConVar("ce_graphics_glow_silhouettes", "1", FCVAR_NONE, "Turns outlines into silhouettes.");
	ce_graphics_glow_intensity = new ConVar("ce_graphics_glow_intensity", "1", FCVAR_NONE, "Global scalar for glow intensity");
	ce_graphics_improved_glows = new ConVar("ce_graphics_improved_glows", "1", FCVAR_NONE, "Should we used the new and improved glow code?");

	m_ComputeEntityFadeHook = GetHooks()->AddHook<Global_UTILComputeEntityFade>(std::bind(&Graphics::ComputeEntityFadeOveride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

	m_ApplyEntityGlowEffectsHook = GetHooks()->AddHook<CGlowObjectManager_ApplyEntityGlowEffects>(std::bind(&Graphics::ApplyEntityGlowEffectsOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7, std::placeholders::_8, std::placeholders::_9));

	m_ForcedMaterialOverrideHook = GetHooks()->AddHook<IStudioRender_ForcedMaterialOverride>(std::bind(&Graphics::ForcedMaterialOverrideOverride, this, std::placeholders::_1, std::placeholders::_2));
}

Graphics::~Graphics()
{
	if (m_ComputeEntityFadeHook && GetHooks()->RemoveHook<Global_UTILComputeEntityFade>(m_ComputeEntityFadeHook, __FUNCSIG__))
		m_ComputeEntityFadeHook = 0;

	Assert(!m_ComputeEntityFadeHook);

	if (m_ApplyEntityGlowEffectsHook && GetHooks()->RemoveHook<CGlowObjectManager_ApplyEntityGlowEffects>(m_ApplyEntityGlowEffectsHook, __FUNCSIG__))
		m_ApplyEntityGlowEffectsHook = 0;

	Assert(!m_ApplyEntityGlowEffectsHook);

	if (m_ForcedMaterialOverrideHook && GetHooks()->RemoveHook<IStudioRender_ForcedMaterialOverride>(m_ForcedMaterialOverrideHook, __FUNCSIG__))
		m_ForcedMaterialOverrideHook = 0;

	Assert(!m_ForcedMaterialOverrideHook);
}

unsigned char Graphics::ComputeEntityFadeOveride(C_BaseEntity* entity, float minDist, float maxDist, float fadeScale)
{
	constexpr auto max = std::numeric_limits<unsigned char>::max();

	if (ce_graphics_disable_prop_fades->GetBool())
	{
		GetHooks()->SetState<Global_UTILComputeEntityFade>(Hooking::HookAction::SUPERCEDE);
		return max;
	}

	return 0;
}

void Graphics::ApplyEntityGlowEffectsOverride(CGlowObjectManager * pThis, const CViewSetup * pSetup, int nSplitScreenSlot, CMatRenderContextPtr & pRenderContext, float flBloomScale, int x, int y, int w, int h)
{
	if (ce_graphics_improved_glows->GetBool())
	{
		GetHooks()->SetState<CGlowObjectManager_ApplyEntityGlowEffects>(Hooking::HookAction::SUPERCEDE);
		pThis->ApplyEntityGlowEffects(pSetup, nSplitScreenSlot, pRenderContext, flBloomScale, x, y, w, h);
	}
	else
	{
		GetHooks()->SetState<CGlowObjectManager_ApplyEntityGlowEffects>(Hooking::HookAction::IGNORE);
	}
}

void Graphics::ForcedMaterialOverrideOverride(IMaterial* material, OverrideType_t overrideType)
{
	if (s_DisableForcedMaterialOverride)
	{
		GetHooks()->SetState<IStudioRender_ForcedMaterialOverride>(Hooking::HookAction::SUPERCEDE);
		// Do nothing
	}
	else
	{
		GetHooks()->SetState<IStudioRender_ForcedMaterialOverride>(Hooking::HookAction::IGNORE);
	}
}

struct ShaderStencilState_t
{
	bool m_bEnable;
	StencilOperation_t m_FailOp;
	StencilOperation_t m_ZFailOp;
	StencilOperation_t m_PassOp;
	StencilComparisonFunction_t m_CompareFunc;
	int m_nReferenceValue;
	uint32 m_nTestMask;
	uint32 m_nWriteMask;

	ShaderStencilState_t()
	{
		m_bEnable = false;
		m_PassOp = m_FailOp = m_ZFailOp = STENCILOPERATION_KEEP;
		m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
		m_nReferenceValue = 0;
		m_nTestMask = m_nWriteMask = 0xFFFFFFFF;
	}

	void SetStencilState(CMatRenderContextPtr &pRenderContext)
	{
		pRenderContext->SetStencilEnable(m_bEnable);

		if (m_bEnable)
		{
			pRenderContext->SetStencilCompareFunction(m_CompareFunc);

			if (m_CompareFunc != STENCILCOMPARISONFUNCTION_ALWAYS)
				pRenderContext->SetStencilFailOperation(m_FailOp);
			if (m_CompareFunc != STENCILCOMPARISONFUNCTION_ALWAYS && m_CompareFunc != STENCILCOMPARISONFUNCTION_NEVER)
				pRenderContext->SetStencilTestMask(m_nTestMask);
			if (m_CompareFunc != STENCILCOMPARISONFUNCTION_NEVER)
				pRenderContext->SetStencilPassOperation(m_PassOp);

			pRenderContext->SetStencilZFailOperation(m_ZFailOp);
			pRenderContext->SetStencilReferenceValue(m_nReferenceValue);
			pRenderContext->SetStencilWriteMask(m_nWriteMask);
		}
	}
};

static thread_local std::map<int, std::vector<int>> s_MoveChildren;
static void BuildMoveChildMap()
{
	s_MoveChildren.clear();

	IClientEntityList* const entityList = Interfaces::GetClientEntityList();
	for (int i = 0; i < entityList->GetHighestEntityIndex(); i++)
	{
		C_BaseEntity* child = dynamic_cast<C_BaseEntity*>(entityList->GetClientEntity(i));
		if (!child || !child->ShouldDraw())
			continue;

		EHANDLE* moveparent = Entities::GetEntityProp<EHANDLE*>(child, "moveparent");
		if (!moveparent || !moveparent->IsValid())
			continue;

		s_MoveChildren[moveparent->ToInt()].push_back(EHANDLE(child).ToInt());
	}
}

void CGlowObjectManager::GlowObjectDefinition_t::DrawModel()
{
	C_BaseEntity* const ent = m_hEntity.Get();
	if (ent)
	{
		ent->DrawModel(STUDIO_RENDER);

		// Draw all move children
		{
			auto found = s_MoveChildren.find(EHANDLE(ent).ToInt());
			if (found != s_MoveChildren.end())
			{
				for (int handleInt : found->second)
					EHANDLE::FromIndex(handleInt)->DrawModel(STUDIO_RENDER);
			}
		}

		C_BaseEntity *pAttachment = ent->FirstMoveChild();
		while (pAttachment != NULL)
		{
			if (!s_LocalGlowObjectManager->HasGlowEffect(pAttachment) && pAttachment->ShouldDraw())
			{
				pAttachment->DrawModel(STUDIO_RENDER);
			}
			pAttachment = pAttachment->NextMovePeer();
		}
	}
}

static void DrawGlowAlways(CUtlVector<CGlowObjectManager::GlowObjectDefinition_t>& glowObjectDefinitions,
	int nSplitScreenSlot, CMatRenderContextPtr& pRenderContext)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	ShaderStencilState_t stencilState;
	stencilState.m_bEnable = true;
	stencilState.m_nReferenceValue = 1;
	stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
	stencilState.m_PassOp = STENCILOPERATION_REPLACE;
	stencilState.m_FailOp = STENCILOPERATION_KEEP;
	stencilState.m_ZFailOp = STENCILOPERATION_REPLACE;
	stencilState.SetStencilState(pRenderContext);

	pRenderContext->OverrideDepthEnable(false, false);
	render->SetBlend(1);
	for (int i = 0; i < glowObjectDefinitions.Count(); i++)
	{
		auto& current = glowObjectDefinitions[i];
		if (current.IsUnused() || !current.ShouldDraw(nSplitScreenSlot) || !current.m_bRenderWhenOccluded || !current.m_bRenderWhenUnoccluded)
			continue;

		static ConVarRef ce_graphics_glow_intensity("ce_graphics_glow_intensity");
		const Vector vGlowColor = current.m_vGlowColor * (current.m_flGlowAlpha * ce_graphics_glow_intensity.GetFloat());
		render->SetColorModulation(vGlowColor.Base()); // This only sets rgb, not alpha

		current.DrawModel();
	}
}

static ConVar glow_outline_effect_stencil_mode("glow_outline_effect_stencil_mode", "0", 0,
	"\n\t0: Draws partially occluded glows in a more 3d-esque way, making them look more like they're actually surrounding the model."
	"\n\t1: Draws partially occluded glows in a more 2d-esque way, which can make them more visible.",
	true, 0, true, 1);

static void DrawGlowOccluded(CUtlVector<CGlowObjectManager::GlowObjectDefinition_t>& glowObjectDefinitions,
	int nSplitScreenSlot, CMatRenderContextPtr& pRenderContext)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
#if ADDED_OVERRIDE_DEPTH_FUNC	// Enable this when the TF2 team has added IMatRenderContext::OverrideDepthFunc or similar.
	ShaderStencilState_t stencilState;
	stencilState.m_bEnable = true;
	stencilState.m_nReferenceValue = 1;
	stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
	stencilState.m_PassOp = glow_outline_effect_stencil_mode.GetBool() ? STENCILOPERATION_KEEP : STENCILOPERATION_REPLACE;
	stencilState.m_FailOp = STENCILOPERATION_KEEP;
	stencilState.m_ZFailOp = STENCILOPERATION_REPLACE;
	stencilState.SetStencilState(pRenderContext);

	pRenderContext->OverrideDepthEnable(true, false);

	// Not implemented, we need this feature to be able to do this in 1 pass. Otherwise,
	// we'd have to do 2 passes, 1st to mark on the stencil where the depth test failed,
	// 2nd to actually utilize that information and draw color there.
	pRenderContext->OverrideDepthFunc(true, SHADER_DEPTHFUNC_NEARER);

	for (int i = 0; i < glowObjectDefinitions.Count(); i++)
	{
		auto& current = glowObjectDefinitions[i];
		if (current.IsUnused() || !current.ShouldDraw(nSplitScreenSlot) || !current.m_bRenderWhenOccluded || current.m_bRenderWhenUnoccluded)
			continue;

		render->SetBlend(current.m_flGlowAlpha);
		Vector vGlowColor = current.m_vGlowColor * current.m_flGlowAlpha;
		render->SetColorModulation(&vGlowColor[0]); // This only sets rgb, not alpha

		current.DrawModel();
	}

	pRenderContext->OverrideDepthFunc(false, SHADER_DEPTHFUNC_NEAREROREQUAL)
#else	// 2-pass as a proof of concept so I can take a nice screenshot.
	pRenderContext->OverrideDepthEnable(true, false);

	ShaderStencilState_t stencilState;
	stencilState.m_bEnable = true;
	stencilState.m_nReferenceValue = 2;
	stencilState.m_nWriteMask = 2;
	stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
	stencilState.m_PassOp = STENCILOPERATION_REPLACE;
	stencilState.m_FailOp = STENCILOPERATION_KEEP;
	stencilState.m_ZFailOp = STENCILOPERATION_KEEP;
	stencilState.SetStencilState(pRenderContext);

	// Draw depthtest-passing pixels to the stencil buffer
	{
		render->SetBlend(0);
		pRenderContext->OverrideAlphaWriteEnable(true, false);
		pRenderContext->OverrideColorWriteEnable(true, false);

		for (int i = 0; i < glowObjectDefinitions.Count(); i++)
		{
			auto& current = glowObjectDefinitions[i];
			if (current.IsUnused() || !current.ShouldDraw(nSplitScreenSlot) || !current.m_bRenderWhenOccluded || current.m_bRenderWhenUnoccluded)
				continue;

			current.DrawModel();
		}
	}

	pRenderContext->OverrideAlphaWriteEnable(true, true);
	pRenderContext->OverrideColorWriteEnable(true, true);

	pRenderContext->OverrideDepthEnable(false, false);

	stencilState.m_bEnable = true;
	stencilState.m_nReferenceValue = 3;
	stencilState.m_nTestMask = 2;
	stencilState.m_nWriteMask = 1;
	stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_NOTEQUAL;
	stencilState.m_PassOp = STENCILOPERATION_REPLACE;
	stencilState.m_ZFailOp = STENCILOPERATION_REPLACE;
	stencilState.m_FailOp = glow_outline_effect_stencil_mode.GetBool() ? STENCILOPERATION_KEEP : STENCILOPERATION_REPLACE;
	stencilState.SetStencilState(pRenderContext);

	// Draw color+alpha, stenciling out pixels from the first pass
	render->SetBlend(1);
	for (int i = 0; i < glowObjectDefinitions.Count(); i++)
	{
		auto& current = glowObjectDefinitions[i];
		if (current.IsUnused() || !current.ShouldDraw(nSplitScreenSlot) || !current.m_bRenderWhenOccluded || current.m_bRenderWhenUnoccluded)
			continue;

		static ConVarRef ce_graphics_glow_intensity("ce_graphics_glow_intensity");
		const Vector vGlowColor = current.m_vGlowColor * (current.m_flGlowAlpha * ce_graphics_glow_intensity.GetFloat());
		render->SetColorModulation(vGlowColor.Base()); // This only sets rgb, not alpha

		current.DrawModel();
	}
#endif
}

static void DrawGlowVisible(CUtlVector<CGlowObjectManager::GlowObjectDefinition_t>& glowObjectDefinitions,
	int nSplitScreenSlot, CMatRenderContextPtr& pRenderContext)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	ShaderStencilState_t stencilState;
	stencilState.m_bEnable = true;
	stencilState.m_nReferenceValue = 1;
	stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
	stencilState.m_PassOp = STENCILOPERATION_REPLACE;
	stencilState.m_FailOp = STENCILOPERATION_KEEP;
	stencilState.m_ZFailOp = glow_outline_effect_stencil_mode.GetBool() ? STENCILOPERATION_KEEP : STENCILOPERATION_REPLACE;

	stencilState.SetStencilState(pRenderContext);

	pRenderContext->OverrideDepthEnable(true, false);
	render->SetBlend(1);
	for (int i = 0; i < glowObjectDefinitions.Count(); i++)
	{
		auto& current = glowObjectDefinitions[i];
		if (current.IsUnused() || !current.ShouldDraw(nSplitScreenSlot) || current.m_bRenderWhenOccluded || !current.m_bRenderWhenUnoccluded)
			continue;

		static ConVarRef ce_graphics_glow_intensity("ce_graphics_glow_intensity");
		const Vector vGlowColor = current.m_vGlowColor * (current.m_flGlowAlpha * ce_graphics_glow_intensity.GetFloat());
		render->SetColorModulation(vGlowColor.Base()); // This only sets rgb, not alpha

		current.DrawModel();
	}
}

void CGlowObjectManager::ApplyEntityGlowEffects(const CViewSetup * pSetup, int nSplitScreenSlot, CMatRenderContextPtr & pRenderContext, float flBloomScale, int x, int y, int w, int h)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	const PIXEvent pixEvent(pRenderContext, "ApplyEntityGlowEffects");

	// Optimization: only do all the framebuffer shuffling if there's at least one glow to be drawn
	bool anyGlowAlways = false;
	bool anyGlowOccluded = false;
	bool anyGlowVisible = false;
	{
		for (int i = 0; i < m_GlowObjectDefinitions.Count(); i++)
		{
			auto& current = m_GlowObjectDefinitions[i];
			if (current.IsUnused() || !current.ShouldDraw(nSplitScreenSlot))
				continue;

			anyGlowAlways = anyGlowAlways || current.m_bRenderWhenOccluded && current.m_bRenderWhenUnoccluded;
			anyGlowOccluded = anyGlowOccluded || current.m_bRenderWhenOccluded && !current.m_bRenderWhenUnoccluded;
			anyGlowVisible = anyGlowVisible || !current.m_bRenderWhenOccluded && current.m_bRenderWhenUnoccluded;
			break;
		}

		if (!anyGlowAlways && !anyGlowOccluded && !anyGlowVisible)
			return;	// Early out
	}

	ITexture* const pRtFullFrameFB0 = materials->FindTexture("_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET);
	ITexture* const pRtFullFrameFB1 = materials->FindTexture("_rt_FullFrameFB1", TEXTURE_GROUP_RENDER_TARGET);

	pRenderContext->PushRenderTargetAndViewport();

	// Set backbuffer + hardware depth as MRT 0. We CANNOT CHANGE RENDER TARGETS after this point!!!
	// In CShaderAPIDx8::CreateDepthTexture all depth+stencil buffers are created with the "discard"
	// flag set to TRUE. Not sure about OpenGL, but according to
	// https://msdn.microsoft.com/en-us/library/windows/desktop/bb174356(v=vs.85).aspx, if you change
	// the depth+stencil buffer away from a buffer that has discard=TRUE, the contents become garbage.
	pRenderContext->SetRenderTargetEx(0, nullptr);

	// Save current backbuffer to _rt_FullFrameFB1
	pRenderContext->CopyRenderTargetToTexture(pRtFullFrameFB1);

	// Clear backbuffer color and stencil, keep depth for testing
	pRenderContext->ClearColor4ub(0, 0, 0, 0);
	pRenderContext->ClearBuffers(true, false, true);

	// Draw glow models
	{
		// Save modulation color and blend
		Vector vOrigColor;
		render->GetColorModulation(vOrigColor.Base());
		const float flOrigBlend = render->GetBlend();

		// Set override material for glow color
		g_pStudioRender->ForcedMaterialOverride(materials->FindMaterial("dev/glow_color", TEXTURE_GROUP_OTHER, true), OVERRIDE_BUILD_SHADOWS);
		// HACK: Disable IStudioRender::ForcedMaterialOverride so ubers don't change it away from dev/glow_color
		s_DisableForcedMaterialOverride = true;

		pRenderContext->OverrideColorWriteEnable(true, true);
		pRenderContext->OverrideAlphaWriteEnable(true, true);

		// Build ourselves a map of move children
		BuildMoveChildMap();

		// Draw "glow when visible" objects
		if (anyGlowVisible)
			DrawGlowVisible(m_GlowObjectDefinitions, nSplitScreenSlot, pRenderContext);

		// Draw "glow when occluded" objects
		if (anyGlowOccluded)
			DrawGlowOccluded(m_GlowObjectDefinitions, nSplitScreenSlot, pRenderContext);

		// Draw "glow always" objects
		if (anyGlowAlways)
			DrawGlowAlways(m_GlowObjectDefinitions, nSplitScreenSlot, pRenderContext);

		// Re-enable IStudioRender::ForcedMaterialOverride
		s_DisableForcedMaterialOverride = false;
		// Disable dev/glow_color override
		g_pStudioRender->ForcedMaterialOverride(NULL);

		render->SetColorModulation(vOrigColor.Base());
		render->SetBlend(flOrigBlend);
		pRenderContext->OverrideDepthEnable(false, false);
	}

	pRenderContext->OverrideAlphaWriteEnable(true, true);
	pRenderContext->OverrideColorWriteEnable(true, true);

	// Copy MSAA'd glow models to _rt_FullFrameFB0
	pRenderContext->CopyRenderTargetToTexture(pRtFullFrameFB0);

	// Move original contents of the backbuffer from _rt_FullFrameFB1 to the backbuffer
	{
#if FIXED_COPY_TEXTURE_TO_RENDER_TARGET	// Coordinates don't seem to be mapped 1:1 properly, screen becomes slightly blurry
		pRenderContext->CopyTextureToRenderTargetEx(0, pRtFullFrameFB1, nullptr);
#else
		pRenderContext->SetStencilEnable(false);

		IMaterial* const pFullFrameFB1 = materials->FindMaterial("debug/debugfbtexture1", TEXTURE_GROUP_RENDER_TARGET);
		pFullFrameFB1->AddRef();
		pRenderContext->Bind(pFullFrameFB1);

		const int nSrcWidth = pSetup->width;
		const int nSrcHeight = pSetup->height;
		int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
		pRenderContext->GetViewport(nViewportX, nViewportY, nViewportWidth, nViewportHeight);

		pRenderContext->OverrideDepthEnable(true, false);
		{
			pRenderContext->DrawScreenSpaceRectangle(pFullFrameFB1,
				0, 0, nViewportWidth, nViewportHeight,
				0, 0, nSrcWidth - 1, nSrcHeight - 1,
				pRtFullFrameFB1->GetActualWidth(), pRtFullFrameFB1->GetActualHeight());
		}
		pRenderContext->OverrideDepthEnable(false, false);

		pFullFrameFB1->Release();
#endif
	}

	// Bloom glow models from _rt_FullFrameFB0 to backbuffer while stenciling out inside of models
	{
		static ConVarRef ce_graphics_glow_silhouettes("ce_graphics_glow_silhouettes");

		if (ce_graphics_glow_silhouettes.GetBool())
		{
			IMaterial* const fbTexture0Transluscent = materials->FindMaterial("debug/debugfbtexture0_transluscent", TEXTURE_GROUP_RENDER_TARGET);
			fbTexture0Transluscent->AddRef();
			pRenderContext->Bind(fbTexture0Transluscent);

			const int nSrcWidth = pSetup->width;
			const int nSrcHeight = pSetup->height;
			int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
			pRenderContext->GetViewport(nViewportX, nViewportY, nViewportWidth, nViewportHeight);

			pRenderContext->OverrideDepthEnable(true, false);
			{
				pRenderContext->DrawScreenSpaceRectangle(fbTexture0Transluscent,
					0, 0, nViewportWidth, nViewportHeight,
					0, 0, nSrcWidth - 1, nSrcHeight - 1,
					pRtFullFrameFB1->GetActualWidth(), pRtFullFrameFB1->GetActualHeight());
			}
			pRenderContext->OverrideDepthEnable(false, false);

			fbTexture0Transluscent->Release();
		}
		else
		{
			// Set stencil state
			ShaderStencilState_t stencilState;
			stencilState.m_bEnable = true;
			stencilState.m_nWriteMask = 0; // We're not changing stencil
			stencilState.m_nReferenceValue = 1;
			stencilState.m_nTestMask = 1;
			stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_NOTEQUAL;
			stencilState.m_PassOp = STENCILOPERATION_KEEP;
			stencilState.m_FailOp = STENCILOPERATION_KEEP;
			stencilState.m_ZFailOp = STENCILOPERATION_KEEP;
			stencilState.SetStencilState(pRenderContext);

			ITexture* const pRtQuarterSize1 = materials->FindTexture("_rt_SmallFB1", TEXTURE_GROUP_RENDER_TARGET);
			IMaterial* const pMatHaloAddToScreen = materials->FindMaterial("dev/halo_add_to_screen", TEXTURE_GROUP_OTHER, true);

			// Write to alpha
			pRenderContext->OverrideAlphaWriteEnable(true, true);

			const int nSrcWidth = pSetup->width;
			const int nSrcHeight = pSetup->height;
			int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
			pRenderContext->GetViewport(nViewportX, nViewportY, nViewportWidth, nViewportHeight);

			// Draw quad
			pRenderContext->DrawScreenSpaceRectangle(pMatHaloAddToScreen,
				0, 0, nViewportWidth, nViewportHeight,
				0, 0, nSrcWidth / 4 - 1, nSrcHeight / 4 - 1,
				pRtQuarterSize1->GetActualWidth(),
				pRtQuarterSize1->GetActualHeight());
		}
	}

	// Done with all of our "advanced" 3D rendering.
	pRenderContext->SetStencilEnable(false);
	pRenderContext->OverrideColorWriteEnable(false, false);
	pRenderContext->OverrideAlphaWriteEnable(false, false);
	pRenderContext->OverrideDepthEnable(false, false);

	pRenderContext->PopRenderTargetAndViewport();
}
