#pragma once
#include "Hooking/GroupGlobalHook.h"
#include "Hooking/GroupManualClassHook.h"
#include "Hooking/GroupClassHook.h"
#include "Hooking/GroupVirtualHook.h"
#include "Hooking/GroupGlobalVirtualHook.h"
#include "PluginBase/Modules.h"

#include <memory>

class C_BasePlayer;
class C_HLTVCamera;
enum ERenderDepthMode : int;
enum OverrideType_t : int;
class QAngle;
class ICvar;
class C_BaseEntity;
struct model_t;
class IVEngineClient;
struct player_info_s;
typedef player_info_s player_info_t;
class IGameEventManager2;
class IGameEvent;
class IClientEngineTools;
class Vector;
class IClientNetworkable;
class CBaseEntityList;
class IHandleEntity;
class IPrediction;
class C_BaseAnimating;
class CStudioHdr;
class CBoneCache;
struct matrix3x4_t;
class CGlowObjectManager;
class CViewSetup;
class CMatRenderContextPtr;
class CHudTexture;
class IStudioRender;
class IMaterial;
class IClientRenderable;
class C_TFPlayer;

namespace vgui
{
	class EditablePanel;
	class ImagePanel;
	class ProgressBar;
}

class HookManager final
{
	enum class Func
	{
		ICvar_ConsoleColorPrintf,
		ICvar_ConsoleDPrintf,
		ICvar_ConsolePrintf,

		IClientEngineTools_InToolMode,
		IClientEngineTools_IsThirdPersonCamera,
		IClientEngineTools_SetupEngineView,

		IVEngineClient_GetPlayerInfo,

		IGameEventManager2_FireEventClientSide,

		IPrediction_PostEntityPacketReceived,

		IStudioRender_ForcedMaterialOverride,

		IClientRenderable_DrawModel,

		C_HLTVCamera_SetCameraAngle,
		C_HLTVCamera_SetMode,
		C_HLTVCamera_SetPrimaryTarget,

		CHudBaseDeathNotice_GetIcon,

		C_BaseAnimating_GetBoneCache,
		C_BaseAnimating_LockStudioHdr,
		C_BaseAnimating_LookupBone,
		C_BaseAnimating_GetBonePosition,
		C_BaseAnimating_DrawModel,
		C_BaseAnimating_InternalDrawModel,
		C_BasePlayer_GetDefaultFOV,
		C_BasePlayer_GetFOV,
		C_TFPlayer_DrawModel,
		vgui_ProgressBar_ApplySettings,

		C_BaseEntity_Init,
		C_BaseEntity_CalcAbsolutePosition,

		CGlowObjectManager_ApplyEntityGlowEffects,

		Global_CreateEntityByName,
		Global_GetLocalPlayerIndex,
		Global_CreateTFGlowObject,
		Global_UTILComputeEntityFade,
		Global_DrawOpaqueRenderable,
		Global_DrawTranslucentRenderable,

		Count,
	};

	enum ShimType;
	template<class HookType, class... Args> class HookShim final :
		public Hooking::BaseGroupHook<ShimType, (ShimType)HookType::HOOK_ID, typename HookType::Functional, typename HookType::RetVal, Args...>
	{
	public:
		~HookShim()
		{
			if (m_InnerHook)
				DetachHook();

			m_HooksTable.clear();
		}

		Functional GetOriginal() override { return m_InnerHook->GetOriginal(); }
		Hooking::HookType GetType() const override { return m_InnerHook->GetType(); }

		int AddHook(const Functional& newHook) override
		{
			const auto retVal = BaseGroupHookType::AddHook(newHook);
			AddInnerHook(retVal, newHook);
			return retVal;
		}
		bool RemoveHook(int hookID, const char* funcName) override
		{
			bool retVal = BaseGroupHookType::RemoveHook(hookID, funcName);
			RemoveInnerHook(hookID, funcName);
			return retVal;
		}

		void SetState(Hooking::HookAction action) override { Assert(m_InnerHook); m_InnerHook->SetState(action); }

		friend class HookManager;

		void InitHook() override { }
		int GetUniqueHookID() const { return (int)HOOK_ID; }

		typedef HookType Inner;
		void AttachHook(const std::shared_ptr<HookType>& innerHook)
		{
			Assert(!m_InnerHook);

			std::lock_guard<decltype(m_Mutex)> lock(m_Mutex);
			m_InnerHook = innerHook;

			std::lock_guard<decltype(m_HooksTableMutex)> hooksTableLock(m_HooksTableMutex);
			for (auto hooks : m_HooksTable)
				AddInnerHook(hooks.first, hooks.second);
		}

	private:
		void DetachHook()
		{
			Assert(m_InnerHook);

			std::lock_guard<decltype(m_Mutex)> lock(m_Mutex);
			for (auto hookID : m_ActiveHooks)
				m_InnerHook->RemoveHook(hookID.second, __FUNCSIG__);

			m_ActiveHooks.clear();
			m_InnerHook.reset();
		}

		void AddInnerHook(uint64 fakeHookID, const Functional& newHook)
		{
			std::lock_guard<decltype(m_Mutex)> lock(m_Mutex);
			if (m_InnerHook)
			{
				const auto current = m_InnerHook->AddHook(newHook);
				m_ActiveHooks.insert(std::make_pair(fakeHookID, current));
			}
		}
		void RemoveInnerHook(uint64 hookID, const char* funcName)
		{
			std::lock_guard<decltype(m_Mutex)> lock(m_Mutex);
			if (m_InnerHook)
			{
				const auto& link = m_ActiveHooks.find(hookID);
				if (link != m_ActiveHooks.end())
				{
					m_InnerHook->RemoveHook(link->second, funcName);
					m_ActiveHooks.erase(link);
				}
			}
		}

		std::recursive_mutex m_Mutex;

		std::shared_ptr<HookType> m_InnerHook;
		std::map<uint64, uint64> m_ActiveHooks;
	};

