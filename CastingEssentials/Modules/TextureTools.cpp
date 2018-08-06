#include "Modules/TextureTools.h"

#include "PluginBase/Interfaces.h"

#include <client/baseclientrendertargets.h>
#include <vtf/vtf.h>

MODULE_REGISTER(TextureTools);

TextureTools::TextureTools() :
	ce_texturetools_full_res_rts("ce_texturetools_full_res_rts", "0", FCVAR_NONE,
		"Create the refraction and water reflection textures at framebuffer resolution (if possible). Must be set before the first map load. Changing after that requires a game restart.",
		[](IConVar*, const char*, float) { GetModule()->ToggleFullResRTs(); }),

	m_CreateRenderTargetsHook(std::bind(&TextureTools::InitClientRenderTargetsOverride, this,
		std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5), false)
{
}

bool TextureTools::CheckDependencies()
{
	return true;
}

static ITexture* CreateWaterReflectionTexture(IMaterialSystem* pMaterialSystem, int iSize)
{
	return pMaterialSystem->CreateNamedRenderTargetTextureEx2(
		"_rt_WaterReflection",
		iSize, iSize, RT_SIZE_FULL_FRAME_BUFFER,
		pMaterialSystem->GetBackBufferFormat(),
		MATERIAL_RT_DEPTH_SHARED,
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		CREATERENDERTARGETFLAGS_HDR);
}

static ITexture* CreateWaterRefractionTexture(IMaterialSystem* pMaterialSystem, int iSize)
{
	return pMaterialSystem->CreateNamedRenderTargetTextureEx2(
		"_rt_WaterRefraction",
		iSize, iSize, RT_SIZE_FULL_FRAME_BUFFER,
		// This is different than reflection because it has to have alpha for fog factor.
		IMAGE_FORMAT_RGBA8888,
		MATERIAL_RT_DEPTH_SHARED,
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		CREATERENDERTARGETFLAGS_HDR);
}

static ITexture* CreateCameraTexture(IMaterialSystem* pMaterialSystem, int iSize)
{
	return pMaterialSystem->CreateNamedRenderTargetTextureEx2(
		"_rt_Camera",
		iSize, iSize, RT_SIZE_DEFAULT,
		pMaterialSystem->GetBackBufferFormat(),
		MATERIAL_RT_DEPTH_SHARED,
		0,
		CREATERENDERTARGETFLAGS_HDR);
}

void TextureTools::ToggleFullResRTs()
{
	m_CreateRenderTargetsHook.SetEnabled(ce_texturetools_full_res_rts.GetBool());
}

void TextureTools::InitClientRenderTargetsOverride(CBaseClientRenderTargets* pThis,
	IMaterialSystem* matsys, IMaterialSystemHardwareConfig* config, int waterRes, int cameraRes)
{
	auto pAbstract = Interfaces::GetClientRenderTargets();
	auto pBase = static_cast<CBaseClientRenderTargets*>(pAbstract);
	Assert(pBase == pThis);

	// Full frame depth texture
	{
		materials->RemoveTextureAlias("_rt_FullFrameDepth");
		m_FullFrameDepth.Init(materials->CreateNamedRenderTargetTextureEx2("_rt_FullFrameDepth", 1, 1,
			RT_SIZE_FULL_FRAME_BUFFER, IMAGE_FORMAT_IA88, MATERIAL_RT_DEPTH_NONE,
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT));
	}

	// Water effects
	pBase->m_WaterReflectionTexture.Init(CreateWaterReflectionTexture(matsys, 256));
	pBase->m_WaterRefractionTexture.Init(CreateWaterRefractionTexture(matsys, 256));

	// Just leave this at default because tf2 doesn't use the camera texture
	pBase->m_CameraTexture.Init(CreateCameraTexture(matsys, 256));

	m_CreateRenderTargetsHook.SetState(Hooking::HookAction::SUPERCEDE);
}