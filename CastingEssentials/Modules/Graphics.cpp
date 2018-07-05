#include "Graphics.h"
#include "Controls/StubPanel.h"
#include "Misc/CRefPtrFix.h"
#include "Misc/Extras/VPlane.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Entities.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"

#include <bone_setup.h>
#include <debugoverlay_shared.h>
#include <view_shared.h>
#include <materialsystem/itexture.h>
#include <materialsystem/imaterial.h>
#include <materialsystem/imaterialvar.h>
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

static constexpr auto STENCIL_INDEX_MASK = 0xFC;

// Should we use a hook to disable IStudioRender::ForcedMaterialOverride?
static bool s_DisableForcedMaterialOverride = false;

Graphics::Graphics() :
	ce_graphics_disable_prop_fades("ce_graphics_disable_prop_fades", "0", FCVAR_UNREGISTERED, "Enable/disable prop fading."),
	ce_graphics_debug_glow("ce_graphics_debug_glow", "0", FCVAR_UNREGISTERED),
	ce_graphics_glow_silhouettes("ce_graphics_glow_silhouettes", "0", FCVAR_NONE, "Turns outlines into silhouettes."),
	ce_graphics_glow_intensity("ce_graphics_glow_intensity", "1", FCVAR_NONE, "Global scalar for glow intensity"),
	ce_graphics_improved_glows("ce_graphics_improved_glows", "1", FCVAR_NONE, "Should we used the new and improved glow code?"),
	ce_graphics_fix_invisible_players("ce_graphics_fix_invisible_players", "1", FCVAR_NONE,
		"Fix a case where players are invisible if you're firstperson speccing them when the round starts."),

	ce_outlines_mode("ce_outlines_mode", "1", FCVAR_NONE, "Changes the style of outlines.\n\t0: TF2-style hard outlines.\n\t1: L4D-style soft outlines."),
	ce_outlines_debug_stencil_out("ce_outlines_debug_stencil_out", "1", FCVAR_NONE, "Should we stencil out the players during the final blend to screen?"),
	ce_outlines_players_override_red("ce_outlines_players_override_red", "", FCVAR_NONE,
		"Override color for red players. [0, 255], format is \"<red> <green> <blue>\"."),
	ce_outlines_players_override_blue("ce_outlines_players_override_blue", "", FCVAR_NONE,
		"Override color for blue players. [0, 255], format is \"<red> <green> <blue>\"."),
	ce_outlines_additive("ce_outlines_additive", "1", FCVAR_NONE, "If set to 1, outlines will add to underlying colors rather than replace them."),
	ce_outlines_debug("ce_outlines_debug", "0", FCVAR_NONE),
	ce_outlines_spy_visibility("ce_outlines_spy_visibility", "1", FCVAR_NONE,
		"If set to 1, always show outlines around cloaked spies (as opposed to only when they are behind walls)."),

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

	ce_graphics_dump_shader_params("ce_graphics_dump_shader_params", DumpShaderParams, "Prints out all parameters for a given shader.", FCVAR_NONE, DumpShaderParamsAutocomplete)
{
	m_ComputeEntityFadeHook = GetHooks()->AddHook<HookFunc::Global_UTILComputeEntityFade>(std::bind(&Graphics::ComputeEntityFadeOveride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

	m_ApplyEntityGlowEffectsHook = GetHooks()->AddHook<HookFunc::CGlowObjectManager_ApplyEntityGlowEffects>(std::bind(&Graphics::ApplyEntityGlowEffectsOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7, std::placeholders::_8, std::placeholders::_9));

	m_ForcedMaterialOverrideHook = GetHooks()->AddHook<HookFunc::IStudioRender_ForcedMaterialOverride>(std::bind(&Graphics::ForcedMaterialOverrideOverride, this, std::placeholders::_1, std::placeholders::_2));
}

Graphics::~Graphics()
{
	if (m_ComputeEntityFadeHook && GetHooks()->RemoveHook<HookFunc::Global_UTILComputeEntityFade>(m_ComputeEntityFadeHook, __FUNCSIG__))
		m_ComputeEntityFadeHook = 0;

	Assert(!m_ComputeEntityFadeHook);

	if (m_ApplyEntityGlowEffectsHook && GetHooks()->RemoveHook<HookFunc::CGlowObjectManager_ApplyEntityGlowEffects>(m_ApplyEntityGlowEffectsHook, __FUNCSIG__))
		m_ApplyEntityGlowEffectsHook = 0;

	Assert(!m_ApplyEntityGlowEffectsHook);

	if (m_ForcedMaterialOverrideHook && GetHooks()->RemoveHook<HookFunc::IStudioRender_ForcedMaterialOverride>(m_ForcedMaterialOverrideHook, __FUNCSIG__))
		m_ForcedMaterialOverrideHook = 0;

	Assert(!m_ForcedMaterialOverrideHook);
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
	IShader** shaderList = (IShader**)stackalloc(shaderCount * sizeof(*shaderList));
	materials->GetShaders(0, shaderCount, shaderList);

	if (cmd.ArgC() < 2)
	{
		// Sort shaders alphabetically
		std::qsort(shaderList, shaderCount, sizeof(*shaderList),
			[](void const* p1, void const* p2) { return stricmp((*(IShader**)p1)->GetName(), (*(IShader**)p2)->GetName()); });

		Warning("Usage: %s <shader name>\nAvailable shaders:\n", cmd.Arg(0));
		for (int i = 0; i < shaderCount; i++)
			Warning("\t%s\n", shaderList[i]->GetName());

		return;
	}

	// Find the shader
	for (int i = 0; i < shaderCount; i++)
	{
		auto current = shaderList[i];
		if (stricmp(current->GetName(), cmd.Arg(1)))
			continue;

		const auto paramCount = current->GetNumParams();

		// Sort parameters alphabetically
		int* paramOrder;
		{
			paramOrder = (int*)stackalloc(paramCount * sizeof(*paramOrder));
			for (int p = 0; p < paramCount; p++)
				paramOrder[p] = p;

			// Shitty thread safety, whatever, this'll never be re-entrant
			static thread_local IShader* s_Shader = nullptr;
			s_Shader = current;
			std::qsort(paramOrder, paramCount, sizeof(*paramOrder),
				[](void const* p1, void const* p2) { return stricmp(s_Shader->GetParamName(*(int*)p1), s_Shader->GetParamName(*(int*)p2)); });
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

int Graphics::DumpShaderParamsAutocomplete(const char *partial, char commands[COMMAND_COMPLETION_MAXITEMS][COMMAND_COMPLETION_ITEM_LENGTH])
{
	const auto shaderCount = materials->ShaderCount();
	IShader** shaderList = (IShader**)stackalloc(shaderCount * sizeof(*shaderList));
	const auto actualShaderCount = materials->GetShaders(0, shaderCount, shaderList);

	Assert(shaderCount == actualShaderCount);

	const char* const cmdName = partial;

	// Go forward until the second word
	while (*partial && !isspace(*partial))
		partial++;

	// Record command length
	const size_t cmdLength = partial - cmdName;

	while (*partial && isspace(*partial))
		partial++;

	const auto partialLength = strlen(partial);

	// Sort shaders alphabetically
	std::qsort(shaderList, shaderCount, sizeof(*shaderList),
		[](void const* p1, void const* p2) { return stricmp((*(IShader**)p1)->GetName(), (*(IShader**)p2)->GetName()); });

	int outputCount = 0;
	int outputIndices[COMMAND_COMPLETION_MAXITEMS];

	// Search starting from the beginning of the string
	for (int i = 0; i < shaderCount; i++)
	{
		const char* shaderName = shaderList[i]->GetName();

		if (strnicmp(partial, shaderName, partialLength))
			continue;

		snprintf(commands[outputCount], COMMAND_COMPLETION_ITEM_LENGTH, "%.*s %s", cmdLength, cmdName, shaderName);
		outputIndices[outputCount] = i;
		outputCount++;

		if (outputCount >= COMMAND_COMPLETION_MAXITEMS)
			break;
	}

	const auto firstLoopOutputsEnd = std::begin(outputIndices) + outputCount;

	// Now search anywhere in the string
	for (int i = 0; i < shaderCount; i++)
	{
		if (outputCount >= COMMAND_COMPLETION_MAXITEMS)
			break;

		if (std::find(std::begin(outputIndices), firstLoopOutputsEnd, i) != firstLoopOutputsEnd)
			continue;

		const char* shaderName = shaderList[i]->GetName();
		if (!stristr(shaderName, partial))
			continue;

		snprintf(commands[outputCount], COMMAND_COMPLETION_ITEM_LENGTH, "%.*s %s", cmdLength, cmdName, shaderName);
		outputCount++;
	}

	return outputCount;
}

unsigned char Graphics::ComputeEntityFadeOveride(C_BaseEntity* entity, float minDist, float maxDist, float fadeScale)
{
	constexpr auto max = std::numeric_limits<unsigned char>::max();

	if (ce_graphics_disable_prop_fades.GetBool())
	{
		GetHooks()->SetState<HookFunc::Global_UTILComputeEntityFade>(Hooking::HookAction::SUPERCEDE);
		return max;
	}

	return 0;
}

void Graphics::ApplyEntityGlowEffectsOverride(CGlowObjectManager * pThis, const CViewSetup * pSetup, int nSplitScreenSlot, CMatRenderContextPtr & pRenderContext, float flBloomScale, int x, int y, int w, int h)
{
	if (ce_graphics_improved_glows.GetBool())
	{
		GetHooks()->SetState<HookFunc::CGlowObjectManager_ApplyEntityGlowEffects>(Hooking::HookAction::SUPERCEDE);
		pThis->ApplyEntityGlowEffects(pSetup, nSplitScreenSlot, pRenderContext, flBloomScale, x, y, w, h);
	}
	else
	{
		GetHooks()->SetState<HookFunc::CGlowObjectManager_ApplyEntityGlowEffects>(Hooking::HookAction::IGNORE);
	}
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

int Graphics::PlaneAABBIntersection(const VPlane& plane, const Vector& mins, const Vector& maxs, Vector intersections[6])
{
	Vector lineSegments[12][2] =
	{
		// Horizontals 1
		{ Vector(mins.x, mins.y, mins.z), Vector(maxs.x, mins.y, mins.z) },
		{ Vector(mins.x, maxs.y, mins.z), Vector(maxs.x, maxs.y, mins.z) },
		{ Vector(mins.x, mins.y, maxs.z), Vector(maxs.x, mins.y, maxs.z) },
		{ Vector(mins.x, maxs.y, maxs.z), Vector(maxs.x, maxs.y, maxs.z) },

		// Horizontals 2
		{ Vector(mins.x, mins.y, mins.z), Vector(mins.x, maxs.y, mins.z) },
		{ Vector(maxs.x, mins.y, mins.z), Vector(maxs.x, maxs.y, mins.z) },
		{ Vector(mins.x, mins.y, maxs.z), Vector(mins.x, maxs.y, maxs.z) },
		{ Vector(maxs.x, mins.y, maxs.z), Vector(maxs.x, maxs.y, maxs.z) },

		// Verticals
		{ Vector(mins.x, mins.y, mins.z), Vector(mins.x, mins.y, maxs.z) },
		{ Vector(maxs.x, mins.y, mins.z), Vector(maxs.x, mins.y, maxs.z) },
		{ Vector(mins.x, maxs.y, mins.z), Vector(mins.x, maxs.y, maxs.z) },
		{ Vector(maxs.x, maxs.y, mins.z), Vector(maxs.x, maxs.y, maxs.z) },
	};

	int intersectionCount = 0;
	for (uint_fast8_t i = 0; i < 12; i++)
	{
		Vector intersection;
		if (!VPlaneIntersectLine(plane, lineSegments[i][0], lineSegments[i][1], &intersection))
			continue;

		intersections[intersectionCount++] = intersection;
		Assert(intersectionCount <= 6);
	}

	return intersectionCount;
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

static void RotateVectorAroundVector(const Vector& toRotate, const Vector& rotationAxis, float degrees, Vector& out)
{
	const float rads = Deg2Rad(degrees);

	out = cosf(rads) * toRotate + sinf(rads) * rotationAxis.Cross(toRotate) + (1 - cosf(rads)) * rotationAxis.Dot(toRotate) * rotationAxis;
}

bool Graphics::Test_PlaneHitboxesIntersect(C_BaseAnimating* animating, Vector2D& screenMins, Vector2D& screenMaxs)
{
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

	VMatrix worldToScreen;
	Interfaces::GetEngineTool()->GetWorldToScreenMatrixForView(*m_View, &worldToScreen);

	Frustum_t viewFrustum;
	GeneratePerspectiveFrustum(m_View->origin, m_View->angles, m_View->zNear, m_View->zFar, m_View->fov, m_View->m_flAspectRatio, viewFrustum);

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
void Graphics::BuildExtraGlowData(CGlowObjectManager* glowMgr)
{
	m_ExtraGlowData.clear();

	bool hasRedOverride, hasBlueOverride;
	Vector redOverride = ColorToVector(ColorFromConVar(ce_outlines_players_override_red, &hasRedOverride)) * ce_graphics_glow_intensity.GetFloat();
	Vector blueOverride = ColorToVector(ColorFromConVar(ce_outlines_players_override_blue, &hasBlueOverride)) * ce_graphics_glow_intensity.GetFloat();

	uint8_t stencilIndex = 0;

	const bool infillsEnable = ce_infills_enable.GetBool();
	const bool bInfillDebug = ce_infills_debug.GetBool();

	const Color redInfillNormal = ColorFromConVar(ce_infills_hurt_red);
	const Color blueInfillNormal = ColorFromConVar(ce_infills_hurt_blue);
	const Color redInfillBuffed = ColorFromConVar(ce_infills_buffed_red);
	const Color blueInfillBuffed = ColorFromConVar(ce_infills_buffed_blue);

	VMatrix worldToScreen;
	if (infillsEnable)
		Interfaces::GetEngineTool()->GetWorldToScreenMatrixForView(*m_View, &worldToScreen);

	for (int i = 0; i < glowMgr->m_GlowObjectDefinitions.Count(); i++)
	{
		auto& current = glowMgr->m_GlowObjectDefinitions[i];
		if (current.IsUnused())
			continue;

		m_ExtraGlowData.emplace_back(&current);
		auto& currentExtra = m_ExtraGlowData.back();

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
						player->UpdateLastHurtTime();

						Vector worldMins, worldMaxs;
						Vector2D screenMins, screenMaxs;

						if (Test_PlaneHitboxesIntersect(player->GetBaseAnimating(), screenMins, screenMaxs))
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
									hurtInfill.m_Color.a() *= ApplyInfillTimeEffects(player->GetLastHurtTime());	// Infill fading/flickering

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

#if DRAW_INFILL_VGUI
		if (currentExtra.m_HurtInfillActive)
		{
			g_pVGuiSurface->DrawSetColor(currentExtra.m_HurtInfillColor);
			g_pVGuiSurface->DrawFilledRect(
				currentExtra.m_HurtInfillRectMin.x, m_View->height - currentExtra.m_HurtInfillRectMin.y,
				currentExtra.m_HurtInfillRectMax.x, m_View->height - currentExtra.m_HurtInfillRectMax.y);
		}

		if (currentExtra.m_BuffedInfillActive)
		{
			g_pVGuiSurface->DrawSetColor(currentExtra.m_BuffedInfillColor);
			g_pVGuiSurface->DrawFilledRect(
				currentExtra.m_BuffedInfillRectMin.x, m_View->height - currentExtra.m_BuffedInfillRectMin.y,
				currentExtra.m_BuffedInfillRectMax.x, m_View->height - currentExtra.m_BuffedInfillRectMax.y);
		}
#else
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
#endif
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
		player->ResetLastHurtTime();
}

void Graphics::BuildMoveChildLists()
{
	IClientEntityList* const entityList = Interfaces::GetClientEntityList();
	for (int i = 0; i < entityList->GetHighestEntityIndex(); i++)
	{
		C_BaseEntity* child = dynamic_cast<C_BaseEntity*>(entityList->GetClientEntity(i));
		if (!child || !child->ShouldDraw())
			continue;

		if (auto childAnimating = child->GetBaseAnimating())
		{
			if (childAnimating->IsViewModel() || !strcmp("CTFViewModel", child->GetClientClass()->GetName()))
				continue;
		}

		EHANDLE* moveparent = Entities::GetEntityProp<EHANDLE*>(child, "moveparent");
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
				//extra->ApplyGlowColor();
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
	render->SetBlend(1);
	for (const auto& current : m_ExtraGlowData)
	{
		if (current.m_Base->IsUnused() || !current.m_Base->ShouldDraw(nSplitScreenSlot) || !current.m_Base->m_bRenderWhenOccluded || !current.m_Base->m_bRenderWhenUnoccluded)
			continue;

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
			if (current.m_Base->IsUnused() || !current.m_Base->ShouldDraw(nSplitScreenSlot) || !current.m_Base->m_bRenderWhenOccluded || current.m_Base->m_bRenderWhenUnoccluded)
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
	render->SetBlend(1);
	for (const auto& current : m_ExtraGlowData)
	{
		if (current.m_Base->IsUnused() || !current.m_Base->ShouldDraw(nSplitScreenSlot) || !current.m_Base->m_bRenderWhenOccluded || current.m_Base->m_bRenderWhenUnoccluded)
			continue;

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
	render->SetBlend(1);
	for (const auto& current : m_ExtraGlowData)
	{
		if (current.m_Base->IsUnused() || !current.m_Base->ShouldDraw(nSplitScreenSlot) || current.m_Base->m_bRenderWhenOccluded || !current.m_Base->m_bRenderWhenUnoccluded)
			continue;

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
	graphicsModule->BuildExtraGlowData(this);

	ITexture* const pRtFullFrameFB0 = materials->FindTexture("_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET);
	ITexture* const pRtFullFrameFB1 = materials->FindTexture("_rt_FullFrameFB1", TEXTURE_GROUP_RENDER_TARGET);
	ITexture* const pRtSmallFB0 = materials->FindTexture("_rt_SmallFB0", TEXTURE_GROUP_RENDER_TARGET);
	ITexture* const pRtSmallFB1 = materials->FindTexture("_rt_SmallFB1", TEXTURE_GROUP_RENDER_TARGET);

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

	// Copy MSAA'd glow models to _rt_FullFrameFB0
	pRenderContext->CopyRenderTargetToTexture(pRtFullFrameFB0);

	const int nSrcWidth = pSetup->width;
	const int nSrcHeight = pSetup->height;
	int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
	pRenderContext->GetViewport(nViewportX, nViewportY, nViewportWidth, nViewportHeight);

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
			pRenderContext->Bind(finalBlendMaterial);

			pRenderContext->OverrideDepthEnable(true, false);
			{
				pRenderContext->DrawScreenSpaceRectangle(finalBlendMaterial,
					0, 0, nViewportWidth, nViewportHeight,
					0, 0, nSrcWidth - 1, nSrcHeight - 1,
					pRtFullFrameFB1->GetActualWidth(), pRtFullFrameFB1->GetActualHeight());
			}
			pRenderContext->OverrideDepthEnable(false, false);
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

			if (graphicsModule->ce_outlines_mode.GetBool())
			{
				//============================================
				// Downsample _rt_FullFrameFB to _rt_SmallFB0
				//============================================
				{
					CRefPtrFix<IMaterial> pMatDownsample(materials->FindMaterial("castingessentials/outlines/l4d_downsample", TEXTURE_GROUP_OTHER, true));
					pRenderContext->SetRenderTarget(pRtSmallFB0);

					// First clear the full target to black if we're not going to touch every pixel
					if ((pRtSmallFB0->GetActualWidth() != (pSetup->width / 4)) || (pRtSmallFB0->GetActualHeight() != (pSetup->height / 4)))
					{
						pRenderContext->Viewport(0, 0, pRtSmallFB0->GetActualWidth(), pRtSmallFB0->GetActualHeight());
						pRenderContext->ClearColor3ub(0, 0, 0);
						pRenderContext->ClearBuffers(true, false, false);
					}

					// Set the viewport
					pRenderContext->Viewport(0, 0, pSetup->width / 4, pSetup->height / 4);

					// Downsample to _rt_SmallFB0
					pRenderContext->DrawScreenSpaceRectangle(pMatDownsample, 0, 0, nSrcWidth / 4, nSrcHeight / 4,
						0, 0, nSrcWidth - 4, nSrcHeight - 4,
						pRtFullFrameFB0->GetActualWidth(), pRtFullFrameFB0->GetActualHeight());
				}

				//============================//
				// Guassian blur x rt0 to rt1 //
				//============================//
				{
					CRefPtrFix<IMaterial> pMatBlurX(materials->FindMaterial("castingessentials/outlines/l4d_blur_x", TEXTURE_GROUP_OTHER, true));
					pRenderContext->SetRenderTarget(pRtSmallFB1);

					// First clear the full target to black if we're not going to touch every pixel
					if ((pRtSmallFB1->GetActualWidth() != (pSetup->width / 4)) || (pRtSmallFB1->GetActualHeight() != (pSetup->height / 4)))
					{
						pRenderContext->Viewport(0, 0, pRtSmallFB1->GetActualWidth(), pRtSmallFB1->GetActualHeight());
						pRenderContext->ClearColor3ub(0, 0, 0);
						pRenderContext->ClearBuffers(true, false, false);
					}

					// Set the viewport
					pRenderContext->Viewport(0, 0, pSetup->width / 4, pSetup->height / 4);

					// Blur X to _rt_SmallFB1
					pRenderContext->DrawScreenSpaceRectangle(pMatBlurX, 0, 0, nSrcWidth / 4, nSrcHeight / 4,
						0, 0, nSrcWidth / 4 - 1, nSrcHeight / 4 - 1,
						pRtSmallFB0->GetActualWidth(), pRtSmallFB0->GetActualHeight());
				}

				//============================//
				// Gaussian blur y rt1 to rt0 //
				//============================//
				{
					CRefPtrFix<IMaterial> pMatBlurY(materials->FindMaterial("castingessentials/outlines/l4d_blur_y", TEXTURE_GROUP_OTHER, true));

					pRenderContext->SetRenderTarget(pRtSmallFB0);
					pRenderContext->Viewport(0, 0, pSetup->width / 4, pSetup->height / 4);
					IMaterialVar *pBloomAmountVar = pMatBlurY->FindVar("$bloomamount", NULL);
					if (pBloomAmountVar)
						pBloomAmountVar->SetFloatValue(flBloomScale);

					// Blur Y to _rt_SmallFB0
					pRenderContext->DrawScreenSpaceRectangle(pMatBlurY, 0, 0, nSrcWidth / 4, nSrcHeight / 4,
						0, 0, nSrcWidth / 4 - 1, nSrcHeight / 4 - 1,
						pRtSmallFB1->GetActualWidth(), pRtSmallFB1->GetActualHeight());
				}

				// Multiply alpha into _rt_SmallFB1
				if (!graphicsModule->ce_outlines_additive.GetBool())
				{
					CRefPtrFix<IMaterial> pMatAlphaMul(materials->FindMaterial("castingessentials/outlines/l4d_ce_translucent_pass", TEXTURE_GROUP_OTHER, true));
					pRenderContext->SetRenderTarget(pRtSmallFB1);
					pRenderContext->Viewport(0, 0, pSetup->width / 4, pSetup->height / 4);
					pRenderContext->ClearBuffers(true, false, false);
					pRenderContext->DrawScreenSpaceRectangle(pMatAlphaMul, 0, 0, nSrcWidth / 4, nSrcHeight / 4,
						0, 0, nSrcWidth / 4 - 1, nSrcHeight / 4 - 1,
						pRtSmallFB1->GetActualWidth(), pRtSmallFB1->GetActualHeight());
				}

				// Final upscale and blend onto backbuffer
				{
					CRefPtrFix<IMaterial> finalBlendL4D(materials->FindMaterial("castingessentials/outlines/l4d_final_blend", TEXTURE_GROUP_RENDER_TARGET));
					{
						auto baseTextureVar = finalBlendL4D->FindVar("$basetexture", nullptr);
						if (baseTextureVar)
							baseTextureVar->SetTextureValue(graphicsModule->ce_outlines_additive.GetBool() ? pRtSmallFB0 : pRtSmallFB1);

						finalBlendL4D->SetMaterialVarFlag(MaterialVarFlags_t::MATERIAL_VAR_TRANSLUCENT, !graphicsModule->ce_outlines_additive.GetBool());
						finalBlendL4D->SetMaterialVarFlag(MaterialVarFlags_t::MATERIAL_VAR_ADDITIVE, graphicsModule->ce_outlines_additive.GetBool());
					}

					// Draw quad
					pRenderContext->SetRenderTarget(nullptr);
					pRenderContext->Viewport(0, 0, pSetup->width, pSetup->height);
					pRenderContext->DrawScreenSpaceRectangle(finalBlendL4D,
						0, 0, nViewportWidth, nViewportHeight,
						0, 0, nSrcWidth / 4 - 1, nSrcHeight / 4 - 1,
						pRtSmallFB0->GetActualWidth(),
						pRtSmallFB0->GetActualHeight());
				}
			}
			else
			{
				CRefPtrFix<IMaterial> pMatHaloAddToScreen(materials->FindMaterial("dev/halo_add_to_screen", TEXTURE_GROUP_OTHER, true));

				// Write to alpha
				pRenderContext->OverrideAlphaWriteEnable(true, true);

				// Draw quad
				pRenderContext->DrawScreenSpaceRectangle(pMatHaloAddToScreen,
					0, 0, nViewportWidth, nViewportHeight,
					0, 0, nSrcWidth - 1, nSrcHeight - 1,
					pRtFullFrameFB0->GetActualWidth(),
					pRtFullFrameFB0->GetActualHeight());
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

	static ConVarRef mat_hdr_level("mat_hdr_level");
	static ConVarRef mat_disable_bloom("mat_disable_bloom");
	if (mat_hdr_level.GetInt() < 1 || mat_disable_bloom.GetBool())
	{
		// Wipe these textures to prevent spooky apparitions
		pRenderContext->ClearColor4ub(0, 0, 0, 0);

		pRenderContext->SetRenderTarget(pRtSmallFB0);
		pRenderContext->ClearBuffers(true, false, false);

		pRenderContext->SetRenderTarget(pRtSmallFB1);
		pRenderContext->ClearBuffers(true, false, false);
	}

	pRenderContext->PopRenderTargetAndViewport();
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
		const Vector vGlowColor = m_Base->m_vGlowColor * (m_Base->m_flGlowAlpha * GetModule()->ce_graphics_glow_intensity.GetFloat());
		Assert(vGlowColor.x < 1 || vGlowColor.y < 1 || vGlowColor.z < 1);

		render->SetColorModulation(vGlowColor.Base());
	}
}
