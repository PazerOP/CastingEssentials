#pragma once
#include "Hooking/HookShim.h"

#include "Hooking/GroupGlobalHook.h"
#include "Hooking/GroupClassHook.h"
#include "Hooking/GroupManualClassHook.h"
#include "Hooking/GroupVirtualHook.h"
#include "Hooking/GroupGlobalVirtualHook.h"

class C_BaseCombatCharacter;
class C_BasePlayer;
class C_HLTVCamera;
class C_TFViewModel;
class C_TFWeaponBase;
class C_BaseCombatWeapon;
class CAccountPanel;
class CAutoGameSystemPerFrame;
class CBaseClientRenderTargets;
class CDamageAccountPanel;
class CInput;
struct ClientModelRenderInfo_t;
class CNewParticleEffect;
class CParticleProperty;
class CUtlBuffer;
class CViewRender;
class CVTFTexture;
struct DrawModelState_t;
class IClientRenderTargets;
class IGameSystem;
class IMaterialSystem;
class IMaterialSystemHardwareConfig;
class INetworkStringTable;
class IStaticPropMgrClient;
struct mstudioseqdesc_t;
using trace_t = class CGameTrace;
struct VTFFileHeader_t;
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
	class AnimationController;
	class EditablePanel;
	class ImagePanel;
	class Panel;
	class ProgressBar;
}

enum class HookFunc
{
	C_BaseAnimating_ComputeHitboxSurroundingBox,
	C_BaseAnimating_DoInternalDrawModel,
	C_BaseAnimating_DrawModel,
	C_BaseAnimating_GetBoneCache,
	C_BaseAnimating_GetBonePosition,
	C_BaseAnimating_GetSequenceActivityName,
	C_BaseAnimating_InternalDrawModel,
	C_BaseAnimating_LockStudioHdr,

	C_BaseEntity_Init,
	C_BaseEntity_CalcAbsolutePosition,

	C_BasePlayer_GetDefaultFOV,
	C_BasePlayer_GetFOV,
	C_BasePlayer_GetLocalPlayer,
	C_BasePlayer_ShouldDrawLocalPlayer,

	C_HLTVCamera_CalcView,
	C_HLTVCamera_SetCameraAngle,
	C_HLTVCamera_SetMode,
	C_HLTVCamera_SetPrimaryTarget,

	C_TFPlayer_CalcView,
	C_TFPlayer_DrawModel,
	C_TFPlayer_GetEntityForLoadoutSlot,

	C_TFViewModel_CalcViewModelView,

	C_TFWeaponBase_PostDataUpdate,

	CAccountPanel_OnAccountValueChanged,
	CAccountPanel_Paint,
	CAutoGameSystemPerFrame_CAutoGameSystemPerFrame,
	CBaseClientRenderTargets_InitClientRenderTargets,
	CDamageAccountPanel_FireGameEvent,
	CDamageAccountPanel_ShouldDraw,

	CInput_CreateMove,

	CParticleProperty_DebugPrintEffects,
	CNewParticleEffect_StopEmission,
	CNewParticleEffect_SetDormant,

	CGlowObjectManager_ApplyEntityGlowEffects,

	CHudBaseDeathNotice_GetIcon,

	CStudioHdr_GetNumSeq,
	CStudioHdr_pSeqdesc,

	CViewRender_PerformScreenSpaceEffects,

	CVTFTexture_GetResourceData,
	CVTFTexture_ReadHeader,

	Global_Cmd_Shutdown,
	Global_CreateEntityByName,
	Global_CreateTFGlowObject,
	Global_DrawOpaqueRenderable,
	Global_DrawTranslucentRenderable,
	Global_GetLocalPlayerIndex,
	Global_GetSequenceName,
	Global_GetVectorInScreenSpace,
	Global_Studio_BoneIndexByName,
	Global_UserInfoChangedCallback,
	Global_UTILComputeEntityFade,
	Global_UTIL_TraceLine,

	IClientEngineTools_InToolMode,
	IClientEngineTools_IsThirdPersonCamera,
	IClientEngineTools_SetupEngineView,

	IClientRenderable_DrawModel,

	ICvar_ConsoleColorPrintf,
	ICvar_ConsoleDPrintf,
	ICvar_ConsolePrintf,

	IGameEventManager2_FireEventClientSide,
	IGameSystem_Add,

