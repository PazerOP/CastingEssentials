#pragma once

#include "PluginBase/Modules.h"
#include "PluginBase/HookManager.h"

#include <convar.h>

#include <optional>

class TextureTools : public Module<TextureTools>
{
public:
	TextureTools();

	static bool CheckDependencies();

private:
	void ToggleResourceOverridesEnabled();
	void ToggleFlagOverridesEnabled();

	void* GetResourceDataOverride(CVTFTexture* pThis, uint32 type, size_t* dataSize);
	bool ReadHeaderOverride(CVTFTexture* pThis, CUtlBuffer& buf, VTFFileHeader_t& header);

	Hook<HookFunc::IClientRenderTargets_InitClientRenderTargets> m_CreateRenderTargetsHook;

	void ToggleFullResRTs();
	void InitClientRenderTargetsOverride(IMaterialSystem* pMaterialSystem, IMaterialSystemHardwareConfig* config);

	ConVar ce_texturetools_full_res_rts;
};
