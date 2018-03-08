#pragma once

#include "PluginBase/Modules.h"

class CModelLoader;
class CTextureManager;
class ITextureInternal;
struct model_t;
enum REFERENCETYPE;

class ContentLoading final : public Module<ContentLoading>
{
public:
	ContentLoading();

private:
	void ApplyAsyncConvars() const;
	static void SetCvar(const char* name, const char* value);

	int m_Hook_CModelLoader_Map_LoadModel;
	void Override_CModelLoader_Map_LoadModel(CModelLoader* pThis, model_t* mod);

	bool Override_Map_CheckForHDR(model_t* pModel, const char* pLoadName);

	void Override_EnableHDR(bool enable);

	ITextureInternal* Override_CTextureManager_LoadTexture(CTextureManager* pThis, const char* pTextureName, const char* pTextureGroupName, int unknown1, bool unknown2);
};