	IPrediction_PostEntityPacketReceived,

	IStaticPropMgrClient_ComputePropOpacity,

	IStudioRender_ForcedMaterialOverride,

	IVEngineClient_GetPlayerInfo,

	vgui_AnimationController_StartAnimationSequence,
	vgui_Panel_FindChildByName,
	vgui_ProgressBar_ApplySettings,

	Count,
};

class HookDefinitions
{
	template<HookFunc fn, bool vaArgs, class Type, class RetVal, class... Args> using VirtualHook =
		Hooking::HookShim<Hooking::GroupVirtualHook<HookFunc, fn, vaArgs, Type, RetVal, Args...>, Args...>;
	template<HookFunc fn, bool vaArgs, class Type, class RetVal, class... Args> using GlobalVirtualHook =
		Hooking::HookShim<Hooking::GroupGlobalVirtualHook<HookFunc, fn, vaArgs, Type, RetVal, Args...>, Args...>;
	template<HookFunc fn, bool vaArgs, class Type, class RetVal, class... Args> using ClassHook =
		Hooking::HookShim<Hooking::GroupClassHook<HookFunc, fn, vaArgs, Type, RetVal, Args...>, Args...>;
	template<HookFunc fn, bool vaArgs, class RetVal, class... Args> using GlobalHook =
		Hooking::HookShim<Hooking::GroupGlobalHook<HookFunc, fn, vaArgs, RetVal, Args...>, Args...>;
	template<HookFunc fn, bool vaArgs, class Type, class RetVal, class... Args> using GlobalClassHook =
		Hooking::HookShim<Hooking::GroupManualClassHook<HookFunc, fn, vaArgs, Type, RetVal, Args...>, Type*, Args...>;

protected:
	template<HookFunc fn> struct HookFuncType {};

