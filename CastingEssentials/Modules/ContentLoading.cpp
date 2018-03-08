#include "ContentLoading.h"
#include "PluginBase/HookManager.h"

#include <convar.h>
#include <qlimits.h>

struct model_t
{
	uint32_t : 32;
	char szName[MAX_QPATH];
};

ContentLoading::ContentLoading()
{
	m_Hook_CModelLoader_Map_LoadModel = GetHooks()->AddHook<HookFunc::CModelLoader_Map_LoadModel>(std::bind(&ContentLoading::Override_CModelLoader_Map_LoadModel, this, std::placeholders::_1, std::placeholders::_2));

	GetHooks()->AddHook<HookFunc::Global_Map_CheckForHDR>(std::bind(&ContentLoading::Override_Map_CheckForHDR, this, std::placeholders::_1, std::placeholders::_2));

	GetHooks()->AddHook<HookFunc::Global_EnableHDR>(std::bind(&ContentLoading::Override_EnableHDR, this, std::placeholders::_1));

	GetHooks()->AddHook<HookFunc::CTextureManager_LoadTexture>(std::bind(&ContentLoading::Override_CTextureManager_LoadTexture, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));

	ApplyAsyncConvars();
}

void ContentLoading::ApplyAsyncConvars() const
{
	SetCvar("mod_load_mesh_async", "1");
	SetCvar("mod_load_anims_async", "1");
	SetCvar("mod_load_vcollide_async", "1");

	SetCvar("mod_forcedata", "0");
	SetCvar("mod_touchalldata", "0");
	SetCvar("mod_forcetouchdata", "0");

	SetCvar("filesystem_max_stdio_read", "2047");
}
void ContentLoading::SetCvar(const char* name, const char* value)
{
	ConVarRef cvar(name);
	cvar.SetValue(value);
}

void ContentLoading::Override_CModelLoader_Map_LoadModel(CModelLoader* pThis, model_t* mod)
{
	GetHooks()->SetState<HookFunc::CModelLoader_Map_LoadModel>(Hooking::HookAction::SUPERCEDE);
	return GetHooks()->GetOriginal<HookFunc::CModelLoader_Map_LoadModel>()(pThis, mod);
}

bool ContentLoading::Override_Map_CheckForHDR(model_t* pModel, const char* pLoadName)
{
	GetHooks()->SetState<HookFunc::Global_Map_CheckForHDR>(Hooking::HookAction::SUPERCEDE);
	return GetHooks()->GetOriginal<HookFunc::Global_Map_CheckForHDR>()(pModel, pLoadName);
}

void ContentLoading::Override_EnableHDR(bool enable)
{
	GetHooks()->SetState<HookFunc::Global_EnableHDR>(Hooking::HookAction::SUPERCEDE);
	return GetHooks()->GetOriginal<HookFunc::Global_EnableHDR>()(enable);
}

ITextureInternal* ContentLoading::Override_CTextureManager_LoadTexture(CTextureManager* pThis, const char* pTextureName, const char* pTextureGroupName, int unknown1, bool unknown2)
{
	GetHooks()->SetState<HookFunc::CTextureManager_LoadTexture>(Hooking::HookAction::SUPERCEDE);
	return GetHooks()->GetOriginal<HookFunc::CTextureManager_LoadTexture>()(pThis, pTextureName, pTextureGroupName, unknown1, unknown2);
}