	template<Func fn, bool vaArgs, class Type, class RetVal, class... Args> using VirtualHook =
		HookShim<Hooking::GroupVirtualHook<Func, fn, vaArgs, Type, RetVal, Args...>, Args...>;
	template<Func fn, bool vaArgs, class Type, class RetVal, class... Args> using GlobalVirtualHook =
		HookShim<Hooking::GroupGlobalVirtualHook<Func, fn, vaArgs, Type, RetVal, Args...>, Args...>;
	template<Func fn, bool vaArgs, class Type, class RetVal, class... Args> using ClassHook =
		HookShim<Hooking::GroupClassHook<Func, fn, vaArgs, Type, RetVal, Args...>, Args...>;
	template<Func fn, bool vaArgs, class RetVal, class... Args> using GlobalHook =
		HookShim<Hooking::GroupGlobalHook<Func, fn, vaArgs, RetVal, Args...>, Args...>;
	template<Func fn, bool vaArgs, class Type, class RetVal, class... Args> using GlobalClassHook =
		HookShim<Hooking::GroupManualClassHook<Func, fn, vaArgs, Type, RetVal, Args...>, Type*, Args...>;

	typedef void(__thiscall *RawSetCameraAngleFn)(C_HLTVCamera*, const QAngle&);
	typedef void(__thiscall *RawSetModeFn)(C_HLTVCamera*, int);
	typedef void(__thiscall *RawSetPrimaryTargetFn)(C_HLTVCamera*, int);
	typedef CBoneCache*(__thiscall *RawGetBoneCacheFn)(C_BaseAnimating*, CStudioHdr*);
	typedef void(__thiscall *RawLockStudioHdrFn)(C_BaseAnimating*);
	typedef void(__thiscall *RawCalcAbsolutePositionFn)(C_BaseEntity*);
	typedef int(__thiscall *RawLookupBoneFn)(C_BaseAnimating*, const char*);
	typedef void(__thiscall *RawGetBonePositionFn)(C_BaseAnimating*, int, Vector&, QAngle&);
	typedef int(__thiscall *RawBaseAnimatingDrawModelFn)(C_BaseAnimating*, int);
	typedef int(__thiscall *Raw_C_BaseAnimating_InternalDrawModel)(C_BaseAnimating* pThis, int flags);
	typedef int(__thiscall *RawTFPlayerDrawModelFn)(C_TFPlayer*, int);
	typedef int(*RawGetLocalPlayerIndexFn)();
	typedef C_BaseEntity*(__cdecl *RawCreateEntityByNameFn)(const char* entityName);
	typedef IClientNetworkable*(__cdecl *RawCreateTFGlowObjectFn)(int entNum, int serialNum);
	typedef bool(__thiscall *RawBaseEntityInitFn)(C_BaseEntity* pThis, int entnum, int iSerialNum);
	typedef unsigned char(__cdecl *RawUTILComputeEntityFadeFn)(C_BaseEntity* pEntity, float flMinDist, float flMaxDist, float flFadeScale);
	typedef void(__cdecl *RawDrawOpaqueRenderableFn)(IClientRenderable* pEnt, bool bTwoPass, ERenderDepthMode DepthMode, int nDefaultFlags);
	typedef void(__cdecl *RawDrawTranslucentRenderableFn)(IClientRenderable* pEnt, bool bTwoPass, bool bShadowDepth, bool bIgnoreDepth);
	typedef void(__thiscall *RawApplyEntityGlowEffectsFn)(CGlowObjectManager*, const CViewSetup*, int, CMatRenderContextPtr&, float, int, int, int, int);
	typedef CHudTexture*(__stdcall *RawGetIconFn)(const char* szIcon, int eIconFormat);
	typedef bool(*RawShouldDrawLocalPlayerFn)();
	typedef KeyValues*(__thiscall *Raw_EditablePanel_GetDialogVariables)(vgui::EditablePanel* pThis);
	typedef void(__thiscall *Raw_ImagePanel_SetImage)(vgui::ImagePanel* pThis, const char* imageName);
	typedef void(__thiscall *Raw_ProgressBar_ApplySettings)(vgui::ProgressBar* pThis, KeyValues* pSettings);
	typedef bool(__thiscall *Raw_C_BaseAnimating_ComputeHitboxSurroundingBox)(C_BaseAnimating* pThis, Vector* pVecWorldMins, Vector* pVecWorldMaxs);
	typedef int(__thiscall *Raw_C_BasePlayer_GetDefaultFOV)(C_BasePlayer* pThis);
	typedef float(__thiscall *Raw_C_BasePlayer_GetFOV)(C_BasePlayer* pThis);

