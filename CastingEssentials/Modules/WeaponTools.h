#pragma once

#include "PluginBase/Modules.h"

#include <convar.h>
#include <dt_recv.h>
#include <shared/ai_activity.h>
#include <shared/ehandle.h>

#include <optional>
#include <unordered_map>

class C_BaseViewModel;

namespace std
{
	template<> struct hash<CBaseHandle>
	{
		size_t operator()(const CBaseHandle& handle) const
		{
			return handle.GetEntryIndex() & (handle.GetSerialNumber() << NUM_ENT_ENTRY_BITS);
		}
	};
}

class WeaponTools final : public Module<WeaponTools>
{
public:
	WeaponTools();

	static bool CheckDependencies();
	static constexpr __forceinline const char* GetModuleName() { return "Weapon Tools"; }

protected:
	void OnTick(bool inGame) override;
	void LevelShutdown() override;

private:
	static float& GetInspectInterp();

	enum class InspectStage
	{
		None = -1,
		PullUp = 0,
		Hold = 1,
		PutDown = 2,
	};

	ConVar ce_weapon_inspect_debug;
	ConVar ce_weapon_inspect_block;
	ConVar ce_weapon_skin_downsample;
	//ConCommand ce_weapon_inspect_force;

	std::optional<VariablePusher<RecvVarProxyFn>> m_InspectStageProxyFnOverride;
	std::optional<VariablePusher<RecvVarProxyFn>> m_ViewModelProxyFnOverride;
	void RecvProxy_SequenceNum_Override(const CRecvProxyData* data, void* pStruct, void* out);

	void SkinDownsampleChanged(const ConVar* var);

	void InspectBlockToggled(const ConVar* cv);

	struct ViewModel
	{
		CHandle<C_BaseViewModel> m_VM;
		Activity m_LastVMIdleActivity = ACT_INVALID;
	};

	void UpdateVMIdleActivity(const CHandle<C_BaseViewModel>& vm, Activity newAct);
	std::unordered_map<CHandle<C_BaseViewModel>, Activity, std::hash<CBaseHandle>> m_ViewModelCache;

	static std::pair<std::byte*, std::byte*> GetSkinDownsampleVars();
};