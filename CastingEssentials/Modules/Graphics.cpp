#include "Graphics.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"

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

#include <random>

#undef min
#undef max

static CGlowObjectManager* s_LocalGlowObjectManager;

Graphics::Graphics()
{
	ce_graphics_disable_prop_fades = new ConVar("ce_graphics_disable_prop_fades", "0", FCVAR_NONE, "Enable/disable prop fading.");
	ce_graphics_debug_glow = new ConVar("ce_graphics_debug_glow", "0");
	ce_graphics_glow_stencil_final = new ConVar("ce_graphics_glow_stencil_final", "1");

	m_ComputeEntityFadeHook = GetHooks()->AddHook<Global_UTILComputeEntityFade>(std::bind(&Graphics::ComputeEntityFadeOveride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

	m_ApplyEntityGlowEffectsHook = GetHooks()->AddHook<CGlowObjectManager_ApplyEntityGlowEffects>(std::bind(&Graphics::ApplyEntityGlowEffectsOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7, std::placeholders::_8, std::placeholders::_9));
}

Graphics::~Graphics()
{
	if (m_ComputeEntityFadeHook && GetHooks()->RemoveHook<Global_UTILComputeEntityFade>(m_ComputeEntityFadeHook, __FUNCSIG__))
		m_ComputeEntityFadeHook = 0;

	Assert(!m_ComputeEntityFadeHook);
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
	GetHooks()->SetState<CGlowObjectManager_ApplyEntityGlowEffects>(Hooking::HookAction::SUPERCEDE);
	return pThis->ApplyEntityGlowEffects(pSetup, nSplitScreenSlot, pRenderContext, flBloomScale, x, y, w, h);
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
		pRenderContext->SetStencilFailOperation(m_FailOp);
		pRenderContext->SetStencilZFailOperation(m_ZFailOp);
		pRenderContext->SetStencilPassOperation(m_PassOp);
		pRenderContext->SetStencilCompareFunction(m_CompareFunc);
		pRenderContext->SetStencilReferenceValue(m_nReferenceValue);
		pRenderContext->SetStencilTestMask(m_nTestMask);
		pRenderContext->SetStencilWriteMask(m_nWriteMask);
	}
};
void CGlowObjectManager::GlowObjectDefinition_t::DrawModel()
{
	if (m_hEntity.Get())
	{
		m_hEntity->DrawModel(STUDIO_RENDER);
		C_BaseEntity *pAttachment = m_hEntity->FirstMoveChild();

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
	ShaderStencilState_t stencilState;
	stencilState.m_bEnable = true;
	stencilState.m_nReferenceValue = 1;
	stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
	stencilState.m_PassOp = STENCILOPERATION_REPLACE;
	stencilState.m_FailOp = STENCILOPERATION_KEEP;
	stencilState.m_ZFailOp = STENCILOPERATION_REPLACE;
	stencilState.SetStencilState(pRenderContext);

	pRenderContext->OverrideDepthEnable(false, false);
	for (int i = 0; i < glowObjectDefinitions.Count(); i++)
	{
		auto& current = glowObjectDefinitions[i];
		if (current.IsUnused() || !current.ShouldDraw(nSplitScreenSlot) || !current.m_bRenderWhenOccluded || !current.m_bRenderWhenUnoccluded)
			continue;

		render->SetBlend(current.m_flGlowAlpha);
		Vector vGlowColor = current.m_vGlowColor * current.m_flGlowAlpha;
		render->SetColorModulation(&vGlowColor[0]); // This only sets rgb, not alpha

		current.DrawModel();
	}
}

static ConVar glow_outline_effect_stencil_mode("glow_outline_effect_stencil_mode", "0", 0, nullptr, true, 0, true, 1);

static void DrawGlowOccluded(CUtlVector<CGlowObjectManager::GlowObjectDefinition_t>& glowObjectDefinitions,
	int nSplitScreenSlot, CMatRenderContextPtr& pRenderContext)
{
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

	// Don't touch color pixels this pass
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

	pRenderContext->OverrideAlphaWriteEnable(false, true);
	pRenderContext->OverrideColorWriteEnable(false, true);

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

	for (int i = 0; i < glowObjectDefinitions.Count(); i++)
	{
		auto& current = glowObjectDefinitions[i];
		if (current.IsUnused() || !current.ShouldDraw(nSplitScreenSlot) || !current.m_bRenderWhenOccluded || current.m_bRenderWhenUnoccluded)
			continue;

		render->SetBlend(current.m_flGlowAlpha);
		const Vector vGlowColor = current.m_vGlowColor * current.m_flGlowAlpha;
		render->SetColorModulation(vGlowColor.Base()); // This only sets rgb, not alpha

		current.DrawModel();
	}
#endif
}

static void DrawGlowVisible(CUtlVector<CGlowObjectManager::GlowObjectDefinition_t>& glowObjectDefinitions,
	int nSplitScreenSlot, CMatRenderContextPtr& pRenderContext)
{
	ShaderStencilState_t stencilState;
	stencilState.m_bEnable = true;
	stencilState.m_nReferenceValue = 1;
	stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
	stencilState.m_PassOp = STENCILOPERATION_REPLACE;
	stencilState.m_FailOp = STENCILOPERATION_KEEP;
	stencilState.m_ZFailOp = glow_outline_effect_stencil_mode.GetBool() ? STENCILOPERATION_KEEP : STENCILOPERATION_REPLACE;

	stencilState.SetStencilState(pRenderContext);

	pRenderContext->OverrideDepthEnable(true, false);
	for (int i = 0; i < glowObjectDefinitions.Count(); i++)
	{
		auto& current = glowObjectDefinitions[i];
		if (current.IsUnused() || !current.ShouldDraw(nSplitScreenSlot) || current.m_bRenderWhenOccluded || !current.m_bRenderWhenUnoccluded)
			continue;

		render->SetBlend(current.m_flGlowAlpha);
		Vector vGlowColor = current.m_vGlowColor * current.m_flGlowAlpha;
		render->SetColorModulation(&vGlowColor[0]); // This only sets rgb, not alpha

		current.DrawModel();
	}
}

void CGlowObjectManager::RenderGlowModels(const CViewSetup *pSetup, int nSplitScreenSlot, CMatRenderContextPtr &pRenderContext)
{
	//==========================================================================================//
	// This renders solid pixels with the correct coloring for each object that needs the glow.	//
	// After this function returns, this image will then be blurred and added into the frame	//
	// buffer with the objects stenciled out.													//
	//==========================================================================================//
	
	ITexture* const pRtFullFrameFB0 = materials->FindTexture("_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET);
	ITexture* const pRtFullFrameFB1 = materials->FindTexture("_rt_FullFrameFB1", TEXTURE_GROUP_RENDER_TARGET);

	// Update our copy of the framebuffer color (with MSAA!)
	pRenderContext->CopyRenderTargetToTexture(pRtFullFrameFB1);

	pRenderContext->PushRenderTargetAndViewport();

	// Set backbuffer + hardware depth as MRT 0
	pRenderContext->SetRenderTargetEx(0, nullptr);

	// Clear color and stencil
	pRenderContext->ClearColor4ub(0, 0, 0, 0);
	pRenderContext->ClearBuffers(true, false, true);

	// Draw glow models
	{
		// Save modulation color and blend
		Vector vOrigColor;
		render->GetColorModulation(vOrigColor.Base());
		const float flOrigBlend = render->GetBlend();

		// Set override material for glow color
		g_pStudioRender->ForcedMaterialOverride(materials->FindMaterial("dev/glow_color", TEXTURE_GROUP_OTHER, true));

		// Draw "glow always" objects
		pRenderContext->OverrideColorWriteEnable(true, true);
		pRenderContext->OverrideAlphaWriteEnable(true, true);

		// Draw "glow when visible" objects
		DrawGlowVisible(m_GlowObjectDefinitions, nSplitScreenSlot, pRenderContext);

		// Draw "glow when occluded" objects
		DrawGlowOccluded(m_GlowObjectDefinitions, nSplitScreenSlot, pRenderContext);

		// Draw "glow always" objects
		DrawGlowAlways(m_GlowObjectDefinitions, nSplitScreenSlot, pRenderContext);

		g_pStudioRender->ForcedMaterialOverride(NULL);
		render->SetColorModulation(vOrigColor.Base());
		render->SetBlend(flOrigBlend);
		pRenderContext->OverrideDepthEnable(false, false);
	}

	// Copy resolved color to _rt_FullFrameFB0
	pRenderContext->CopyRenderTargetToTexture(pRtFullFrameFB0);

	// Clear backbuffer color, we're going to use it as the target for the final outline
	pRenderContext->ClearColor4ub(0, 0, 0, 0);
	pRenderContext->ClearBuffers(true, false, false);

	// Draw to backbuffer while stenciling out inside of models
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

	// Done with stencil rendering.
	pRenderContext->SetStencilEnable(false);

	// No longer need glow models from _rt_FullFrameFB0, move backbuffer (final glows stenciled out) there
	pRenderContext->CopyRenderTargetToTexture(pRtFullFrameFB0);

	// Move original contents of the backbuffer from _rt_FullFrameFB1 to the backbuffer
	{
#if FIXED_COPY_TEXTURE_TO_RENDER_TARGET	// Coordinates don't seem to be mapped 1:1 properly, screen becomes slightly blurry
		pRenderContext->CopyTextureToRenderTargetEx(0, pRtFullFrameFB1, nullptr);
#else
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

	// Let CGlowObjectManager::ApplyEntityGlowEffects draw final glows from _rt_FullFrameFB0 to the backbuffer,
	// then we're all done!

	pRenderContext->PopRenderTargetAndViewport();
}

void CGlowObjectManager::ApplyEntityGlowEffects(const CViewSetup * pSetup, int nSplitScreenSlot, CMatRenderContextPtr & pRenderContext, float flBloomScale, int x, int y, int w, int h)
{
	//=============================================
	// Render the glow colors to _rt_FullFrameFB 
	//=============================================
	{
		PIXEvent pixEvent(pRenderContext, "RenderGlowModels");
		RenderGlowModels(pSetup, nSplitScreenSlot, pRenderContext);
	}

	// Get viewport
	int const nSrcWidth = pSetup->width;
	int const nSrcHeight = pSetup->height;
	int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
	pRenderContext->GetViewport(nViewportX, nViewportY, nViewportWidth, nViewportHeight);

	ITexture* const pRtFullFrameFB0 = materials->FindTexture("_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET);
	IMaterial* const pGlowCopy = materials->FindMaterial("dev/glow_copy", TEXTURE_GROUP_OTHER);

	if (!IsErrorMaterial(pGlowCopy))
	{
		pGlowCopy->AddRef();
		pRenderContext->Bind(pGlowCopy);
		pRenderContext->DrawScreenSpaceRectangle(pGlowCopy,
			0, 0, nViewportWidth, nViewportHeight,
			0, 0, nSrcWidth - 1, nSrcHeight - 1,
			pRtFullFrameFB0->GetActualWidth(),
			pRtFullFrameFB0->GetActualHeight());
		pGlowCopy->Release();
	}
}