	typedef bool(__cdecl *Raw_RenderBox)(const Vector& origin, const QAngle& angles, const Vector& mins, const Vector& maxs, Color c, bool zBuffer, bool insideOut);
	typedef bool(__cdecl *Raw_RenderBox_1)(const Vector& origin, const QAngle& angles, const Vector& mins, const Vector& maxs, Color c, IMaterial* material, bool insideOut);
	typedef bool(__cdecl *Raw_RenderWireframeBox)(const Vector& origin, const QAngle& angles, const Vector& mins, const Vector& maxs, Color c, bool zBuffer);
	typedef bool(__cdecl *Raw_RenderLine)(const Vector& p0, const Vector& p1, Color c, bool zBuffer);
	typedef bool(__cdecl *Raw_RenderTriangle)(const Vector& p0, const Vector& p1, const Vector& p2, Color c, bool zBuffer);


	static RawSetCameraAngleFn GetRawFunc_C_HLTVCamera_SetCameraAngle();
	static RawSetModeFn GetRawFunc_C_HLTVCamera_SetMode();
	static RawSetPrimaryTargetFn GetRawFunc_C_HLTVCamera_SetPrimaryTarget();
	static RawGetBoneCacheFn GetRawFunc_C_BaseAnimating_GetBoneCache();
	static RawLockStudioHdrFn GetRawFunc_C_BaseAnimating_LockStudioHdr();
	static RawCalcAbsolutePositionFn GetRawFunc_C_BaseEntity_CalcAbsolutePosition();
	static RawLookupBoneFn GetRawFunc_C_BaseAnimating_LookupBone();
	static RawGetBonePositionFn GetRawFunc_C_BaseAnimating_GetBonePosition();
	static RawBaseAnimatingDrawModelFn GetRawFunc_C_BaseAnimating_DrawModel();
	static Raw_C_BaseAnimating_InternalDrawModel GetRawFunc_C_BaseAnimating_InternalDrawModel();
	static RawTFPlayerDrawModelFn GetRawFunc_C_TFPlayer_DrawModel();
	static RawGetLocalPlayerIndexFn GetRawFunc_Global_GetLocalPlayerIndex();
	static RawCreateEntityByNameFn GetRawFunc_Global_CreateEntityByName();
	static RawBaseEntityInitFn GetRawFunc_C_BaseEntity_Init();
	static RawUTILComputeEntityFadeFn GetRawFunc_Global_UTILComputeEntityFade();
	static RawDrawOpaqueRenderableFn GetRawFunc_Global_DrawOpaqueRenderable();
	static RawDrawTranslucentRenderableFn GetRawFunc_Global_DrawTranslucentRenderable();
	static RawApplyEntityGlowEffectsFn GetRawFunc_CGlowObjectManager_ApplyEntityGlowEffects();
	static RawGetIconFn GetRawFunc_CHudBaseDeathNotice_GetIcon();
	static Raw_ProgressBar_ApplySettings GetRawFunc_ProgressBar_ApplySettings();
	static Raw_C_BasePlayer_GetDefaultFOV GetRawFunc_C_BasePlayer_GetDefaultFOV();

public:
	HookManager();
	static RawCreateTFGlowObjectFn GetRawFunc_Global_CreateTFGlowObject();
	static RawShouldDrawLocalPlayerFn GetRawFunc_C_BasePlayer_ShouldDrawLocalPlayer();
	static Raw_EditablePanel_GetDialogVariables GetRawFunc_EditablePanel_GetDialogVariables();
	static Raw_ImagePanel_SetImage GetRawFunc_ImagePanel_SetImage();
	static Raw_C_BaseAnimating_ComputeHitboxSurroundingBox GetRawFunc_C_BaseAnimating_ComputeHitboxSurroundingBox();

