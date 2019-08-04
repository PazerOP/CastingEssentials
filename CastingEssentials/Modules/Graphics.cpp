#include "Graphics.h"
#include "Controls/StubPanel.h"
#include "Misc/CRefPtrFix.h"
#include "Misc/SuggestionList.h"
#include "Misc/Extras/VPlane.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Entities.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"
#include "Modules/CameraState.h"

#include <bone_setup.h>
#include <debugoverlay_shared.h>
#include <view_shared.h>
#include <materialsystem/itexture.h>
#include <materialsystem/imaterial.h>
#include <materialsystem/imaterialvar.h>
#include <../materialsystem/itextureinternal.h>
#include <../materialsystem/texturemanager.h>
#include <model_types.h>
#include <shaderapi/ishaderapi.h>
#include <tier3/tier3.h>
#include <toolframework/ienginetool.h>
#include <vgui/ISurface.h>
#include <vprof.h>
#include <materialsystem/ishader.h>

#include <algorithm>
#include <random>

#undef min
#undef max

MODULE_REGISTER(Graphics);

EntityOffset<EHANDLE> Graphics::s_MoveParent;
EntityTypeChecker Graphics::s_TFViewModelType;

static constexpr auto STENCIL_INDEX_MASK = 0xFC;

// Should we use a hook to disable IStudioRender::ForcedMaterialOverride?
static bool s_DisableForcedMaterialOverride = false;

Graphics::Graphics() :
	ce_graphics_disable_prop_fades("ce_graphics_disable_prop_fades", "0", FCVAR_NONE, "Enable/disable prop fading.",
		[](IConVar* var, const char*, float) { GetModule()->TogglePropFade(static_cast<ConVar*>(var)); }),
	ce_graphics_debug_glow("ce_graphics_debug_glow", "0", FCVAR_UNREGISTERED),
	ce_graphics_glow_silhouettes("ce_graphics_glow_silhouettes", "0", FCVAR_NONE, "Turns outlines into silhouettes."),
	ce_graphics_improved_glows("ce_graphics_improved_glows", "1", FCVAR_NONE, "Should we used the new and improved glow code?",
		[](IConVar* var, const char*, float) { GetModule()->ToggleImprovedGlows(static_cast<ConVar*>(var)); }),
	ce_graphics_fix_invisible_players("ce_graphics_fix_invisible_players", "1", FCVAR_NONE,
		"Fix a case where players are invisible if you're firstperson speccing them when the round starts."),
	ce_graphics_fix_viewmodel_particles("ce_graphics_fix_viewmodel_particles", "1", FCVAR_NONE,
		"Fix a case where particles on view models(e.g. medic beams and unusual effects) disappear right after loading.",
		[](IConVar* var, const char*, float) { GetModule()->ToggleFixViewmodel(static_cast<ConVar*>(var)); }),

	ce_graphics_fxaa("ce_graphics_fxaa", "0", FCVAR_NONE, "Enables postprocess antialiasing (NVIDIA FXAA 3.11)",
		[](IConVar* var, const char*, float) { GetModule()->ToggleFXAA(static_cast<ConVar*>(var)); }),
	ce_graphics_fxaa_debug("ce_graphics_fxaa_debug", "0"),

	ce_outlines_debug_stencil_out("ce_outlines_debug_stencil_out", "1", FCVAR_NONE, "Should we stencil out the players during the final blend to screen?"),
	ce_outlines_players_override_red("ce_outlines_players_override_red", "", FCVAR_NONE,
		"Override color for red players. [0, 255], format is \"<red> <green> <blue> <alpha>\"."),
	ce_outlines_players_override_blue("ce_outlines_players_override_blue", "", FCVAR_NONE,
		"Override color for blue players. [0, 255], format is \"<red> <green> <blue> <alpha>\"."),
	ce_outlines_blur("ce_outlines_blur", "0", FCVAR_NONE, "Amount of blur to apply to the outlines. <1 to disable.", true, 0, false, 0),
	ce_outlines_expand("ce_outlines_expand", "2.5", FCVAR_NONE, "Radius of the outline effect."),
	ce_outlines_debug("ce_outlines_debug", "0", FCVAR_NONE),
	ce_outlines_spy_visibility("ce_outlines_spy_visibility", "1", FCVAR_NONE,
		"If set to 1, always show outlines around cloaked spies (as opposed to only when they are behind walls)."),
	ce_outlines_cull_frustum("ce_outlines_cull_frustum", "1", FCVAR_HIDDEN, "Enable frustum culling for outlines/infills."),
	ce_outlines_pvs_optimizations("ce_outlines_pvs_optimizations", "1", FCVAR_HIDDEN, "Simplifies glow when occluded/unoccluded outlines for entities based on PVS."),

	ce_infills_enable("ce_infills_enable", "0", FCVAR_NONE, "Enables player infills."),
	ce_infills_additive("ce_infills_additive", "0", FCVAR_NONE, "Enables additive rendering of player infills."),
	ce_infills_hurt_red("ce_infills_hurt_red", "189 59 59 96", FCVAR_NONE, "Infill for red players that are not overhealed."),
	ce_infills_hurt_blue("ce_infills_hurt_blue", "91 122 140 96", FCVAR_NONE, "Infill for blue players that are not overhealed."),
	ce_infills_buffed_red("ce_infills_buffed_red", "240 129 73 0", FCVAR_NONE, "Infill for red players that are overhealed."),
	ce_infills_buffed_blue("ce_infills_buffed_blue", "239 152 73 0", FCVAR_NONE, "Infill for blue players that are overhealed."),
	ce_infills_debug("ce_infills_debug", "0", FCVAR_NONE),
	ce_infills_test("ce_infills_test", []() { GetModule()->ResetPlayerHurtTimes(); }, "Replay the hurt flicker/fades for all players for testing."),

	ce_infills_flicker_hertz("ce_infills_flicker_hertz", "10", FCVAR_NONE, "Infill on-hurt flicker frequency.", true, 0, false, 1),
	ce_infills_flicker_intensity("ce_infills_flicker_intensity", "0.55", FCVAR_NONE, "Infill on-hurt flicker intensity", true, 0, true, 1),
	ce_infills_flicker_after_hurt_time("ce_infills_flicker_after_hurt_time", "1.0", FCVAR_NONE,
		"How long to fade from flickers into normal fades. -1 to disable."),
	ce_infills_flicker_after_hurt_bias("ce_infills_flicker_after_hurt_bias", "0.15", FCVAR_NONE, "Bias amount for flicker fade outs.", true, 0, true, 1),
	ce_infills_fade_after_hurt_time("ce_infills_fade_after_hurt_time", "2.0", FCVAR_NONE, "How long to fade out infills after taking damage. -1 to disable."),
	ce_infills_fade_after_hurt_bias("ce_infills_fade_after_hurt_bias", "0.15", FCVAR_NONE, "Bias amount for infill fade outs.", true, 0, true, 1),

	m_ShaderParamsCallbacks(&DumpShaderParams, &DumpShaderParamsAutocomplete),
	ce_graphics_dump_shader_params("ce_graphics_dump_shader_params", m_ShaderParamsCallbacks, "Prints out all parameters for a given shader.", FCVAR_NONE, m_ShaderParamsCallbacks),
	ce_graphics_dump_rts("ce_graphics_dump_rts", DumpRTs, "Dump all rendertargets to console."),

	m_PostEffectsHook(std::bind(&Graphics::PostEffectsOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5)),

	m_ComputePropOpacityHook(std::bind(&Graphics::ComputePropOpacityOverride, this, std::placeholders::_1, std::placeholders::_2)),
	m_ComputeEntityFadeHook(std::bind(&Graphics::ComputeEntityFadeOveride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)),

	m_ApplyEntityGlowEffectsHook(std::bind(&Graphics::ApplyEntityGlowEffectsOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7, std::placeholders::_8, std::placeholders::_9)),

	m_ForcedMaterialOverrideHook(std::bind(&Graphics::ForcedMaterialOverrideOverride, this, std::placeholders::_1, std::placeholders::_2)),

	m_PostDataUpdateHook(std::bind(&Graphics::PostDataUpdateOverride, this, std::placeholders::_1, std::placeholders::_2)),
	m_ShouldDrawLocalPlayerHook(std::bind(&Graphics::ShouldDrawLocalPlayerOverride, this, std::placeholders::_1))
{
	ToggleImprovedGlows(&ce_graphics_improved_glows);
	ToggleFixViewmodel(&ce_graphics_fix_viewmodel_particles);
}

