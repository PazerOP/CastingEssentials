#pragma once
#include "Hooking/GroupGlobalHook.h"
#include "Hooking/GroupManualClassHook.h"
#include "Hooking/GroupClassHook.h"
#include "Hooking/GroupVirtualHook.h"
#include "Hooking/GroupGlobalVirtualHook.h"

#include <memory>

class C_BaseCombatCharacter;
class C_BasePlayer;
class C_HLTVCamera;
class CAccountPanel;
class CAutoGameSystemPerFrame;
class CDamageAccountPanel;
class IGameSystem;
class INetworkStringTable;
using trace_t = class CGameTrace;
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

enum class HookFunc
{
	Global_CreateEntityByName,
	Global_CreateTFGlowObject,
	Global_DrawOpaqueRenderable,
	Global_DrawTranslucentRenderable,
	Global_GetLocalPlayerIndex,
	Global_GetVectorInScreenSpace,
	Global_UserInfoChangedCallback,
	Global_UTILComputeEntityFade,
	Global_UTIL_TraceLine,

	C_BaseAnimating_ComputeHitboxSurroundingBox,
	C_BaseAnimating_DrawModel,
	C_BaseAnimating_GetBoneCache,
	C_BaseAnimating_GetBonePosition,
	C_BaseAnimating_InternalDrawModel,
	C_BaseAnimating_LockStudioHdr,
	C_BaseAnimating_LookupBone,

	C_BaseEntity_Init,
	C_BaseEntity_CalcAbsolutePosition,

	C_BasePlayer_GetDefaultFOV,
	C_BasePlayer_GetFOV,
	C_BasePlayer_GetLocalPlayer,
	C_BasePlayer_ShouldDrawLocalPlayer,

	C_HLTVCamera_SetCameraAngle,
	C_HLTVCamera_SetMode,
	C_HLTVCamera_SetPrimaryTarget,

	C_TFPlayer_DrawModel,

	CAccountPanel_OnAccountValueChanged,
	CAccountPanel_Paint,
	CAutoGameSystemPerFrame_CAutoGameSystemPerFrame,
	CDamageAccountPanel_DisplayDamageFeedback,
	CDamageAccountPanel_ShouldDraw,

	CGlowObjectManager_ApplyEntityGlowEffects,

	CHudBaseDeathNotice_GetIcon,

	IClientEngineTools_InToolMode,
	IClientEngineTools_IsThirdPersonCamera,
	IClientEngineTools_SetupEngineView,

	IClientRenderable_DrawModel,

	ICvar_ConsoleColorPrintf,
	ICvar_ConsoleDPrintf,
	ICvar_ConsolePrintf,

	IGameEventManager2_FireEventClientSide,
	IGameSystem_Add,
	IGameSystem_Remove,

	IPrediction_PostEntityPacketReceived,

	IStudioRender_ForcedMaterialOverride,

	IVEngineClient_GetPlayerInfo,

	vgui_EditablePanel_GetDialogVariables,
	vgui_ImagePanel_SetImage,
	vgui_ProgressBar_ApplySettings,

	Count,
};