	static Raw_RenderBox GetRawFunc_RenderBox();
	static Raw_RenderBox_1 GetRawFunc_RenderBox_1();
	static Raw_RenderWireframeBox GetRawFunc_RenderWireframeBox();
	static Raw_RenderLine GetRawFunc_RenderLine();
	static Raw_RenderTriangle GetRawFunc_RenderTriangle();
	static Raw_C_BasePlayer_GetFOV GetRawFunc_C_BasePlayer_GetFOV();

	static bool Load();
	static bool Unload();

	typedef VirtualHook<Func::ICvar_ConsoleColorPrintf, true, ICvar, void, const Color&, const char*> ICvar_ConsoleColorPrintf;
	typedef VirtualHook<Func::ICvar_ConsoleDPrintf, true, ICvar, void, const char*> ICvar_ConsoleDPrintf;
	typedef VirtualHook<Func::ICvar_ConsolePrintf, true, ICvar, void, const char*> ICvar_ConsolePrintf;

	typedef VirtualHook<Func::IClientEngineTools_InToolMode, false, IClientEngineTools, bool> IClientEngineTools_InToolMode;
	typedef VirtualHook<Func::IClientEngineTools_IsThirdPersonCamera, false, IClientEngineTools, bool> IClientEngineTools_IsThirdPersonCamera;
	typedef VirtualHook<Func::IClientEngineTools_SetupEngineView, false, IClientEngineTools, bool, Vector&, QAngle&, float&> IClientEngineTools_SetupEngineView;

	typedef GlobalVirtualHook<Func::IClientRenderable_DrawModel, false, IClientRenderable, int, int> IClientRenderable_DrawModel;

	typedef VirtualHook<Func::IVEngineClient_GetPlayerInfo, false, IVEngineClient, bool, int, player_info_t*> IVEngineClient_GetPlayerInfo;

	typedef VirtualHook<Func::IGameEventManager2_FireEventClientSide, false, IGameEventManager2, bool, IGameEvent*> IGameEventManager2_FireEventClientSide;

	typedef VirtualHook<Func::IPrediction_PostEntityPacketReceived, false, IPrediction, void> IPrediction_PostEntityPacketReceived;

	typedef VirtualHook<Func::IStudioRender_ForcedMaterialOverride, false, IStudioRender, void, IMaterial*, OverrideType_t> IStudioRender_ForcedMaterialOverride;

	typedef ClassHook<Func::C_HLTVCamera_SetCameraAngle, false, C_HLTVCamera, void, const QAngle&> C_HLTVCamera_SetCameraAngle;
	typedef ClassHook<Func::C_HLTVCamera_SetMode, false, C_HLTVCamera, void, int> C_HLTVCamera_SetMode;
	typedef ClassHook<Func::C_HLTVCamera_SetPrimaryTarget, false, C_HLTVCamera, void, int> C_HLTVCamera_SetPrimaryTarget;

	typedef GlobalHook<Func::CHudBaseDeathNotice_GetIcon, false, CHudTexture*, const char*, int> CHudBaseDeathNotice_GetIcon;

	typedef GlobalClassHook<Func::C_BaseAnimating_GetBoneCache, false, C_BaseAnimating, CBoneCache*, CStudioHdr*> C_BaseAnimating_GetBoneCache;
	typedef GlobalClassHook<Func::C_BaseAnimating_LockStudioHdr, false, C_BaseAnimating, void> C_BaseAnimating_LockStudioHdr;
	typedef GlobalClassHook<Func::C_BaseAnimating_LookupBone, false, C_BaseAnimating, int, const char*> C_BaseAnimating_LookupBone;
	typedef GlobalClassHook<Func::C_BaseAnimating_GetBonePosition, false, C_BaseAnimating, void, int, Vector&, QAngle&> C_BaseAnimating_GetBonePosition;
	typedef GlobalClassHook<Func::C_BaseAnimating_DrawModel, false, C_BaseAnimating, int, int> C_BaseAnimating_DrawModel;
	typedef GlobalClassHook<Func::C_BaseAnimating_InternalDrawModel, false, C_BaseAnimating, int, int> C_BaseAnimating_InternalDrawModel;

	typedef GlobalClassHook<Func::C_BasePlayer_GetDefaultFOV, false, C_BasePlayer, int> C_BasePlayer_GetDefaultFOV;

	typedef GlobalClassHook<Func::C_TFPlayer_DrawModel, false, C_TFPlayer, int, int> C_TFPlayer_DrawModel;

	typedef GlobalClassHook<Func::vgui_ProgressBar_ApplySettings, false, vgui::ProgressBar, void, KeyValues*> vgui_ProgressBar_ApplySettings;