bool Graphics::IsDefaultParam(const char* paramName)
{
	return
		!stricmp(paramName, "$alpha") ||
		!stricmp(paramName, "$basetexturetransform") ||
		!stricmp(paramName, "$basetexture") ||
		!stricmp(paramName, "$color") ||
		!stricmp(paramName, "$color2") ||
		!stricmp(paramName, "$flags") ||
		!stricmp(paramName, "$flags_defined") ||
		!stricmp(paramName, "$flags2") ||
		!stricmp(paramName, "$flags_defined2") ||
		!stricmp(paramName, "$flashlighttexture") ||
		!stricmp(paramName, "$flashlighttextureframe") ||
		!stricmp(paramName, "$srgbtint") ||
		!stricmp(paramName, "$frame");
}

void Graphics::DumpShaderParams(const CCommand& cmd)
{
	const auto shaderCount = materials->ShaderCount();
	std::vector<IShader*> shaderList;
	shaderList.resize(shaderCount);
	materials->GetShaders(0, shaderCount, shaderList.data());

	if (cmd.ArgC() < 2)
	{
		// Sort shaders alphabetically
		std::sort(shaderList.begin(), shaderList.end(), [](const IShader* p1, const IShader* p2)
			{ return stricmp(p1->GetName(), p2->GetName()) < 0; });

		Warning("Usage: %s <shader name>\nAvailable shaders:\n", cmd.Arg(0));
		for (int i = 0; i < shaderCount; i++)
			Warning("\t%s\n", shaderList[i]->GetName());

		return;
	}

	// Find the shader
	for (const auto& current : shaderList)
	{
		if (stricmp(current->GetName(), cmd.Arg(1)))
			continue;

		const auto paramCount = current->GetNumParams();

		// Sort parameters alphabetically
		std::vector<int> paramOrder;
		paramOrder.reserve(paramCount);
		{
			for (int p = 0; p < paramCount; p++)
				paramOrder.push_back(p);

			std::sort(paramOrder.begin(), paramOrder.end(),
				[current](int p1, int p2) { return stricmp(current->GetParamName(p1), current->GetParamName(p2)) < 0; });
		}

		for (int p2 = 0; p2 < paramCount; p2++)
		{
			const auto p = paramOrder[p2];

			const char* type;
			switch (current->GetParamType(p))
			{
				case SHADER_PARAM_TYPE_TEXTURE:		type = "Texture";	break;
				case SHADER_PARAM_TYPE_INTEGER:		type = "Int";		break;
				case SHADER_PARAM_TYPE_COLOR:		type = "Color";		break;
				case SHADER_PARAM_TYPE_VEC2:		type = "Vec2";		break;
				case SHADER_PARAM_TYPE_VEC3:		type = "Vec3";		break;
				case SHADER_PARAM_TYPE_VEC4:		type = "Vec4";		break;
				case SHADER_PARAM_TYPE_ENVMAP:		type = "Envmap";	break;
				case SHADER_PARAM_TYPE_FLOAT:		type = "Float";		break;
				case SHADER_PARAM_TYPE_BOOL:		type = "Bool";		break;
				case SHADER_PARAM_TYPE_FOURCC:		type = "FourCC";	break;
				case SHADER_PARAM_TYPE_MATRIX:		type = "Matrix";	break;
				case SHADER_PARAM_TYPE_MATRIX4X2:	type = "Matrix4x2";	break;
				case SHADER_PARAM_TYPE_MATERIAL:	type = "Material";	break;
				case SHADER_PARAM_TYPE_STRING:		type = "String";	break;
				default:							type = "UNKNOWN";	break;
			}

			const char* name = current->GetParamName(p);
			Color clr = IsDefaultParam(name) ? Color(128, 128, 128, 255) : Color(128, 255, 128, 255);
			ConColorMsg(clr, "\t%s (%s): %s - default \"%s\"\n", name, current->GetParamHelp(p), type, current->GetParamDefault(p));
		}

		return;
	}

	Warning("No shader found with the name %s\n", cmd.Arg(1));
}

void Graphics::DumpShaderParamsAutocomplete(const CCommand& partial, CUtlVector<CUtlString>& outSuggestions)
{
	if (partial.ArgC() < 1 || partial.ArgC() > 2)
		return;

	const auto shaderCount = materials->ShaderCount();
	IShader** shaderList = (IShader**)stackalloc(shaderCount * sizeof(*shaderList));
	const auto actualShaderCount = materials->GetShaders(0, shaderCount, shaderList);

	Assert(shaderCount == actualShaderCount);

	const auto arg1Length = strlen(partial[1]);

	SuggestionList<> suggestionsStart, suggestionsAnywhere;
	for (auto it = shaderList; it < shaderList + shaderCount; ++it)
	{
		const auto name = (*it)->GetName();
		if (!strnicmp(partial[1], name, arg1Length))
			suggestionsStart.insert(name);
		else if (stristr(name, partial[1]))
			suggestionsAnywhere.insert(name);
	}

	suggestionsStart.EnsureSorted();
	suggestionsAnywhere.EnsureSorted();

	auto AddSuggestion = [&partial, &outSuggestions](const char* suggestion)
	{
		char buf[512];
		const auto length = sprintf_s(buf, "%s %s", partial[0], suggestion);
		outSuggestions.AddToTail(CUtlString(buf, length));
	};

	for (const auto& suggestion : suggestionsStart)
		AddSuggestion(suggestion);

	for (const auto& suggestion : suggestionsAnywhere)
	{
		if (outSuggestions.Count() >= COMMAND_COMPLETION_MAXITEMS)
			break;

		AddSuggestion(suggestion);
	}
}

void Graphics::DumpRTs(const CCommand& cmd)
{
	auto textureMgr = Interfaces::GetTextureManager();
	if (!textureMgr)
	{
		Warning("[%s]: Unable to get texture manager\n", cmd[0]);
		return;
	}

	int index = -1;
	ITextureInternal* tex;
	int count = 0;
	while ((index = textureMgr->FindNext(index, &tex)) != -1)
	{
		if (!tex->IsRenderTarget())
			continue;

		Msg("\t%s: %ix%i\n", tex->GetName(), tex->GetActualWidth(), tex->GetActualHeight());
		count++;
	}

	Msg("Dumped %i rendertargets.\n", count);
}

void Graphics::ToggleFXAA(const ConVar* var)
{
	m_PostEffectsHook.SetEnabled(var->GetBool());
}