	template<> struct HookFuncType<HookFunc::Global_Cmd_Shutdown>
	{
		typedef void(__cdecl* Raw)();
	};
	template<> struct HookFuncType<HookFunc::Global_GetSequenceName>
	{
		typedef const char*(__cdecl* Raw)(CStudioHdr* studiohdr, int sequence);
	};
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
	template<> struct HookFuncType<HookFunc::IPrediction_PostEntityPacketReceived>
	{
		typedef VirtualHook<HookFunc::IPrediction_PostEntityPacketReceived, false, IPrediction, void> Hook;
	};
	template<> struct HookFuncType<HookFunc::IStaticPropMgrClient_ComputePropOpacity>
	{
		typedef void(__thiscall* Raw)(const Vector& viewOrigin, float factor);
		typedef VirtualHook<HookFunc::IStaticPropMgrClient_ComputePropOpacity, false, IStaticPropMgrClient, void, const Vector&, float> Hook;
	};
	template<> struct HookFuncType<HookFunc::IStudioRender_ForcedMaterialOverride>
	{
		typedef VirtualHook<HookFunc::IStudioRender_ForcedMaterialOverride, false, IStudioRender, void, IMaterial*, OverrideType_t> Hook;
	};
	template<> struct HookFuncType<HookFunc::IClientRenderable_DrawModel>
	{
		typedef GlobalVirtualHook<HookFunc::IClientRenderable_DrawModel, false, IClientRenderable, int, int> Hook;
	};
	template<> struct HookFuncType<HookFunc::C_HLTVCamera_CalcView>
	{
		typedef void(__thiscall* Raw)(C_HLTVCamera* pThis, Vector& origin, QAngle& angles, float& fov);
		typedef GlobalClassHook<HookFunc::C_HLTVCamera_CalcView, false, C_HLTVCamera, void, Vector&, QAngle&, float&> Hook;
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
	template<> struct HookFuncType<HookFunc::CStudioHdr_GetNumSeq>
	{
		typedef int(__thiscall *Raw)(const CStudioHdr* pThis);
	};
	template<> struct HookFuncType<HookFunc::CStudioHdr_pSeqdesc>
	{
		typedef mstudioseqdesc_t&(__thiscall *Raw)(CStudioHdr* pThis, int iSequence);
	};
	template<> struct HookFuncType<HookFunc::CViewRender_PerformScreenSpaceEffects>
	{
		typedef void(__thiscall* Raw)(CViewRender* pThis, int x, int y, int w, int h);
		typedef GlobalClassHook<HookFunc::CViewRender_PerformScreenSpaceEffects, false, CViewRender, void, int, int, int, int> Hook;
	};
	template<> struct HookFuncType<HookFunc::CVTFTexture_GetResourceData>
	{
		typedef void*(__thiscall* Raw)(CVTFTexture* pThis, uint32 type, size_t *size);
		typedef GlobalClassHook<HookFunc::CVTFTexture_GetResourceData, false, CVTFTexture, void*, uint32, size_t*> Hook;
	};
	template<> struct HookFuncType<HookFunc::CVTFTexture_ReadHeader>
	{
		typedef bool(__thiscall* Raw)(CVTFTexture* pThis, CUtlBuffer& buf, VTFFileHeader_t& header);
		typedef GlobalClassHook<HookFunc::CVTFTexture_ReadHeader, false, CVTFTexture, bool, CUtlBuffer&, VTFFileHeader_t&> Hook;
	};
	template<> struct HookFuncType<HookFunc::C_BaseAnimating_ComputeHitboxSurroundingBox>
	{
		typedef bool(__thiscall *Raw)(C_BaseAnimating* pThis, Vector* pVecWorldMins, Vector* pVecWorldMaxs);
	};
	template<> struct HookFuncType<HookFunc::C_BaseAnimating_DoInternalDrawModel>
	{
		typedef void(__thiscall* Raw)(C_BaseAnimating* pThis, ClientModelRenderInfo_t* pInfo, DrawModelState_t* pState, matrix3x4_t* pBoneToWorldArray);
		typedef GlobalClassHook<HookFunc::C_BaseAnimating_DoInternalDrawModel, false, C_BaseAnimating, void, ClientModelRenderInfo_t*, DrawModelState_t*, matrix3x4_t*> Hook;
	};
	template<> struct HookFuncType<HookFunc::C_BaseAnimating_GetBoneCache>
	{
		typedef CBoneCache*(__thiscall *Raw)(C_BaseAnimating*, CStudioHdr*);
	};
	template<> struct HookFuncType<HookFunc::C_BaseAnimating_LockStudioHdr>
	{
		typedef void(__thiscall *Raw)(C_BaseAnimating*);
		typedef GlobalClassHook<HookFunc::C_BaseAnimating_LockStudioHdr, false, C_BaseAnimating, void> Hook;
	};
	template<> struct HookFuncType<HookFunc::C_BaseAnimating_GetBonePosition>
	{
		typedef void(__thiscall *Raw)(C_BaseAnimating*, int, Vector&, QAngle&);
		typedef GlobalClassHook<HookFunc::C_BaseAnimating_GetBonePosition, false, C_BaseAnimating, void, int, Vector&, QAngle&> Hook;
	};
	template<> struct HookFuncType<HookFunc::C_BaseAnimating_GetSequenceActivityName>
	{
		typedef const char*(__thiscall* Raw)(C_BaseAnimating* pThis, int nSequence);
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
		typedef bool(__thiscall *Raw)(C_BasePlayer* pThis);
		typedef GlobalClassHook<HookFunc::C_BasePlayer_ShouldDrawLocalPlayer, false, C_BasePlayer, bool> Hook;
	};
	template<> struct HookFuncType<HookFunc::C_TFPlayer_CalcView>
	{
		typedef void(__thiscall* Raw)(C_TFPlayer* pThis, Vector& eyeOrigin, QAngle& eyeAngles, float& zNear, float& zFar, float& fov);
		typedef GlobalClassHook<HookFunc::C_TFPlayer_CalcView, false, C_TFPlayer, void, Vector&, QAngle&, float&, float&, float&> Hook;
	};
	template<> struct HookFuncType<HookFunc::C_TFPlayer_DrawModel>
	{
		typedef int(__thiscall *Raw)(C_TFPlayer*, int);
		typedef GlobalClassHook<HookFunc::C_TFPlayer_DrawModel, false, C_TFPlayer, int, int> Hook;
	};
	template<> struct HookFuncType<HookFunc::C_TFPlayer_GetEntityForLoadoutSlot>
	{
		typedef C_BaseEntity*(__thiscall *Raw)(C_TFPlayer* pThis, int slot, bool includeWearables);
	};
	template<> struct HookFuncType<HookFunc::C_TFViewModel_CalcViewModelView>
	{
		typedef void(__thiscall *Raw)(C_TFViewModel* pThis, C_BasePlayer* player, const Vector& eyePos, const QAngle& eyeAng);
		typedef GlobalClassHook<HookFunc::C_TFViewModel_CalcViewModelView, false, C_TFViewModel, void, C_BasePlayer*, const Vector&, const QAngle&> Hook;
	};
	template<> struct HookFuncType<HookFunc::C_TFWeaponBase_PostDataUpdate>
	{
		typedef void(__thiscall *Raw)(IClientNetworkable* pThis, int updateType);
		typedef GlobalClassHook<HookFunc::C_TFWeaponBase_PostDataUpdate, false, IClientNetworkable, void, int> Hook;
	};
	template<> struct HookFuncType<HookFunc::vgui_AnimationController_StartAnimationSequence>
	{
		typedef bool(__thiscall* Raw)(vgui::AnimationController* pThis, vgui::Panel* pWithinParent, const char* sequenceName, bool unknown);
	};
	template<> struct HookFuncType<HookFunc::vgui_Panel_FindChildByName>
	{
		typedef vgui::Panel*(__thiscall* Raw)(vgui::Panel* pThis, const char* childName, bool recurseDown);
		typedef GlobalClassHook<HookFunc::vgui_Panel_FindChildByName, false, vgui::Panel, vgui::Panel*, const char*, bool> Hook;
	};
	template<> struct HookFuncType<HookFunc::vgui_ProgressBar_ApplySettings>
	{
		typedef void(__thiscall *Raw)(vgui::ProgressBar* pThis, KeyValues* pSettings);
		typedef GlobalClassHook<HookFunc::vgui_ProgressBar_ApplySettings, false, vgui::ProgressBar, void, KeyValues*> Hook;
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
	template<> struct HookFuncType<HookFunc::CBaseClientRenderTargets_InitClientRenderTargets>
	{
		typedef void(__thiscall* Raw)(CBaseClientRenderTargets* pThis, IMaterialSystem* pMaterialSystem, IMaterialSystemHardwareConfig* config, int iWaterTextureSize, int iCameraTextureSize);
		typedef GlobalClassHook<HookFunc::CBaseClientRenderTargets_InitClientRenderTargets, false, CBaseClientRenderTargets, void, IMaterialSystem*, IMaterialSystemHardwareConfig*, int, int> Hook;
	};
	template<> struct HookFuncType<HookFunc::CDamageAccountPanel_FireGameEvent>
	{
		typedef void(__thiscall *Raw)(CDamageAccountPanel* pThis, IGameEvent*);
		typedef GlobalClassHook<HookFunc::CDamageAccountPanel_FireGameEvent, false, CDamageAccountPanel, void, IGameEvent*> Hook;
	};
	template<> struct HookFuncType<HookFunc::CDamageAccountPanel_ShouldDraw>
	{
		typedef bool(__thiscall *Raw)(CDamageAccountPanel* pThis);
		typedef GlobalClassHook<HookFunc::CDamageAccountPanel_ShouldDraw, false, CDamageAccountPanel, bool> Hook;
	};
	template<> struct HookFuncType<HookFunc::CInput_CreateMove>
	{
		typedef void(__thiscall* Raw)(CInput* pThis, int sequenceNumber, float inputSampleFrametime, bool active);
		typedef GlobalClassHook<HookFunc::CInput_CreateMove, false, CInput, void, int, float, bool> Hook;
	};
	template<> struct HookFuncType<HookFunc::CParticleProperty_DebugPrintEffects>
	{
		typedef void(__thiscall *Raw)(CParticleProperty* pThis);
	};
	template<> struct HookFuncType<HookFunc::CNewParticleEffect_StopEmission>
	{
		typedef void(__thiscall *Raw)(CNewParticleEffect* pThis, bool bInfiniteOnly, bool bRemoveAllParticles, bool bWakeOnStop);
	};
	template<> struct HookFuncType<HookFunc::CNewParticleEffect_SetDormant>
	{
		typedef void(__thiscall *Raw)(CNewParticleEffect* pThis, bool bDormant);
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
	template<> struct HookFuncType<HookFunc::Global_Studio_BoneIndexByName>
	{
		typedef int(__cdecl* Raw)(const CStudioHdr* pStudioHdr, const char* pName);
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
};