	typedef GlobalClassHook<Func::C_BaseEntity_Init, false, C_BaseEntity, bool, int, int> C_BaseEntity_Init;
	typedef GlobalClassHook<Func::C_BaseEntity_CalcAbsolutePosition, false, C_BaseEntity, void> C_BaseEntity_CalcAbsolutePosition;

	typedef GlobalClassHook<Func::CGlowObjectManager_ApplyEntityGlowEffects, false, CGlowObjectManager, void, const CViewSetup*, int, CMatRenderContextPtr&, float, int, int, int, int> CGlowObjectManager_ApplyEntityGlowEffects;

	typedef GlobalHook<Func::Global_GetLocalPlayerIndex, false, int> Global_GetLocalPlayerIndex;
	typedef GlobalHook<Func::Global_CreateEntityByName, false, C_BaseEntity*, const char*> Global_CreateEntityByName;
	typedef GlobalHook<Func::Global_CreateTFGlowObject, false, IClientNetworkable*, int, int> Global_CreateTFGlowObject;
	typedef GlobalHook<Func::Global_UTILComputeEntityFade, false, unsigned char, C_BaseEntity*, float, float, float> Global_UTILComputeEntityFade;
	typedef GlobalHook<Func::Global_DrawOpaqueRenderable, false, void, IClientRenderable*, bool, ERenderDepthMode, int> Global_DrawOpaqueRenderable;
	typedef GlobalHook<Func::Global_DrawTranslucentRenderable, false, void, IClientRenderable*, bool, bool, bool> Global_DrawTranslucentRenderable;

	template<class Hook> typename Hook::Functional GetFunc() { static_assert(false, "Invalid hook type"); }

	template<class Hook> Hook* GetHook() { static_assert(false, "Invalid hook type"); }
	template<> ICvar_ConsoleColorPrintf* GetHook<ICvar_ConsoleColorPrintf>() { return &m_Hook_ICvar_ConsoleColorPrintf; }
	template<> ICvar_ConsoleDPrintf* GetHook<ICvar_ConsoleDPrintf>() { return &m_Hook_ICvar_ConsoleDPrintf; }
	template<> ICvar_ConsolePrintf* GetHook<ICvar_ConsolePrintf>() { return &m_Hook_ICvar_ConsolePrintf; }
	template<> IClientEngineTools_InToolMode* GetHook<IClientEngineTools_InToolMode>() { return &m_Hook_IClientEngineTools_InToolMode; }
	template<> IClientEngineTools_IsThirdPersonCamera* GetHook<IClientEngineTools_IsThirdPersonCamera>() { return &m_Hook_IClientEngineTools_IsThirdPersonCamera; }
	template<> IClientEngineTools_SetupEngineView* GetHook<IClientEngineTools_SetupEngineView>() { return &m_Hook_IClientEngineTools_SetupEngineView; }
	template<> IClientRenderable_DrawModel* GetHook<IClientRenderable_DrawModel>() { return &m_Hook_IClientRenderable_DrawModel; }
	template<> IVEngineClient_GetPlayerInfo* GetHook<IVEngineClient_GetPlayerInfo>() { return &m_Hook_IVEngineClient_GetPlayerInfo; }
	template<> IGameEventManager2_FireEventClientSide* GetHook<IGameEventManager2_FireEventClientSide>() { return &m_Hook_IGameEventManager2_FireEventClientSide; }
	template<> IPrediction_PostEntityPacketReceived* GetHook<IPrediction_PostEntityPacketReceived>() { return &m_Hook_IPrediction_PostEntityPacketReceived; }
	template<> IStudioRender_ForcedMaterialOverride* GetHook<IStudioRender_ForcedMaterialOverride>() { return &m_Hook_IStudioRender_ForcedMaterialOverride; }
	template<> C_HLTVCamera_SetCameraAngle* GetHook<C_HLTVCamera_SetCameraAngle>() { return &m_Hook_C_HLTVCamera_SetCameraAngle; }
	template<> C_HLTVCamera_SetMode* GetHook<C_HLTVCamera_SetMode>() { return &m_Hook_C_HLTVCamera_SetMode; }
	template<> C_HLTVCamera_SetPrimaryTarget* GetHook<C_HLTVCamera_SetPrimaryTarget>() { return &m_Hook_C_HLTVCamera_SetPrimaryTarget; }
	template<> C_BaseAnimating_DrawModel* GetHook<C_BaseAnimating_DrawModel>() { return &m_Hook_C_BaseAnimating_DrawModel; }
	template<> C_BaseAnimating_InternalDrawModel* GetHook<C_BaseAnimating_InternalDrawModel>() { return &m_Hook_C_BaseAnimating_InternalDrawModel; }
	template<> C_BasePlayer_GetDefaultFOV* GetHook<C_BasePlayer_GetDefaultFOV>() { return &m_Hook_C_BasePlayer_GetDefaultFOV; }
	template<> C_TFPlayer_DrawModel* GetHook<C_TFPlayer_DrawModel>() { return &m_Hook_C_TFPlayer_DrawModel; }
	template<> C_BaseEntity_Init* GetHook<C_BaseEntity_Init>() { return &m_Hook_C_BaseEntity_Init; }
	template<> Global_CreateEntityByName* GetHook<Global_CreateEntityByName>() { return &m_Hook_Global_CreateEntityByName; }
	template<> Global_GetLocalPlayerIndex* GetHook<Global_GetLocalPlayerIndex>() { return &m_Hook_Global_GetLocalPlayerIndex; }
	template<> Global_UTILComputeEntityFade* GetHook<Global_UTILComputeEntityFade>() { return &m_Hook_Global_UTILComputeEntityFade; }
	template<> Global_DrawOpaqueRenderable* GetHook<Global_DrawOpaqueRenderable>() { return &m_Hook_Global_DrawOpaqueRenderable; }
	template<> Global_DrawTranslucentRenderable* GetHook<Global_DrawTranslucentRenderable>() { return &m_Hook_Global_DrawTranslucentRenderable; }
	template<> CGlowObjectManager_ApplyEntityGlowEffects* GetHook<CGlowObjectManager_ApplyEntityGlowEffects>() { return &m_Hook_CGlowObjectManager_ApplyEntityGlowEffects; }
	template<> vgui_ProgressBar_ApplySettings* GetHook<vgui_ProgressBar_ApplySettings>() { return &m_Hook_vgui_ProgressBar_ApplySettings; }