void Graphics::PostEffectsOverride(CViewRender* pThis, int x, int y, int w, int h)
{
	m_PostEffectsHook.SetState(Hooking::HookAction::SUPERCEDE);

	m_PostEffectsHook.GetOriginal()(pThis, x, y, w, h);

	// Always run FXAA after other postproc effects, as they may exhibit shader aliasing
	DrawFXAA(x, y, w, h);
}
void Graphics::DrawFXAA(int x, int y, int w, int h)
{
	CMatRenderContextPtr pRenderContext(materials);
	pRenderContext->PushRenderTargetAndViewport();
	pRenderContext->SetToneMappingScaleLinear(Vector(1, 1, 1));

	ITexture* pRtFullFrameFB0 = materials->FindTexture("_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET);
	ITexture* pRtFullFrameFB1 = materials->FindTexture("_rt_FullFrameFB1", TEXTURE_GROUP_RENDER_TARGET);

	pRenderContext->SetRenderTargetEx(0, nullptr);

	// Save current backbuffer to _rt_FullFrameFB1
	pRenderContext->CopyRenderTargetToTexture(pRtFullFrameFB1);

	// Store luma in alpha of fb1
	{
		CRefPtrFix<IMaterial> lumaToAlphaMaterial(materials->FindMaterial("castingessentials/fxaa/luma_to_alpha", TEXTURE_GROUP_CLIENT_EFFECTS));

		static int frameCount = 0;
		pRenderContext->SetRenderTarget(pRtFullFrameFB0);
		pRenderContext->ClearColor4ub(0, 0, 0, 0);
		pRenderContext->ClearBuffers(true, true, true);

		pRenderContext->OverrideAlphaWriteEnable(true, true);
		{
			pRenderContext->DrawScreenSpaceRectangle(lumaToAlphaMaterial,
				x, y, w, h,
				x, y, x + w - 1, y + h - 1,
				pRtFullFrameFB1->GetActualWidth(), pRtFullFrameFB1->GetActualHeight());
		}
		pRenderContext->OverrideAlphaWriteEnable(false, false);
	}

	// FXAA to backbuffer
	{
		CRefPtrFix<IMaterial> fxaaMaterial(materials->FindMaterial("castingessentials/fxaa/fxaa", TEXTURE_GROUP_CLIENT_EFFECTS));

		fxaaMaterial->FindVar("$C0_X")->SetFloatValue(pRtFullFrameFB1->GetActualWidth());
		fxaaMaterial->FindVar("$C0_Y")->SetFloatValue(pRtFullFrameFB1->GetActualHeight());

		pRenderContext->SetRenderTarget(nullptr);

		if (ce_graphics_fxaa_debug.GetBool())
		{
			static std::default_random_engine s_Random;
			pRenderContext->ClearColor4ub(0, 0, 0, 255);
			pRenderContext->ClearBuffers(true, true, true);
		}

		pRenderContext->DrawScreenSpaceRectangle(fxaaMaterial,
			x, y, w, h,
			x, y, x + w - 1, y + h - 1,
			pRtFullFrameFB0->GetActualWidth(), pRtFullFrameFB0->GetActualHeight());
	}

	pRenderContext->PopRenderTargetAndViewport();
}
void Graphics::TogglePropFade(const ConVar* var)
{
	const bool enabled = var->GetBool();
	m_ComputeEntityFadeHook.SetEnabled(enabled);
	m_ComputePropOpacityHook.SetEnabled(enabled);
}

void Graphics::ComputePropOpacityOverride(const Vector& viewOrigin, float factor)
{
	m_ComputePropOpacityHook.SetState(Hooking::HookAction::SUPERCEDE);
	m_ComputePropOpacityHook.GetOriginal()(viewOrigin, -1);
}

unsigned char Graphics::ComputeEntityFadeOveride(C_BaseEntity* entity, float minDist, float maxDist, float fadeScale)
{
	GetHooks()->SetState<HookFunc::Global_UTILComputeEntityFade>(Hooking::HookAction::SUPERCEDE);
	return std::numeric_limits<unsigned char>::max();
}

void Graphics::ToggleImprovedGlows(const ConVar* var)
{
	const bool enabled = var->GetBool();
	m_ApplyEntityGlowEffectsHook.SetEnabled(enabled);
	m_ForcedMaterialOverrideHook.SetEnabled(enabled);
}

void Graphics::ApplyEntityGlowEffectsOverride(CGlowObjectManager * pThis, const CViewSetup * pSetup, int nSplitScreenSlot, CMatRenderContextPtr & pRenderContext, float flBloomScale, int x, int y, int w, int h)
{
	GetHooks()->SetState<HookFunc::CGlowObjectManager_ApplyEntityGlowEffects>(Hooking::HookAction::SUPERCEDE);
	pThis->ApplyEntityGlowEffects(pSetup, nSplitScreenSlot, pRenderContext, flBloomScale, x, y, w, h);
}

void Graphics::ForcedMaterialOverrideOverride(IMaterial* material, OverrideType_t overrideType)
{
	if (s_DisableForcedMaterialOverride)
	{
		GetHooks()->SetState<HookFunc::IStudioRender_ForcedMaterialOverride>(Hooking::HookAction::SUPERCEDE);
		// Do nothing
	}
	else
	{
		GetHooks()->SetState<HookFunc::IStudioRender_ForcedMaterialOverride>(Hooking::HookAction::IGNORE);
	}
}

void Graphics::ToggleFixViewmodel(const ConVar* var)
{
	auto enabled = var->GetBool();
	m_ShouldDrawLocalPlayerHook.SetEnabled(enabled);
	m_PostDataUpdateHook.SetEnabled(enabled);
}

void Graphics::PostDataUpdateOverride(IClientNetworkable* pThis, int updateType)
{
	m_PostDataUpdateHook.SetState(Hooking::HookAction::SUPERCEDE);
	auto weapon = static_cast<C_BaseEntity*>(pThis);
	const auto localOwnerOverride = CreateVariablePusher(m_LocalOwner, weapon ? weapon->GetOwnerEntity() : nullptr);
	m_PostDataUpdateHook.GetOriginal()(pThis, updateType);
}

bool Graphics::ShouldDrawLocalPlayerOverride(C_BasePlayer * pThis)
{
	if (!m_LocalOwner) return false;

	m_ShouldDrawLocalPlayerHook.SetState(Hooking::HookAction::SUPERCEDE);
	auto ret = m_ShouldDrawLocalPlayerHook.GetOriginal()(pThis);
	if (!ret && CameraState::GetModule()->GetLocalObserverMode() == OBS_MODE_IN_EYE) {
		auto specTarget = Player::AsPlayer(CameraState::GetModule()->GetLocalObserverTarget());
		if (specTarget && m_LocalOwner == specTarget->GetEntity()) return true;
	}
	return ret;
}

Graphics::ExtraGlowData* Graphics::FindExtraGlowData(int entindex)
{
	for (auto& extraGlowData : m_ExtraGlowData)
	{
		if (extraGlowData.m_Base->m_hEntity.GetEntryIndex() == entindex)
			return &extraGlowData;
	}

	return nullptr;
}

void Graphics::GetAABBCorner(const Vector& mins, const Vector& maxs, uint_fast8_t cornerIndex, Vector& corner)
{
	Assert(cornerIndex >= 0 && cornerIndex < 8);

	corner.Init(
		cornerIndex & (1 << 2) ? maxs.x : mins.x,
		cornerIndex & (1 << 1) ? maxs.y : mins.y,
		cornerIndex & (1 << 0) ? maxs.z : mins.z);
}

void Graphics::GetRotatedBBCorners(const Vector& origin, const QAngle& angles, const Vector& mins, const Vector& maxs, Vector corners[8])
{
	// Build a rotation matrix from angles
	matrix3x4_t fRotateMatrix;
	AngleMatrix(angles, fRotateMatrix);

	for (uint_fast8_t i = 0; i < 8; ++i)
	{
		Vector unrotated;
		GetAABBCorner(mins, maxs, i, unrotated);

		VectorRotate(unrotated, fRotateMatrix, corners[i]);
		corners[i] += origin;
	}
}

bool Graphics::WorldToScreenMat(const VMatrix& worldToScreen, const Vector& world, Vector2D& screen)
{
	float w = worldToScreen[3][0] * world[0] + worldToScreen[3][1] * world[1] + worldToScreen[3][2] * world[2] + worldToScreen[3][3];
	if (w < 0.001f)
		return false;

	float invW = 1 / w;

	screen.Init(
		(worldToScreen[0][0] * world[0] + worldToScreen[0][1] * world[1] + worldToScreen[0][2] * world[2] + worldToScreen[0][3]) * invW,
		(worldToScreen[1][0] * world[0] + worldToScreen[1][1] * world[1] + worldToScreen[1][2] * world[2] + worldToScreen[1][3]) * invW);

	// Transform [-1, 1] coordinates to actual screen pixel coordinates
	screen.x = 0.5f * (screen.x + 1) * m_View->width + m_View->x;
	screen.y = 0.5f * (screen.y + 1) * m_View->height + m_View->y;

	return true;
}

bool Graphics::Test_PlaneHitboxesIntersect(C_BaseAnimating* animating, const Frustum_t& viewFrustum,
	const VMatrix& worldToScreen, Vector2D& screenMins, Vector2D& screenMaxs)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);

	CStudioHdr *pStudioHdr = animating->GetModelPtr();
	if (!pStudioHdr)
		return false;

	mstudiohitboxset_t *set = pStudioHdr->pHitboxSet(animating->m_nHitboxSet);
	if (!set || !set->numhitboxes)
		return false;

	CBoneCache *pCache = animating->GetBoneCache(pStudioHdr);
	matrix3x4_t *hitboxbones[MAXSTUDIOBONES];
	pCache->ReadCachedBonePointers(hitboxbones, pStudioHdr->numbones());

	screenMins.Init(std::numeric_limits<vec_t>::max(), std::numeric_limits<vec_t>::max());
	screenMaxs.Init(-std::numeric_limits<vec_t>::max(), -std::numeric_limits<vec_t>::max());

	Vector minsBB0, minsBB1, minsOrigin, minsCorner, maxsBB0, maxsBB1, maxsOrigin, maxsCorner;
	QAngle minsAngles, maxsAngles;

	Vector forward, right, up;
	AngleVectors(m_View->angles, &forward, &right, &up);

	const VPlane viewPlaneX = VPlaneInit(up, m_View->origin);
	const VPlane viewPlaneY = VPlaneInit(right, m_View->origin);
	const VPlane viewPlaneZ = VPlaneInit(forward, m_View->origin);

	uint32_t validCount = 0;
	for (int i = 0; i < set->numhitboxes; i++)
	{
		mstudiobbox_t *pbox = set->pHitbox(i);

		const Vector bboxMins = pbox->bbmin * animating->GetModelScale();
		const Vector bboxMaxs = pbox->bbmax * animating->GetModelScale();

		Vector bonePos;
		QAngle boneAngles;
		MatrixAngles(*hitboxbones[pbox->bone], boneAngles, bonePos);

		Vector bbCorners[8];
		GetRotatedBBCorners(bonePos, boneAngles, bboxMins, bboxMaxs, bbCorners);

		for (uint_fast8_t c = 0; c < 8; c++)
		{
			const auto& corner = bbCorners[c];

			const auto leftPlane = VPlaneInit(*viewFrustum.GetPlane(FRUSTUM_LEFT));
			const auto rightPlane = VPlaneInit(*viewFrustum.GetPlane(FRUSTUM_RIGHT));
			const auto topPlane = VPlaneInit(*viewFrustum.GetPlane(FRUSTUM_TOP));
			const auto bottomPlane = VPlaneInit(*viewFrustum.GetPlane(FRUSTUM_BOTTOM));

			// Check if the corner is beyond any of the four surrounding planes of our view frustum
			// All of the planes are facing inward, so SIDE_BACK means its outside of the frustum
			const auto sideLeft = leftPlane.GetPointSide(corner);
			const auto sideRight = rightPlane.GetPointSide(corner);
			const auto sideTop = topPlane.GetPointSide(corner);
			const auto sideBottom = bottomPlane.GetPointSide(corner);

			bool yCalculated = false;
			Vector2D screenPos;

			if (sideLeft == SIDE_BACK || sideRight == SIDE_BACK)
			{
				// Screen X comes from corner snapped to screen horizontal plane
				{
					const auto& snappedCorner = viewPlaneX.SnapPointToPlane(corner);
					if (!WorldToScreenMat(worldToScreen, snappedCorner, screenPos))
						continue;
				}

				// Screen Y comes from point snapped to left or right frustum plane
				{
					const auto& plane = sideLeft == SIDE_BACK ? leftPlane : rightPlane;
					const auto& snappedCorner = plane.SnapPointToPlane(corner);

					const float screenPosX = screenPos.x;	// Save this
					if (!WorldToScreenMat(worldToScreen, snappedCorner, screenPos))
						continue;

					screenPos.x = screenPosX;	// Restore this
					yCalculated = true;
				}
			}
			if (sideTop == SIDE_BACK || sideBottom == SIDE_BACK)
			{
				// Screen Y comes from corner snapped to screen vertical plane
				if (!yCalculated)
				{
					const auto& snappedCorner = viewPlaneY.SnapPointToPlane(corner);
					if (!WorldToScreenMat(worldToScreen, snappedCorner, screenPos))
						continue;
				}

				// Screen X comes from point snapped to left or right frustum plane
				{
					const auto& plane = sideTop == SIDE_BACK ? topPlane : bottomPlane;
					const auto& snappedCorner = plane.SnapPointToPlane(corner);

					const float screenPosY = screenPos.y;	// Save this
					if (!WorldToScreenMat(worldToScreen, snappedCorner, screenPos))
						continue;

					screenPos.y = screenPosY;	// Restore this
				}
			}
			if (sideLeft != SIDE_BACK && sideRight != SIDE_BACK && sideTop != SIDE_BACK && sideBottom != SIDE_BACK)
			{
				if (!WorldToScreenMat(worldToScreen, corner, screenPos))
					continue;
			}

			Vector2DMin(screenMins, screenPos, screenMins);
			Vector2DMax(screenMaxs, screenPos, screenMaxs);
			validCount++;
		}
	}

	return validCount > 0;
}