class HookManager final
{
	enum ShimType;
	template<class HookType, class... Args> class HookShim final :
		public Hooking::BaseGroupHook<ShimType, (ShimType)HookType::HOOK_ID, typename HookType::Functional, typename HookType::RetVal, Args...>
	{
		using Functional = typename HookType::Functional;
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

	template<HookFunc fn, bool vaArgs, class Type, class RetVal, class... Args> using VirtualHook =
		HookShim<Hooking::GroupVirtualHook<HookFunc, fn, vaArgs, Type, RetVal, Args...>, Args...>;
	template<HookFunc fn, bool vaArgs, class Type, class RetVal, class... Args> using GlobalVirtualHook =
		HookShim<Hooking::GroupGlobalVirtualHook<HookFunc, fn, vaArgs, Type, RetVal, Args...>, Args...>;
	template<HookFunc fn, bool vaArgs, class Type, class RetVal, class... Args> using ClassHook =
		HookShim<Hooking::GroupClassHook<HookFunc, fn, vaArgs, Type, RetVal, Args...>, Args...>;
	template<HookFunc fn, bool vaArgs, class RetVal, class... Args> using GlobalHook =
		HookShim<Hooking::GroupGlobalHook<HookFunc, fn, vaArgs, RetVal, Args...>, Args...>;
	template<HookFunc fn, bool vaArgs, class Type, class RetVal, class... Args> using GlobalClassHook =
		HookShim<Hooking::GroupManualClassHook<HookFunc, fn, vaArgs, Type, RetVal, Args...>, Type*, Args...>;

	typedef bool(__cdecl *Raw_RenderBox)(const Vector& origin, const QAngle& angles, const Vector& mins, const Vector& maxs, Color c, bool zBuffer, bool insideOut);
	typedef bool(__cdecl *Raw_RenderBox_1)(const Vector& origin, const QAngle& angles, const Vector& mins, const Vector& maxs, Color c, IMaterial* material, bool insideOut);
	typedef bool(__cdecl *Raw_RenderWireframeBox)(const Vector& origin, const QAngle& angles, const Vector& mins, const Vector& maxs, Color c, bool zBuffer);
	typedef bool(__cdecl *Raw_RenderLine)(const Vector& p0, const Vector& p1, Color c, bool zBuffer);
	typedef bool(__cdecl *Raw_RenderTriangle)(const Vector& p0, const Vector& p1, const Vector& p2, Color c, bool zBuffer);

#pragma region HookFuncType
	template<HookFunc fn> struct HookFuncType { };

	template<> struct HookFuncType<HookFunc::ICvar_ConsoleColorPrintf>
	{
		typedef VirtualHook<HookFunc::ICvar_ConsoleColorPrintf, true, ICvar, void, const Color&, const char*> Hook;
	};
	template<> struct HookFuncType<HookFunc::ICvar_ConsoleDPrintf>
	{
		typedef VirtualHook<HookFunc::ICvar_ConsoleDPrintf, true, ICvar, void, const char*> Hook;
	};
	template<> struct HookFuncType<HookFunc::ICvar_ConsolePrintf>
	{
		typedef VirtualHook<HookFunc::ICvar_ConsolePrintf, true, ICvar, void, const char*> Hook;
	};
	template<> struct HookFuncType<HookFunc::IClientEngineTools_InToolMode>
	{
		typedef VirtualHook<HookFunc::IClientEngineTools_InToolMode, false, IClientEngineTools, bool> Hook;
	};
	template<> struct HookFuncType<HookFunc::IClientEngineTools_IsThirdPersonCamera>
	{
		typedef VirtualHook<HookFunc::IClientEngineTools_IsThirdPersonCamera, false, IClientEngineTools, bool> Hook;
	};
	template<> struct HookFuncType<HookFunc::IClientEngineTools_SetupEngineView>
	{
		typedef VirtualHook<HookFunc::IClientEngineTools_SetupEngineView, false, IClientEngineTools, bool, Vector&, QAngle&, float&> Hook;
	};
	template<> struct HookFuncType<HookFunc::IVEngineClient_GetPlayerInfo>
	{
		typedef VirtualHook<HookFunc::IVEngineClient_GetPlayerInfo, false, IVEngineClient, bool, int, player_info_t*> Hook;
	};
	template<> struct HookFuncType<HookFunc::IGameEventManager2_FireEventClientSide>
	{
		typedef VirtualHook<HookFunc::IGameEventManager2_FireEventClientSide, false, IGameEventManager2, bool, IGameEvent*> Hook;
	};
	template<> struct HookFuncType<HookFunc::IGameSystem_Add>
	{
		typedef void(__cdecl* Raw)(IGameSystem* system);
	};
	template<> struct HookFuncType<HookFunc::IGameSystem_Remove>
	{
		typedef void(__cdecl* Raw)(IGameSystem* system);
	};
	template<> struct HookFuncType<HookFunc::IPrediction_PostEntityPacketReceived>
	{
		typedef VirtualHook<HookFunc::IPrediction_PostEntityPacketReceived, false, IPrediction, void> Hook;
	};
	template<> struct HookFuncType<HookFunc::IStudioRender_ForcedMaterialOverride>
	{
		typedef VirtualHook<HookFunc::IStudioRender_ForcedMaterialOverride, false, IStudioRender, void, IMaterial*, OverrideType_t> Hook;
	};
	template<> struct HookFuncType<HookFunc::IClientRenderable_DrawModel>
	{
		typedef GlobalVirtualHook<HookFunc::IClientRenderable_DrawModel, false, IClientRenderable, int, int> Hook;
	};
	template<> struct HookFuncType<HookFunc::C_HLTVCamera_SetCameraAngle>
	{
		typedef void(__thiscall *Raw)(C_HLTVCamera* pThis, const QAngle& ang);
		typedef ClassHook<HookFunc::C_HLTVCamera_SetCameraAngle, false, C_HLTVCamera, void, const QAngle&> Hook;
	};
	template<> struct HookFuncType<HookFunc::C_HLTVCamera_SetMode>
	{
		typedef void(__thiscall *Raw)(C_HLTVCamera* pThis, int mode);
		typedef ClassHook<HookFunc::C_HLTVCamera_SetMode, false, C_HLTVCamera, void, int> Hook;
	};
	template<> struct HookFuncType<HookFunc::C_HLTVCamera_SetPrimaryTarget>
	{
		typedef void(__thiscall *Raw)(C_HLTVCamera* pThis, int targetEntindex);
		typedef ClassHook<HookFunc::C_HLTVCamera_SetPrimaryTarget, false, C_HLTVCamera, void, int> Hook;
	};
	template<> struct HookFuncType<HookFunc::CHudBaseDeathNotice_GetIcon>
	{
		typedef CHudTexture*(__stdcall *Raw)(const char* szIcon, int eIconFormat);
		typedef GlobalHook<HookFunc::CHudBaseDeathNotice_GetIcon, false, CHudTexture*, const char*, int> Hook;
	};
	template<> struct HookFuncType<HookFunc::C_BaseAnimating_ComputeHitboxSurroundingBox>
	{
		typedef bool(__thiscall *Raw)(C_BaseAnimating* pThis, Vector* pVecWorldMins, Vector* pVecWorldMaxs);
	};
	template<> struct HookFuncType<HookFunc::C_BaseAnimating_GetBoneCache>
	{
		typedef CBoneCache*(__thiscall *Raw)(C_BaseAnimating*, CStudioHdr*);
		typedef GlobalClassHook<HookFunc::C_BaseAnimating_GetBoneCache, false, C_BaseAnimating, CBoneCache*, CStudioHdr*> Hook;
	};
	template<> struct HookFuncType<HookFunc::C_BaseAnimating_LockStudioHdr>
	{
		typedef void(__thiscall *Raw)(C_BaseAnimating*);
		typedef GlobalClassHook<HookFunc::C_BaseAnimating_LockStudioHdr, false, C_BaseAnimating, void> Hook;
	};
	template<> struct HookFuncType<HookFunc::C_BaseAnimating_LookupBone>
	{
		typedef int(__thiscall *Raw)(C_BaseAnimating*, const char*);
		typedef GlobalClassHook<HookFunc::C_BaseAnimating_LookupBone, false, C_BaseAnimating, int, const char*> Hook;
	};
	template<> struct HookFuncType<HookFunc::C_BaseAnimating_GetBonePosition>
	{
		typedef void(__thiscall *Raw)(C_BaseAnimating*, int, Vector&, QAngle&);
		typedef GlobalClassHook<HookFunc::C_BaseAnimating_GetBonePosition, false, C_BaseAnimating, void, int, Vector&, QAngle&> Hook;
	};
	template<> struct HookFuncType<HookFunc::C_BaseAnimating_DrawModel>
	{
		typedef int(__thiscall *Raw)(C_BaseAnimating*, int);
		typedef GlobalClassHook<HookFunc::C_BaseAnimating_DrawModel, false, C_BaseAnimating, int, int> Hook;
	};
	template<> struct HookFuncType<HookFunc::C_BaseAnimating_InternalDrawModel>
	{
		typedef int(__thiscall *Raw)(C_BaseAnimating* pThis, int flags);
		typedef GlobalClassHook<HookFunc::C_BaseAnimating_InternalDrawModel, false, C_BaseAnimating, int, int> Hook;
	};
	template<> struct HookFuncType<HookFunc::C_BaseEntity_CalcAbsolutePosition>
	{
		typedef void(__thiscall *Raw)(C_BaseEntity* pThis);
		typedef GlobalClassHook<HookFunc::C_BaseEntity_CalcAbsolutePosition, false, C_BaseEntity, void> Hook;
	};
	template<> struct HookFuncType<HookFunc::C_BaseEntity_Init>
	{
		typedef bool(__thiscall *Raw)(C_BaseEntity* pThis, int entnum, int iSerialNum);
		typedef GlobalClassHook<HookFunc::C_BaseEntity_Init, false, C_BaseEntity, bool, int, int> Hook;
	};
	template<> struct HookFuncType<HookFunc::C_BasePlayer_GetDefaultFOV>
	{
		typedef int(__thiscall *Raw)(C_BasePlayer* pThis);
		typedef GlobalClassHook<HookFunc::C_BasePlayer_GetDefaultFOV, false, C_BasePlayer, int> Hook;
	};
	template<> struct HookFuncType<HookFunc::C_BasePlayer_GetFOV>
	{
		typedef float(__thiscall *Raw)(C_BasePlayer* pThis);
	};
	template<> struct HookFuncType<HookFunc::C_BasePlayer_GetLocalPlayer>
	{
		typedef C_BasePlayer*(__cdecl *Raw)();
	};
	template<> struct HookFuncType<HookFunc::C_BasePlayer_ShouldDrawLocalPlayer>
	{
		typedef bool(*Raw)();
	};
	template<> struct HookFuncType<HookFunc::C_TFPlayer_DrawModel>
	{
		typedef int(__thiscall *Raw)(C_TFPlayer*, int);
		typedef GlobalClassHook<HookFunc::C_TFPlayer_DrawModel, false, C_TFPlayer, int, int> Hook;
	};
	template<> struct HookFuncType<HookFunc::vgui_EditablePanel_GetDialogVariables>
	{
		typedef KeyValues*(__thiscall *Raw)(vgui::EditablePanel* pThis);
	};
	template<> struct HookFuncType<HookFunc::vgui_ProgressBar_ApplySettings>
	{
		typedef void(__thiscall *Raw)(vgui::ProgressBar* pThis, KeyValues* pSettings);
		typedef GlobalClassHook<HookFunc::vgui_ProgressBar_ApplySettings, false, vgui::ProgressBar, void, KeyValues*> Hook;
	};
	template<> struct HookFuncType<HookFunc::vgui_ImagePanel_SetImage>
	{
		typedef void(__thiscall *Raw)(vgui::ImagePanel* pThis, const char* imageName);
	};
	template<> struct HookFuncType<HookFunc::CGlowObjectManager_ApplyEntityGlowEffects>
	{
		typedef void(__thiscall *Raw)(CGlowObjectManager*, const CViewSetup*, int, CMatRenderContextPtr&, float, int, int, int, int);
		typedef GlobalClassHook<HookFunc::CGlowObjectManager_ApplyEntityGlowEffects, false, CGlowObjectManager, void, const CViewSetup*, int, CMatRenderContextPtr&, float, int, int, int, int> Hook;
	};
	template<> struct HookFuncType<HookFunc::CAccountPanel_OnAccountValueChanged>
	{
		typedef void*(__thiscall *Raw)(CAccountPanel* pThis, int unknown, int healthDelta, int deltaType);
		typedef GlobalClassHook<HookFunc::CAccountPanel_OnAccountValueChanged, false, CAccountPanel, void*, int, int, int> Hook;
	};
	template<> struct HookFuncType<HookFunc::CAccountPanel_Paint>
	{
		typedef void(__thiscall* Raw)(CAccountPanel* pThis);
		typedef GlobalClassHook<HookFunc::CAccountPanel_Paint, false, CAccountPanel, void> Hook;
	};
	template<> struct HookFuncType<HookFunc::CAutoGameSystemPerFrame_CAutoGameSystemPerFrame>
	{
		typedef void(__thiscall* Raw)(CAutoGameSystemPerFrame* pThis, const char* name);
	};
	template<> struct HookFuncType<HookFunc::CDamageAccountPanel_DisplayDamageFeedback>
	{
		typedef void(__thiscall *Raw)(CDamageAccountPanel* pThis, C_TFPlayer* pAttacker, C_BaseCombatCharacter* pVictim, int iDamageAmount, int iHealth, bool unknown);
		typedef GlobalClassHook<HookFunc::CDamageAccountPanel_DisplayDamageFeedback, false, CDamageAccountPanel, void, C_TFPlayer*, C_BaseCombatCharacter*, int, int, bool> Hook;
	};
	template<> struct HookFuncType<HookFunc::CDamageAccountPanel_ShouldDraw>
	{
		typedef bool(__thiscall *Raw)(CDamageAccountPanel* pThis);
		typedef GlobalClassHook<HookFunc::CDamageAccountPanel_ShouldDraw, false, CDamageAccountPanel, bool> Hook;
	};
	template<> struct HookFuncType<HookFunc::Global_CreateEntityByName>
	{
		typedef C_BaseEntity*(__cdecl *Raw)(const char* entityName);
		typedef GlobalHook<HookFunc::Global_CreateEntityByName, false, C_BaseEntity*, const char*> Hook;
	};
	template<> struct HookFuncType<HookFunc::Global_GetLocalPlayerIndex>
	{
		typedef int(*Raw)();
		typedef GlobalHook<HookFunc::Global_GetLocalPlayerIndex, false, int> Hook;
	};
	template<> struct HookFuncType<HookFunc::Global_GetVectorInScreenSpace>
	{
		typedef bool(__cdecl* Raw)(Vector pos, int& x, int& y, Vector* offset);
		typedef GlobalHook<HookFunc::Global_GetVectorInScreenSpace, false, bool, Vector, int&, int&, Vector*> Hook;
	};
	template<> struct HookFuncType<HookFunc::Global_CreateTFGlowObject>
	{
		typedef IClientNetworkable*(__cdecl *Raw)(int entNum, int serialNum);
		typedef GlobalHook<HookFunc::Global_CreateTFGlowObject, false, IClientNetworkable*, int, int> Hook;
	};
	template<> struct HookFuncType<HookFunc::Global_UserInfoChangedCallback>
	{
		typedef void(__cdecl* Raw)(void*, INetworkStringTable* stringTable, int stringNumber, const char* newString, const void* newData);
		typedef GlobalHook<HookFunc::Global_UserInfoChangedCallback, false, void, void*, INetworkStringTable*, int, const char*, const void*> Hook;
	};
	template<> struct HookFuncType<HookFunc::Global_UTILComputeEntityFade>
	{
		typedef unsigned char(__cdecl *Raw)(C_BaseEntity* pEntity, float flMinDist, float flMaxDist, float flFadeScale);
		typedef GlobalHook<HookFunc::Global_UTILComputeEntityFade, false, unsigned char, C_BaseEntity*, float, float, float> Hook;
	};
	template<> struct HookFuncType<HookFunc::Global_UTIL_TraceLine>
	{
		typedef void(__cdecl* Raw)(const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask, const IHandleEntity* ignore, int collisionGroup, trace_t* ptr);
		typedef GlobalHook<HookFunc::Global_UTIL_TraceLine, false, void, const Vector&, const Vector&, unsigned int, const IHandleEntity*, int, trace_t*> Hook;
	};
	template<> struct HookFuncType<HookFunc::Global_DrawOpaqueRenderable>
	{
		typedef void(__cdecl *Raw)(IClientRenderable* pEnt, bool bTwoPass, ERenderDepthMode DepthMode, int nDefaultFlags);
		typedef GlobalHook<HookFunc::Global_DrawOpaqueRenderable, false, void, IClientRenderable*, bool, ERenderDepthMode, int> Hook;
	};
	template<> struct HookFuncType<HookFunc::Global_DrawTranslucentRenderable>
	{
		typedef void(__cdecl *Raw)(IClientRenderable* pEnt, bool bTwoPass, bool bShadowDepth, bool bIgnoreDepth);
		typedef GlobalHook<HookFunc::Global_DrawTranslucentRenderable, false, void, IClientRenderable*, bool, bool, bool> Hook;
	};
#pragma endregion HookFuncType

public:
	HookManager();

	static bool Load();
	static bool Unload();

	template<HookFunc fn> __forceinline typename HookFuncType<fn>::Hook::OriginalFnType GetFunc() { return GetHook<fn>()->GetOriginal(); }
	template<HookFunc fn> __forceinline typename HookFuncType<fn>::Hook* GetHook() { return static_cast<HookFuncType<fn>::Hook*>(m_Hooks[(int)fn].get()); };
	template<HookFunc fn> static __forceinline typename HookFuncType<fn>::Raw GetRawFunc() { return (HookFuncType<fn>::Raw)s_RawFunctions[(int)fn]; }

	template<HookFunc fn> inline int AddHook(const typename HookFuncType<fn>::Hook::Functional& hook)
	{
		auto hkPtr = GetHook<fn>();
		if (!hkPtr)
			return 0;

		return hkPtr->AddHook(hook);
	}
	template<HookFunc fn> inline bool RemoveHook(int hookID, const char* funcName)
	{
		auto hkPtr = GetHook<fn>();
		if (!hkPtr)
			return false;

		return hkPtr->RemoveHook(hookID, funcName);
	}
	template<HookFunc fn> inline typename HookFuncType<fn>::Hook::Functional GetOriginal()
	{
		auto hkPtr = GetHook<fn>();
		if (!hkPtr)
			return nullptr;

		return hkPtr->GetOriginal();
	}
	template<HookFunc fn> inline void SetState(Hooking::HookAction state)
	{
		auto hkPtr = GetHook<fn>();
		if (!hkPtr)
			return;

		hkPtr->SetState(state);
	}
	template<HookFunc fn> inline bool IsInHook()
	{
		auto hkPtr = GetHook<fn>();
		if (!hkPtr)
			return false;

		return hkPtr->IsInHook();
	}

private:
	std::unique_ptr<Hooking::IGroupHook> m_Hooks[(int)HookFunc::Count];

	template<HookFunc fn, class... Args> void InitHook(Args&&... args);
	template<HookFunc fn> void InitGlobalHook();

	static void* s_RawFunctions[(int)HookFunc::Count];
	static void InitRawFunctionsList();
	template<HookFunc fn> static void FindFunc(const char* signature, const char* mask, int offset = 0, const char* module = "client");
	static void FindFunc_C_BasePlayer_GetLocalPlayer();

	void IngameStateChanged(bool inGame);
	class Panel;
	std::unique_ptr<Panel> m_Panel;
};

extern void* SignatureScan(const char* moduleName, const char* signature, const char* mask);
extern HookManager* GetHooks();