	template<class Hook> int AddHook(const typename Hook::Functional& hook)
	{
		auto hkPtr = GetHook<Hook>();
		if (!hkPtr)
			return 0;

		return hkPtr->AddHook(hook);
	}
	template<class Hook> bool RemoveHook(int hookID, const char* funcName)
	{
		auto hkPtr = GetHook<Hook>();
		if (!hkPtr)
			return false;

		return hkPtr->RemoveHook(hookID, funcName);
	}
	template<class Hook> typename Hook::Functional GetOriginal()
	{
		auto hkPtr = GetHook<Hook>();
		if (!hkPtr)
			return nullptr;

		return hkPtr->GetOriginal();
	}
	template<class Hook> void SetState(Hooking::HookAction state)
	{
		auto hkPtr = GetHook<Hook>();
		if (!hkPtr)
			return;

		hkPtr->SetState(state);
	}

private:
	ICvar_ConsoleColorPrintf m_Hook_ICvar_ConsoleColorPrintf;
	ICvar_ConsoleDPrintf m_Hook_ICvar_ConsoleDPrintf;
	ICvar_ConsolePrintf m_Hook_ICvar_ConsolePrintf;

	IClientEngineTools_InToolMode m_Hook_IClientEngineTools_InToolMode;
	IClientEngineTools_IsThirdPersonCamera m_Hook_IClientEngineTools_IsThirdPersonCamera;
	IClientEngineTools_SetupEngineView m_Hook_IClientEngineTools_SetupEngineView;

	IClientRenderable_DrawModel m_Hook_IClientRenderable_DrawModel;

	IVEngineClient_GetPlayerInfo m_Hook_IVEngineClient_GetPlayerInfo;

	IGameEventManager2_FireEventClientSide m_Hook_IGameEventManager2_FireEventClientSide;

	IPrediction_PostEntityPacketReceived m_Hook_IPrediction_PostEntityPacketReceived;

	IStudioRender_ForcedMaterialOverride m_Hook_IStudioRender_ForcedMaterialOverride;

	C_HLTVCamera_SetCameraAngle m_Hook_C_HLTVCamera_SetCameraAngle;
	C_HLTVCamera_SetMode m_Hook_C_HLTVCamera_SetMode;
	C_HLTVCamera_SetPrimaryTarget m_Hook_C_HLTVCamera_SetPrimaryTarget;

	C_BaseAnimating_DrawModel m_Hook_C_BaseAnimating_DrawModel;
	C_BaseAnimating_InternalDrawModel m_Hook_C_BaseAnimating_InternalDrawModel;
	C_BasePlayer_GetDefaultFOV m_Hook_C_BasePlayer_GetDefaultFOV;
	C_TFPlayer_DrawModel m_Hook_C_TFPlayer_DrawModel;
	vgui_ProgressBar_ApplySettings m_Hook_vgui_ProgressBar_ApplySettings;

	C_BaseEntity_Init m_Hook_C_BaseEntity_Init;