float Graphics::ApplyInfillTimeEffects(float lastHurtTime)
{
	// Fade the whole thing out
	const auto fadeProgress = ce_infills_fade_after_hurt_time.GetFloat() > 0 ?
		RemapValClamped(lastHurtTime, 0, ce_infills_fade_after_hurt_time.GetFloat(), 0, 1) : 0;

	const float fade = 1 - Bias(fadeProgress, ce_infills_fade_after_hurt_bias.GetFloat());

	const auto flickerProgress = ce_infills_flicker_after_hurt_time.GetFloat() > 0 ?
		RemapValClamped(lastHurtTime, 0, ce_infills_flicker_after_hurt_time.GetFloat(), 0, 1) : 0;

	const float flicker = Lerp(ce_infills_flicker_intensity.GetFloat(), fade,
		std::cos(2 * lastHurtTime * M_PI_F * ce_infills_flicker_hertz.GetFloat()) * 0.5f + 0.5f);

	const float combined = Lerp(Bias(flickerProgress, ce_infills_flicker_after_hurt_bias.GetFloat()), flicker, fade);

	return combined;
}

void Graphics::EnforceSpyVisibility(Player& player, CGlowObjectManager::GlowObjectDefinition_t& outline) const
{
	if (ce_outlines_spy_visibility.GetBool())
		outline.m_bRenderWhenUnoccluded = player.CheckCondition(TFCond::TFCond_Cloaked);
}

#include <client/view_scene.h>
void Graphics::BuildExtraGlowData(CGlowObjectManager* glowMgr, bool& anyAlways, bool& anyOccluded, bool& anyUnoccluded)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);

	m_ExtraGlowData.clear();
	anyAlways = false;
	anyOccluded = false;
	anyUnoccluded = false;

	bool hasRedOverride, hasBlueOverride;
	Vector redOverride = ColorToVector(ColorFromConVar(ce_outlines_players_override_red, &hasRedOverride));
	Vector blueOverride = ColorToVector(ColorFromConVar(ce_outlines_players_override_blue, &hasBlueOverride));

	uint8_t stencilIndex = 0;

	const bool infillsEnable = ce_infills_enable.GetBool();
	const bool bInfillDebug = ce_infills_debug.GetBool();

	const Color redInfillNormal = ColorFromConVar(ce_infills_hurt_red);
	const Color blueInfillNormal = ColorFromConVar(ce_infills_hurt_blue);
	const Color redInfillBuffed = ColorFromConVar(ce_infills_buffed_red);
	const Color blueInfillBuffed = ColorFromConVar(ce_infills_buffed_blue);

	Frustum_t viewFrustum;
	GeneratePerspectiveFrustum(m_View->origin, m_View->angles, m_View->zNear, m_View->zFar, m_View->fov, m_View->m_flAspectRatio, viewFrustum);

	VMatrix worldToScreen;
	if (infillsEnable)
		Interfaces::GetEngineTool()->GetWorldToScreenMatrixForView(*m_View, &worldToScreen);

	for (int i = 0; i < glowMgr->m_GlowObjectDefinitions.Count(); i++)
	{
		auto& current = glowMgr->m_GlowObjectDefinitions[i];
		if (current.IsUnused())
			continue;

		auto const ent = current.m_hEntity.Get();

		// Frustum culling
		if (ent && ce_outlines_cull_frustum.GetBool())
		{
			Vector renderMins, renderMaxs;
			ent->GetRenderBoundsWorldspace(renderMins, renderMaxs);

			static constexpr Vector BLOAT(16);
			renderMins -= BLOAT;
			renderMaxs += BLOAT;

			if (R_CullBox(renderMins, renderMaxs, viewFrustum))
				continue;    // Failed frustum cull
		}

		m_ExtraGlowData.emplace_back(&current);
		auto& currentExtra = m_ExtraGlowData.back();

		// Glow mode
		{
			// Simplify glow mode if statements later on
			currentExtra.m_Mode = GlowMode::Never;
			if (currentExtra.m_Base->m_bRenderWhenOccluded)
				*(std::underlying_type_t<GlowMode>*)(&currentExtra.m_Mode) |= (int)GlowMode::Occluded;
			if (currentExtra.m_Base->m_bRenderWhenUnoccluded)
				*(std::underlying_type_t<GlowMode>*)(&currentExtra.m_Mode) |= (int)GlowMode::Unoccluded;

			// Check PVS (if we would benefit from it)
			if (ce_outlines_pvs_optimizations.GetBool() &&
				currentExtra.m_Mode != GlowMode::Always &&
				(ent && !ClientLeafSystem()->IsRenderableInPVS(ent)))
			{
				if (currentExtra.m_Mode == GlowMode::Occluded)
				{
					currentExtra.m_Mode = GlowMode::Always;
				}
				else if (currentExtra.m_Mode == GlowMode::Unoccluded)
				{
					// Kind of a waste since we emplace on then pop off, but this is the slightly
					// less common path, and so we save on (usually) not having to copy the struct
					// into the vector.
					m_ExtraGlowData.pop_back();
					continue;
				}
			}

			// Update glow bucket status
			switch (currentExtra.m_Mode)
			{
				case GlowMode::Always:
					anyAlways = true;
					break;
				case GlowMode::Occluded:
					anyOccluded = true;
					break;
				case GlowMode::Unoccluded:
					anyUnoccluded = true;
					break;
			}
		}

		if (Player::IsValidIndex(current.m_hEntity.GetEntryIndex()))
		{
			auto player = Player::GetPlayer(current.m_hEntity.GetEntryIndex(), __FUNCTION__);
			if (player)
			{
				auto team = player->GetTeam();
				if (team == TFTeam::Red || team == TFTeam::Blue)
				{
					// Kind of a hack, this being here
					EnforceSpyVisibility(*player, current);

					if (infillsEnable)
					{
						Vector worldMins, worldMaxs;
						Vector2D screenMins, screenMaxs;

						if (Test_PlaneHitboxesIntersect(player->GetBaseAnimating(), viewFrustum, worldToScreen, screenMins, screenMaxs))
						{
							auto& hurtInfill = currentExtra.m_Infills[(size_t)InfillType::Hurt];
							auto& buffedInfill = currentExtra.m_Infills[(size_t)InfillType::Buffed];

							currentExtra.m_StencilIndex = ++stencilIndex;

							if (currentExtra.m_StencilIndex >= (1 << 6))
								PluginWarning("Ran out of stencil indices for players???");

							const auto& playerHealth = player->GetHealth();
							const auto& playerMaxHealth = player->GetMaxHealth();
							const auto healthPercentage = playerHealth / (float)playerMaxHealth;
							const auto overhealPercentage = RemapValClamped(playerHealth, playerMaxHealth, int(playerMaxHealth * 1.5 / 5) * 5, 0, 1);

							if (overhealPercentage <= 0)
							{
								if (healthPercentage != 1)
								{
									if (bInfillDebug)
										hurtInfill.m_RectMin = screenMins;
									else
										hurtInfill.m_RectMin.Init();

									hurtInfill.m_Color = team == TFTeam::Red ? redInfillNormal : blueInfillNormal;
									hurtInfill.m_Color.a() *= ApplyInfillTimeEffects(player->GetState<PlayerHealthState>().GetLastHurtTime());	// Infill fading/flickering

									hurtInfill.m_RectMax.Init(
										bInfillDebug ? screenMaxs.x : m_View->width,
										Lerp(healthPercentage, screenMaxs.y, screenMins.y));

									hurtInfill.m_Active = true;
								}
							}
							else
							{
								buffedInfill.m_Active = true;

								buffedInfill.m_RectMax.Init(
									bInfillDebug ? screenMaxs.x : m_View->width,
									Lerp(overhealPercentage, screenMins.y, screenMaxs.y));

								buffedInfill.m_Color = team == TFTeam::Red ? redInfillBuffed : blueInfillBuffed;

								if (bInfillDebug)
									buffedInfill.m_RectMin = screenMins;
								else
								{
									buffedInfill.m_RectMin.Init();

									if (overhealPercentage >= 1)
										buffedInfill.m_RectMax.y = m_View->height;
								}

								hurtInfill.m_RectMin.Init();
								hurtInfill.m_RectMax.Init();
							}
						}
					}

					if (team == TFTeam::Red && hasRedOverride)
					{
						currentExtra.m_GlowColorOverride = redOverride;
						currentExtra.m_ShouldOverrideGlowColor = true;
					}
					else if (team == TFTeam::Blue && hasBlueOverride)
					{
						currentExtra.m_GlowColorOverride = blueOverride;
						currentExtra.m_ShouldOverrideGlowColor = true;
					}
				}
			}
		}
	}

	// If we're doing infills, sort the vector so we render back to front
	if (infillsEnable)
	{
		std::sort(m_ExtraGlowData.begin(), m_ExtraGlowData.end(), [](const ExtraGlowData& lhs, const ExtraGlowData& rhs) -> bool
		{
			const auto& origin = Graphics::GetModule()->m_View->origin;
			const auto& ent1 = lhs.m_Base->m_hEntity.Get();
			const auto& ent2 = rhs.m_Base->m_hEntity.Get();

			const float dist1 = ent1 ? ent1->GetAbsOrigin().DistToSqr(origin) : 0;
			const float dist2 = ent2 ? ent2->GetAbsOrigin().DistToSqr(origin) : 0;

			return dist2 < dist1;
		});
	}

	if (ce_outlines_debug.GetBool())
	{
		int conIndex = 0;
		engine->Con_NPrintf(conIndex++, "Glow object count: %zi/%i", m_ExtraGlowData.size(), glowMgr->m_GlowObjectDefinitions.Count());
		for (int i = 0; i < glowMgr->m_GlowObjectDefinitions.Count(); i++)
		{
			auto& current = glowMgr->m_GlowObjectDefinitions[i];

			int extraIndex = -1;
			for (size_t k = 0; k < m_ExtraGlowData.size(); k++)
			{
				if (m_ExtraGlowData[k].m_Base == &current)
					extraIndex = (int)k;
			}

			const char* prefix = i == glowMgr->m_nFirstFreeSlot ? "FIRST FREE --> " : "";
			if (current.IsUnused())
			{
				//glowMgr->m_GlowObjectDefinitions.Remove(i--);
				engine->Con_NPrintf(conIndex++, "%sUnused: next free %i [%i]", prefix, current.m_nNextFreeSlot, i);
			}
			else
			{
				auto ent = current.m_hEntity.Get();
				Assert(ent);

				char buffer[128];
				sprintf_s(buffer, "Glow object %i (extra %i)", i, extraIndex);
				NDebugOverlay::Text(ent->GetAbsOrigin(), buffer, false, NDEBUG_PERSIST_TILL_NEXT_FRAME);
				engine->Con_NPrintf(conIndex++, "%sGlow object: %s [i %i, extra %i, entindex %i]", prefix, ent->GetClientClass()->GetName(), i, extraIndex, ent->entindex());
			}
		}

		if (glowMgr->m_nFirstFreeSlot == CGlowObjectManager::GlowObjectDefinition_t::END_OF_FREE_LIST)
			engine->Con_NPrintf(conIndex++, "=== NEW ELEMENT REQUIRED ===");
	}

	// Build ourselves a map of move children (we need extra glow data before we can do this)
	BuildMoveChildLists();
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

void Graphics::DrawInfills(CMatRenderContextPtr& pRenderContext)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);

	CRefPtrFix<IMaterial> infillMaterial(materials->FindMaterial(
		ce_infills_additive.GetBool() ? "vgui/white_additive" : "vgui/white",
		TEXTURE_GROUP_OTHER, true));

	CMeshBuilder meshBuilder;