	Global_CreateEntityByName m_Hook_Global_CreateEntityByName;
	Global_GetLocalPlayerIndex m_Hook_Global_GetLocalPlayerIndex;
	Global_UTILComputeEntityFade m_Hook_Global_UTILComputeEntityFade;
	Global_DrawOpaqueRenderable m_Hook_Global_DrawOpaqueRenderable;
	Global_DrawTranslucentRenderable m_Hook_Global_DrawTranslucentRenderable;
	CGlowObjectManager_ApplyEntityGlowEffects m_Hook_CGlowObjectManager_ApplyEntityGlowEffects;

	void IngameStateChanged(bool inGame);
	class Panel;
	std::unique_ptr<Panel> m_Panel;

	// Passthrough from Interfaces so we don't have to #include "Interfaces.h" yet
	static C_HLTVCamera* GetHLTVCamera();
	static CBaseEntityList* GetBaseEntityList();
};

using ICvar_ConsoleColorPrintf = HookManager::ICvar_ConsoleColorPrintf;
using ICvar_ConsoleDPrintf = HookManager::ICvar_ConsoleDPrintf;
using ICvar_ConsolePrintf = HookManager::ICvar_ConsolePrintf;

using IClientEngineTools_InToolMode = HookManager::IClientEngineTools_InToolMode;
using IClientEngineTools_IsThirdPersonCamera = HookManager::IClientEngineTools_IsThirdPersonCamera;
using IClientEngineTools_SetupEngineView = HookManager::IClientEngineTools_SetupEngineView;

using IClientRenderable_DrawModel = HookManager::IClientRenderable_DrawModel;

using IVEngineClient_GetPlayerInfo = HookManager::IVEngineClient_GetPlayerInfo;

using IGameEventManager2_FireEventClientSide = HookManager::IGameEventManager2_FireEventClientSide;

using IPrediction_PostEntityPacketReceived = HookManager::IPrediction_PostEntityPacketReceived;

using IStudioRender_ForcedMaterialOverride = HookManager::IStudioRender_ForcedMaterialOverride;

using C_HLTVCamera_SetCameraAngle = HookManager::C_HLTVCamera_SetCameraAngle;
using C_HLTVCamera_SetMode = HookManager::C_HLTVCamera_SetMode;
using C_HLTVCamera_SetPrimaryTarget = HookManager::C_HLTVCamera_SetPrimaryTarget;

using CHudBaseDeathNotice_GetIcon = HookManager::CHudBaseDeathNotice_GetIcon;

using C_BaseAnimating_GetBoneCache = HookManager::C_BaseAnimating_GetBoneCache;
using C_BaseAnimating_LockStudioHdr = HookManager::C_BaseAnimating_LockStudioHdr;
using C_BaseAnimating_LookupBone = HookManager::C_BaseAnimating_LookupBone;
using C_BaseAnimating_GetBonePosition = HookManager::C_BaseAnimating_GetBonePosition;
using C_BaseAnimating_DrawModel = HookManager::C_BaseAnimating_DrawModel;
using C_BaseAnimating_InternalDrawModel = HookManager::C_BaseAnimating_InternalDrawModel;
using C_BasePlayer_GetDefaultFOV = HookManager::C_BasePlayer_GetDefaultFOV;
using C_TFPlayer_DrawModel = HookManager::C_TFPlayer_DrawModel;
using vgui_ProgressBar_ApplySettings = HookManager::vgui_ProgressBar_ApplySettings;

using C_BaseEntity_Init = HookManager::C_BaseEntity_Init;
using C_BaseEntity_CalcAbsolutePosition = HookManager::C_BaseEntity_CalcAbsolutePosition;

using CGlowObjectManager_ApplyEntityGlowEffects = HookManager::CGlowObjectManager_ApplyEntityGlowEffects;

using Global_GetLocalPlayerIndex = HookManager::Global_GetLocalPlayerIndex;
using Global_CreateEntityByName = HookManager::Global_CreateEntityByName;
using Global_CreateTFGlowObject = HookManager::Global_CreateTFGlowObject;
using Global_UTILComputeEntityFade = HookManager::Global_UTILComputeEntityFade;
using Global_DrawOpaqueRenderable = HookManager::Global_DrawOpaqueRenderable;
using Global_DrawTranslucentRenderable = HookManager::Global_DrawTranslucentRenderable;

extern void* SignatureScan(const char* moduleName, const char* signature, const char* mask);
extern HookManager* GetHooks();