#define DRAW_INFILL_VGUI 0

	constexpr float pixelOffset = 0.5;

	pRenderContext->MatrixMode(MATERIAL_PROJECTION);
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	pRenderContext->Scale(1, -1, 1);
	pRenderContext->Ortho(pixelOffset, pixelOffset, m_View->width + pixelOffset, m_View->height + pixelOffset, -1.0f, 1.0f);

	// make sure there is no translation and rotation laying around
	pRenderContext->MatrixMode(MATERIAL_MODEL);
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode(MATERIAL_VIEW);
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	for (const auto& currentExtra : m_ExtraGlowData)
	{
		if (!currentExtra.AnyInfillsActive())
			continue;

		ShaderStencilState_t stencilState;
		stencilState.m_bEnable = !ce_infills_debug.GetBool();
		stencilState.m_nReferenceValue = (currentExtra.m_StencilIndex << 2) | 1;
		stencilState.m_nTestMask = 0xFFFFFFFD;
		stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_EQUAL;
		stencilState.m_PassOp = STENCILOPERATION_KEEP;
		stencilState.m_FailOp = STENCILOPERATION_KEEP;
		stencilState.m_ZFailOp = STENCILOPERATION_KEEP;
		stencilState.SetStencilState(pRenderContext);

		//const auto& avg = VectorAvg(currentExtra.m_InfillMaxsWorld - currentExtra.m_InfillMinsWorld);
		auto mesh = pRenderContext->GetDynamicMesh(true, nullptr, nullptr, infillMaterial);

		meshBuilder.Begin(mesh, MATERIAL_QUADS, currentExtra.m_Infills.size());

		for (size_t i = 0; i < currentExtra.m_Infills.size(); i++)
		{
			const auto& infill = currentExtra.m_Infills[i];
			if (!infill.m_Active)
				continue;

			// Upper left
			meshBuilder.Position3f(infill.m_RectMin.x, m_View->height - infill.m_RectMin.y, 0);
			meshBuilder.Color4ubv(infill.m_Color.GetRawColorPtr());
			meshBuilder.TexCoord2f(0, 0, 0);
			meshBuilder.AdvanceVertex();

			// Lower left
			meshBuilder.Position3f(infill.m_RectMin.x, m_View->height - infill.m_RectMax.y, 0);
			meshBuilder.Color4ubv(infill.m_Color.GetRawColorPtr());
			meshBuilder.TexCoord2f(0, 0, 1);
			meshBuilder.AdvanceVertex();

			// Lower right
			meshBuilder.Position3f(infill.m_RectMax.x, m_View->height - infill.m_RectMax.y, 0);
			meshBuilder.Color4ubv(infill.m_Color.GetRawColorPtr());
			meshBuilder.TexCoord2f(0, 1, 1);
			meshBuilder.AdvanceVertex();

			// Upper right
			meshBuilder.Position3f(infill.m_RectMax.x, m_View->height - infill.m_RectMin.y, 0);
			meshBuilder.Color4ubv(infill.m_Color.GetRawColorPtr());
			meshBuilder.TexCoord2f(0, 1, 0);
			meshBuilder.AdvanceVertex();
		}

		meshBuilder.End();
		mesh->Draw();
	}

	pRenderContext->MatrixMode(MATERIAL_PROJECTION);
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode(MATERIAL_MODEL);
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode(MATERIAL_VIEW);
	pRenderContext->PopMatrix();
}

void Graphics::ResetPlayerHurtTimes()
{
	for (Player* player : Player::Iterable())
		player->GetState<PlayerHealthState>().ResetLastHurtTime();
}

void Graphics::BuildMoveChildLists()
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);

	IClientEntityList* const entityList = Interfaces::GetClientEntityList();
	for (int i = 0; i < entityList->GetHighestEntityIndex(); i++)
	{
		IClientEntity* clientEnt = entityList->GetClientEntity(i);
		if (!clientEnt)
			continue;

		C_BaseEntity* child = clientEnt->GetBaseEntity();
		if (!child || !child->ShouldDraw())
			continue;

		if (auto childAnimating = child->GetBaseAnimating())
		{
			if (childAnimating->IsViewModel() || s_TFViewModelType.Match(childAnimating))
				continue;
		}

		const auto& moveparent = s_MoveParent.TryGetValue(child);
		if (!moveparent || !moveparent->IsValid())
			continue;

		auto extraGlowData = FindExtraGlowData(moveparent->GetEntryIndex());
		if (extraGlowData)
			extraGlowData->m_MoveChildren.push_back(child);
	}
}

static Vector GetColorModulation()
{
	Vector color;
	render->GetColorModulation(color.Base());
	return color;
}

void CGlowObjectManager::GlowObjectDefinition_t::DrawModel()
{
	C_BaseEntity* const ent = m_hEntity.Get();
	if (ent)
	{
		const auto& extra = Graphics::GetModule()->FindExtraGlowData(m_hEntity.GetEntryIndex());
		if (!extra)
		{
			PluginWarning("Unable to find extra glow data for entity %i", m_hEntity.GetEntryIndex());
			return;
		}

		extra->ApplyGlowColor();
		const auto initialColor = GetColorModulation();

		// Draw ourselves
		ent->DrawModel(STUDIO_RENDER);
		AssertMsg(initialColor == GetColorModulation(), "Color mismatch after drawing %s", ent->GetClientClass()->GetName());

		// Draw all move children
		for (auto moveChild : extra->m_MoveChildren)
		{
			moveChild->DrawModel(STUDIO_RENDER);
			AssertMsg(initialColor == GetColorModulation(), "Color mismatch after drawing %s", moveChild->GetClientClass()->GetName());
		}

		C_BaseEntity *pAttachment = ent->FirstMoveChild();
		while (pAttachment != NULL)
		{
			if (pAttachment->ShouldDraw())
			{
				pAttachment->DrawModel(STUDIO_RENDER);
				AssertMsg(initialColor == GetColorModulation(), "Color mismatch after drawing %s", pAttachment->GetClientClass()->GetName());
			}

			pAttachment = pAttachment->NextMovePeer();
		}

		// Did Valve "break" something?
		Assert(initialColor == GetColorModulation());
	}
}

void Graphics::DrawGlowAlways(int nSplitScreenSlot, CMatRenderContextPtr& pRenderContext) const
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	ShaderStencilState_t stencilState;
	stencilState.m_bEnable = true;
	stencilState.m_nReferenceValue = 1;
	stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
	stencilState.m_PassOp = STENCILOPERATION_REPLACE;
	stencilState.m_ZFailOp = STENCILOPERATION_REPLACE;
	stencilState.SetStencilState(pRenderContext);

	pRenderContext->OverrideColorWriteEnable(true, true);
	pRenderContext->OverrideAlphaWriteEnable(true, true);
	pRenderContext->OverrideDepthEnable(false, false);
	for (const auto& current : m_ExtraGlowData)
	{
		if (current.m_Mode != GlowMode::Always || current.m_Base->IsUnused() || !current.m_Base->ShouldDraw(nSplitScreenSlot))
			continue;

		render->SetBlend(current.m_Base->m_flGlowAlpha);

		if (current.AnyInfillsActive())
		{
			pRenderContext->SetStencilWriteMask(0xFFFFFFFF);
			pRenderContext->SetStencilReferenceValue((current.m_StencilIndex << 2) | 1);
		}
		else
			pRenderContext->SetStencilWriteMask(1);

		current.m_Base->DrawModel();
	}
}

static ConVar glow_outline_effect_stencil_mode("glow_outline_effect_stencil_mode", "0", 0,
	"\n\t0: Draws partially occluded glows in a more 3d-esque way, making them look more like they're actually surrounding the model."
	"\n\t1: Draws partially occluded glows in a more 2d-esque way, which can make them more visible.",
	true, 0, true, 1);

void Graphics::DrawGlowOccluded(int nSplitScreenSlot, CMatRenderContextPtr& pRenderContext) const
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
	pRenderContext->OverrideDepthFunc(true, SHADER_DEPTHFUNC_FARTHER);

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

	ShaderStencilState_t stencilState;
	stencilState.m_bEnable = true;
	stencilState.m_nReferenceValue = 2;
	stencilState.m_nWriteMask = 2;
	stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
	stencilState.m_PassOp = STENCILOPERATION_REPLACE;
	stencilState.m_ZFailOp = STENCILOPERATION_KEEP;
	stencilState.SetStencilState(pRenderContext);

	// Draw depthtest-passing pixels to the stencil buffer
	{
		render->SetBlend(0);
		pRenderContext->OverrideAlphaWriteEnable(true, false);
		pRenderContext->OverrideColorWriteEnable(true, false);

		for (const auto& current : m_ExtraGlowData)
		{
			if (current.m_Mode != GlowMode::Occluded || current.m_Base->IsUnused() || !current.m_Base->ShouldDraw(nSplitScreenSlot))
				continue;

			pRenderContext->OverrideDepthEnable(true, false);
			current.m_Base->DrawModel();
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
	for (const auto& current : m_ExtraGlowData)
	{
		if (current.m_Mode != GlowMode::Occluded || current.m_Base->IsUnused() || !current.m_Base->ShouldDraw(nSplitScreenSlot))
			continue;

		render->SetBlend(current.m_Base->m_flGlowAlpha);

		if (current.AnyInfillsActive())
		{
			pRenderContext->SetStencilWriteMask(0xFFFFFFFF);
			pRenderContext->SetStencilReferenceValue((current.m_StencilIndex << 2) | 3);
		}
		else
			pRenderContext->SetStencilWriteMask(1);

		current.m_Base->DrawModel();
	}
#endif
}

void Graphics::DrawGlowVisible(int nSplitScreenSlot, CMatRenderContextPtr& pRenderContext) const
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	ShaderStencilState_t stencilState;
	stencilState.m_bEnable = true;
	stencilState.m_nReferenceValue = 1;
	stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
	stencilState.m_PassOp = STENCILOPERATION_REPLACE;
	stencilState.m_ZFailOp = glow_outline_effect_stencil_mode.GetBool() ? STENCILOPERATION_KEEP : STENCILOPERATION_REPLACE;

	stencilState.SetStencilState(pRenderContext);

	pRenderContext->OverrideDepthEnable(true, false);
	for (const auto& current : m_ExtraGlowData)
	{
		if (current.m_Mode != GlowMode::Unoccluded || current.m_Base->IsUnused() || !current.m_Base->ShouldDraw(nSplitScreenSlot))
			continue;

		render->SetBlend(current.m_Base->m_flGlowAlpha);

		if (current.AnyInfillsActive())
		{
			pRenderContext->SetStencilWriteMask(0xFFFFFFFF);
			pRenderContext->SetStencilReferenceValue((current.m_StencilIndex << 2) | 1);
		}
		else
			pRenderContext->SetStencilWriteMask(1);

		current.m_Base->DrawModel();
	}
}

void CGlowObjectManager::ApplyEntityGlowEffects(const CViewSetup* pSetup, int nSplitScreenSlot, CMatRenderContextPtr& pRenderContext, float flBloomScale, int x, int y, int w, int h)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	const PIXEvent pixEvent(pRenderContext, "ApplyEntityGlowEffects");

	auto const graphicsModule = Graphics::GetModule();

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

			if (anyGlowAlways && anyGlowOccluded && anyGlowVisible)
				break;
		}

		if (!anyGlowAlways && !anyGlowOccluded && !anyGlowVisible)
			return;	// Early out
	}

	VariablePusher<const CViewSetup*> _saveViewSetup(graphicsModule->m_View, pSetup);

	// Collect extra glow data we'll use in multiple upcoming loops -- This used to be right
	// before it would get used, but with NDEBUG_PER_FRAME_SUPPORT it needs to be before we
	// change a bunch of rendering settings.
	graphicsModule->BuildExtraGlowData(this, anyGlowAlways, anyGlowOccluded, anyGlowVisible);

	ITexture* const pRtFullFrameFB0 = materials->FindTexture("_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET);
	ITexture* const pRtFullFrameFB1 = materials->FindTexture("_rt_FullFrameFB1", TEXTURE_GROUP_RENDER_TARGET);

	pRenderContext->PushRenderTargetAndViewport();
	pRenderContext->SetToneMappingScaleLinear(Vector(1, 1, 1));

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
		CRefPtrFix<IMaterial> pGlowColorMaterial(materials->FindMaterial("castingessentials/outlines/color_override_material", TEXTURE_GROUP_OTHER, true));
		g_pStudioRender->ForcedMaterialOverride(pGlowColorMaterial);
		// HACK: Disable IStudioRender::ForcedMaterialOverride so ubers don't change it away from dev/glow_color
		s_DisableForcedMaterialOverride = true;

		pRenderContext->OverrideColorWriteEnable(true, true);
		pRenderContext->OverrideAlphaWriteEnable(true, true);

		// Draw "glow when visible" objects
		if (anyGlowVisible)
			graphicsModule->DrawGlowVisible(nSplitScreenSlot, pRenderContext);

		// Draw "glow when occluded" objects
		if (anyGlowOccluded)
			graphicsModule->DrawGlowOccluded(nSplitScreenSlot, pRenderContext);

		// Draw "glow always" objects
		if (anyGlowAlways)
			graphicsModule->DrawGlowAlways(nSplitScreenSlot, pRenderContext);

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

	const int nSrcWidth = pSetup->width;
	const int nSrcHeight = pSetup->height;
	int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
	pRenderContext->GetViewport(nViewportX, nViewportY, nViewportWidth, nViewportHeight);

	// Copy MSAA'd glow models to _rt_FullFrameFB0
	pRenderContext->CopyRenderTargetToTexture(pRtFullFrameFB0);

	// Move original contents of the backbuffer from _rt_FullFrameFB1 to the backbuffer
	{
#if FIXED_COPY_TEXTURE_TO_RENDER_TARGET	// Coordinates don't seem to be mapped 1:1 properly, screen becomes slightly blurry
		pRenderContext->CopyTextureToRenderTargetEx(0, pRtFullFrameFB1, nullptr);
#else
		pRenderContext->SetStencilEnable(false);

		CRefPtrFix<IMaterial> backbufferReloadMaterial(materials->FindMaterial("castingessentials/outlines/backbuffer_reload_from_fb1", TEXTURE_GROUP_RENDER_TARGET));
		pRenderContext->Bind(backbufferReloadMaterial);

		pRenderContext->OverrideDepthEnable(true, false);
		{
			pRenderContext->DrawScreenSpaceRectangle(backbufferReloadMaterial,
				0, 0, nViewportWidth, nViewportHeight,
				0, 0, nSrcWidth - 1, nSrcHeight - 1,
				pRtFullFrameFB1->GetActualWidth(), pRtFullFrameFB1->GetActualHeight());
		}
		pRenderContext->OverrideDepthEnable(false, false);
#endif
	}

	// Bloom glow models from _rt_FullFrameFB0 to backbuffer while stenciling out inside of models
	{
		if (graphicsModule->ce_graphics_glow_silhouettes.GetBool())
		{
			CRefPtrFix<IMaterial> finalBlendMaterial(materials->FindMaterial("castingessentials/outlines/final_blend", TEXTURE_GROUP_RENDER_TARGET));
			finalBlendMaterial->FindVar("$basetexture", nullptr)->SetTextureValue(pRtFullFrameFB0);

			pRenderContext->DrawScreenSpaceRectangle(finalBlendMaterial,
				0, 0, nViewportWidth, nViewportHeight,
				0, 0, nSrcWidth - 1, nSrcHeight - 1,
				pRtFullFrameFB1->GetActualWidth(), pRtFullFrameFB1->GetActualHeight());
		}
		else
		{
			// Set stencil state
			ShaderStencilState_t stencilState;
			stencilState.m_bEnable = graphicsModule->ce_outlines_debug_stencil_out.GetBool();
			stencilState.m_nWriteMask = 0; // We're not changing stencil
			stencilState.m_nReferenceValue = 1;
			stencilState.m_nTestMask = 1;
			stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_NOTEQUAL;
			stencilState.m_PassOp = STENCILOPERATION_KEEP;
			stencilState.m_FailOp = STENCILOPERATION_KEEP;
			stencilState.m_ZFailOp = STENCILOPERATION_KEEP;
			stencilState.SetStencilState(pRenderContext);

			auto srcTexture = pRtFullFrameFB0;
			auto dstTexture = pRtFullFrameFB1;

			//==================================//
			// Poisson expand onto _rt_SmallFB1 //
			//==================================//
			{
				CRefPtrFix<IMaterial> pPoissonExpand(materials->FindMaterial("castingessentials/outlines/poisson_outline", TEXTURE_GROUP_OTHER, true));
				pRenderContext->SetRenderTarget(dstTexture);
				pRenderContext->ClearBuffers(true, false, false);

				pPoissonExpand->FindVar("$C0_X")->SetFloatValue(1.0f / nSrcWidth);
				pPoissonExpand->FindVar("$C0_Y")->SetFloatValue(1.0f / nSrcHeight);
				pPoissonExpand->FindVar("$C0_Z")->SetFloatValue(graphicsModule->ce_outlines_expand.GetFloat());

				// Draw quad
				pRenderContext->DrawScreenSpaceRectangle(pPoissonExpand,
					0, 0, nSrcWidth, nSrcHeight,
					0, 0, nSrcWidth - 1, nSrcHeight - 1,
					srcTexture->GetActualWidth(), srcTexture->GetActualHeight());

				std::swap(srcTexture, dstTexture);
			}

			Vector2D targetSize(nSrcWidth, nSrcHeight);
			if (const auto BLUR = graphicsModule->ce_outlines_blur.GetFloat(); BLUR >= 1)
			{
				targetSize /= BLUR;

				// Downsampling
				{
					CRefPtrFix<IMaterial> matDownsample(materials->FindMaterial(
						"castingessentials/outlines/l4d_ce_downsample4x", TEXTURE_GROUP_RENDER_TARGET));

					pRenderContext->ClearColor4ub(0, 0, 0, 0);

					// Keep halving size until we can just rely on linear interpolation
					Vector2D inSize(nSrcWidth, nSrcHeight);
					Vector2D outSize(inSize / 2);
					for (; inSize.x / 2 > targetSize.x; inSize /= 2, outSize /= 2)
					{
						// Copy back and forth, halving res every time
						matDownsample->FindVar("$basetexture")->SetTextureValue(srcTexture);

						pRenderContext->SetRenderTarget(dstTexture);
						pRenderContext->ClearBuffers(true, false, false);

						pRenderContext->DrawScreenSpaceRectangle(matDownsample,
							0, 0, outSize.x, outSize.y,
							0, 0, inSize.x - 1, inSize.y - 1,
							srcTexture->GetActualWidth(), srcTexture->GetActualHeight());

						std::swap(srcTexture, dstTexture);
					}

					// Linear interpolate the rest of the way
					{
						CRefPtrFix<IMaterial> matCopy(materials->FindMaterial(
							"castingessentials/outlines/direct_copy", TEXTURE_GROUP_RENDER_TARGET));

						matCopy->FindVar("$basetexture")->SetTextureValue(srcTexture);

						pRenderContext->SetRenderTarget(dstTexture);
						pRenderContext->ClearBuffers(true, false, false);

						pRenderContext->DrawScreenSpaceRectangle(matCopy,
							0, 0, targetSize.x, targetSize.y,
							0, 0, inSize.x - 1, inSize.y - 1,
							srcTexture->GetActualWidth(), srcTexture->GetActualHeight());

						std::swap(srcTexture, dstTexture);
					}
				}

				//=================//
				// Guassian blur x //
				//=================//
				{
					CRefPtrFix<IMaterial> pMatBlurX(materials->FindMaterial("castingessentials/outlines/l4d_ce_blur_x", TEXTURE_GROUP_OTHER, true));
					pMatBlurX->FindVar("$basetexture")->SetTextureValue(srcTexture);
					pRenderContext->SetRenderTarget(dstTexture);
					pRenderContext->ClearBuffers(true, false, false);

					pMatBlurX->FindVar("$C0_X")->SetFloatValue(srcTexture->GetActualWidth());
					pMatBlurX->FindVar("$C0_Y")->SetFloatValue(srcTexture->GetActualHeight());

					// Blur X to _rt_SmallFB1
					pRenderContext->DrawScreenSpaceRectangle(pMatBlurX,
						0, 0, std::lround(targetSize.x), std::lround(targetSize.y),
						0, 0, targetSize.x - 1, targetSize.y - 1,
						srcTexture->GetActualWidth(), srcTexture->GetActualHeight());

					std::swap(srcTexture, dstTexture);
				}

				//=================//
				// Guassian blur y //
				//=================//
				{
					CRefPtrFix<IMaterial> pMatBlurY(materials->FindMaterial("castingessentials/outlines/l4d_ce_blur_y", TEXTURE_GROUP_OTHER));
					pMatBlurY->FindVar("$basetexture")->SetTextureValue(srcTexture);
					pRenderContext->SetRenderTarget(dstTexture);

					pMatBlurY->FindVar("$C0_X")->SetFloatValue(srcTexture->GetActualWidth());
					pMatBlurY->FindVar("$C0_Y")->SetFloatValue(srcTexture->GetActualHeight());

					// Blur Y to _rt_SmallFB0
					pRenderContext->DrawScreenSpaceRectangle(pMatBlurY,
						0, 0, std::lround(targetSize.x), std::lround(targetSize.y),
						0, 0, targetSize.x - 1, targetSize.y - 1,
						srcTexture->GetActualWidth(), srcTexture->GetActualHeight());

					std::swap(srcTexture, dstTexture);
				}
			}

			// Final upscale and blend onto backbuffer
			{
				CRefPtrFix<IMaterial> finalBlend(materials->FindMaterial("castingessentials/outlines/final_blend", TEXTURE_GROUP_RENDER_TARGET));
				finalBlend->FindVar("$basetexture", nullptr)->SetTextureValue(srcTexture);

				// Draw quad to backbuffer
				pRenderContext->SetRenderTarget(nullptr);

				//pRenderContext->Viewport(0, 0, pSetup->width, pSetup->height);
				pRenderContext->DrawScreenSpaceRectangle(finalBlend,
					0, 0, nViewportWidth, nViewportHeight,
					0, 0, targetSize.x - 1, targetSize.y - 1,
					srcTexture->GetActualWidth(), srcTexture->GetActualHeight());
			}
		}
	}

	// Player infills
	if (graphicsModule->ce_infills_enable.GetBool())
		graphicsModule->DrawInfills(pRenderContext);

	// Done with all of our "advanced" 3D rendering.
	pRenderContext->SetStencilEnable(false);
	pRenderContext->OverrideColorWriteEnable(false, false);
	pRenderContext->OverrideAlphaWriteEnable(false, false);
	pRenderContext->OverrideDepthEnable(false, false);

	pRenderContext->PopRenderTargetAndViewport();
}