template<> inline C_HLTVCamera_SetCameraAngle::Functional HookManager::GetFunc<C_HLTVCamera_SetCameraAngle>()
{
	return std::bind(
		[](RawSetCameraAngleFn func, C_HLTVCamera* pThis, const QAngle& ang) { func(pThis, ang); },
		GetRawFunc_C_HLTVCamera_SetCameraAngle(), GetHLTVCamera(), std::placeholders::_1);
}
template<> inline HookManager::C_HLTVCamera_SetMode::Functional HookManager::GetFunc<HookManager::C_HLTVCamera_SetMode>()
{
	return std::bind(
		[](int mode) { GetRawFunc_C_HLTVCamera_SetMode()(GetHLTVCamera(), mode); },
		std::placeholders::_1);
}
template<> inline C_HLTVCamera_SetPrimaryTarget::Functional HookManager::GetFunc<C_HLTVCamera_SetPrimaryTarget>()
{
	return std::bind(
		[](RawSetPrimaryTargetFn func, C_HLTVCamera* pThis, int target) { func(pThis, target); },
		GetRawFunc_C_HLTVCamera_SetPrimaryTarget(), GetHLTVCamera(), std::placeholders::_1);
}

template<> inline CHudBaseDeathNotice_GetIcon::Functional HookManager::GetFunc<CHudBaseDeathNotice_GetIcon>()
{
	return std::bind(
		GetRawFunc_CHudBaseDeathNotice_GetIcon(),
		std::placeholders::_1, std::placeholders::_2);
}

template<> inline C_BaseAnimating_GetBoneCache::Functional HookManager::GetFunc<C_BaseAnimating_GetBoneCache>()
{
	return std::bind(
		[](RawGetBoneCacheFn func, C_BaseAnimating* pThis, CStudioHdr* pStudioHdr) { return func(pThis, pStudioHdr); },
		GetRawFunc_C_BaseAnimating_GetBoneCache(), std::placeholders::_1, std::placeholders::_2);
}
template<> inline C_BaseAnimating_LockStudioHdr::Functional HookManager::GetFunc<C_BaseAnimating_LockStudioHdr>()
{
	return std::bind(
		[](RawLockStudioHdrFn func, C_BaseAnimating* pThis) { func(pThis); },
		GetRawFunc_C_BaseAnimating_LockStudioHdr(), std::placeholders::_1);
}
template<> inline C_BaseAnimating_LookupBone::Functional HookManager::GetFunc<C_BaseAnimating_LookupBone>()
{
	return std::bind(
		[](RawLookupBoneFn func, C_BaseAnimating* pThis, const char* szName) { return func(pThis, szName); },
		GetRawFunc_C_BaseAnimating_LookupBone(), std::placeholders::_1, std::placeholders::_2);
}
template<> inline C_BaseAnimating_GetBonePosition::Functional HookManager::GetFunc<C_BaseAnimating_GetBonePosition>()
{
	return std::bind(
		[](RawGetBonePositionFn func, C_BaseAnimating* pThis, int iBone, Vector& origin, QAngle& angles) { return func(pThis, iBone, origin, angles); },
		GetRawFunc_C_BaseAnimating_GetBonePosition(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
}

template<> inline C_BaseEntity_CalcAbsolutePosition::Functional HookManager::GetFunc<C_BaseEntity_CalcAbsolutePosition>()
{
	return std::bind(
		[](RawCalcAbsolutePositionFn func, C_BaseEntity* pThis) { func(pThis); },
		GetRawFunc_C_BaseEntity_CalcAbsolutePosition(), std::placeholders::_1);
}

template<> inline CGlowObjectManager_ApplyEntityGlowEffects::Functional HookManager::GetFunc<CGlowObjectManager_ApplyEntityGlowEffects>()
{
	return std::bind(
		[](RawApplyEntityGlowEffectsFn func, CGlowObjectManager* pThis, const CViewSetup* pSetup, int nSplitScreenSlot, CMatRenderContextPtr& pRenderContext, float flBloomScale, int x, int y, int w, int h) { return func(pThis, pSetup, nSplitScreenSlot, pRenderContext, flBloomScale, x, y, w, h); },
		GetRawFunc_CGlowObjectManager_ApplyEntityGlowEffects(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7, std::placeholders::_8, std::placeholders::_9);
}

template<> inline Global_GetLocalPlayerIndex::Functional HookManager::GetFunc<Global_GetLocalPlayerIndex>()
{
	return std::bind(GetRawFunc_Global_GetLocalPlayerIndex());
}

template<> inline Global_CreateEntityByName::Functional HookManager::GetFunc<Global_CreateEntityByName>()
{
	return std::bind(GetRawFunc_Global_CreateEntityByName(), std::placeholders::_1);
}

template<> inline Global_CreateTFGlowObject::Functional HookManager::GetFunc<Global_CreateTFGlowObject>()
{
	return std::bind(GetRawFunc_Global_CreateTFGlowObject(), std::placeholders::_1, std::placeholders::_2);
}