bool Graphics::CheckDependencies()
{
	{
		const auto baseEntityClass = Entities::GetClientClass("CBaseEntity");
		s_MoveParent = Entities::GetEntityProp<EHANDLE>(baseEntityClass, "moveparent");
	}

	s_TFViewModelType = Entities::GetTypeChecker("CTFViewModel");

	return true;
}

void Graphics::OnTick(bool inGame)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);;
	if (!Interfaces::GetEngineClient()->IsInGame())
		return;

	if (ce_graphics_fix_invisible_players.GetBool())
	{
		for (Player* p : Player::Iterable())
		{
			if (!p)
				continue;

			auto entity = p->GetBaseEntity();
			if (!entity)
				continue;

			if (entity->RenderHandle() == INVALID_CLIENT_RENDER_HANDLE)
			{
				if (!entity->ShouldDraw())
					continue;

				PluginMsg("[ce_graphics_fix_invisible_players] Forced player %i into client leaf system.\n", entity->entindex());
				entity->AddToLeafSystem();
			}
		}
	}
}

Graphics::ExtraGlowData::ExtraGlowData(CGlowObjectManager::GlowObjectDefinition_t* base) : m_Base(base)
{
	m_ShouldOverrideGlowColor = false;

	// Dumb assert in copy constructor gets triggered when resizing std::vector
	m_GlowColorOverride.Init(-1, -1, -1);
}

bool Graphics::ExtraGlowData::AnyInfillsActive() const
{
	Assert(m_Infills.size() == 2);

	return m_Infills[0].m_Active || m_Infills[1].m_Active;
}

void Graphics::ExtraGlowData::ApplyGlowColor() const
{
	if (m_ShouldOverrideGlowColor)
	{
		Assert(m_GlowColorOverride.x >= 0 && m_GlowColorOverride.y >= 0 && m_GlowColorOverride.z >= 0);
		Assert(m_GlowColorOverride.x < 1 || m_GlowColorOverride.y < 1 || m_GlowColorOverride.z < 1);

		render->SetColorModulation(m_GlowColorOverride.Base());
	}
	else
	{
		render->SetColorModulation(m_Base->m_vGlowColor.Base());
	}
}

float Graphics::PlayerHealthState::GetLastHurtTime() const
{
	return Interfaces::GetEngineTool()->ClientTime() - m_LastHurtTime;
}

void Graphics::PlayerHealthState::ResetLastHurtTime()
{
	m_LastHurtTime = Interfaces::GetEngineTool()->ClientTime();
}

void Graphics::PlayerHealthState::UpdateInternal(bool tickUpdate, bool frameUpdate)
{
	if (!tickUpdate)
		return;

	auto health = GetPlayer().GetHealth();

	// Update last hurt time
	if (health < m_LastHurtHealth)
		m_LastHurtTime = Interfaces::GetEngineTool()->ClientTime();

	m_LastHurtHealth = health;
}
