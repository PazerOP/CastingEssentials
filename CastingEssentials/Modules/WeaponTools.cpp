#include "WeaponTools.h"

#include "Modules/CameraState.h"
#include "PluginBase/Entities.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"

#include <client/c_basecombatweapon.h>
#include <client/c_baseentity.h>
#include <studio.h>
#include <shared/animation.h>

#include <Windows.h>

MODULE_REGISTER(WeaponTools);

WeaponTools::WeaponTools() :
	ce_weapon_inspect_debug("ce_weapon_inspect_debug", "0", FCVAR_NONE),
	ce_weapon_inspect_block("ce_weapon_inspect_block", "0", FCVAR_NONE, "Don't play weapon inspect animations when triggered by players.",
		[](IConVar* var, const char*, float) { GetModule()->InspectBlockToggled(static_cast<ConVar*>(var)); }),
	ce_weapon_skin_downsample("ce_weapon_skin_downsample", "2", FCVAR_NONE, "Downsample the skins for other players by this many mips.", true, 0, true, 10,
		[](IConVar* var, const char*, float) { GetModule()->SkinDownsampleChanged(static_cast<ConVar*>(var)); })

	//ce_weapon_inspect_force("ce_weapon_inspect_force", []() { GetModule()->ForceInspectWeapon(); }, "Start inspecting the player's weapon.")
{
}

static int ActivityToSequence(CStudioHdr* pStudioHdr, Activity activity)
{
	if (!pStudioHdr)
		return -1;

	const auto seqCount = pStudioHdr->GetNumSeq();
	for (int i = 0; i < seqCount; i++)
	{
		const auto& seqDesc = pStudioHdr->pSeqdesc(i);
		if (seqDesc.activity == activity)
			return i;
	}

	PluginWarning("Unable to find a sequence for activity %i\n", activity);
	return -1;
}

static Activity SequenceToActivity(CStudioHdr* pStudioHdr, int nSequence)
{
	if (!pStudioHdr)
		return ACT_INVALID;

	return (Activity)pStudioHdr->pSeqdesc(nSequence).activity;
}

static bool ShouldDisableActivity(Activity inputAct)
{
	static_assert(ACT_ITEM1_VM_INSPECT_START == 1908);
	static_assert(ACT_ITEM2_VM_INSPECT_START == 1911);
	static_assert(ACT_ITEM3_VM_INSPECT_START == 1914);
	static_assert(ACT_ITEM4_VM_INSPECT_START == 1917);
	static_assert(ACT_PRIMARY_VM_INSPECT_START == 1875);
	static_assert(ACT_PRIMARY_ALT1_VM_INSPECT_START == 1920);
	static_assert(ACT_SECONDARY_VM_INSPECT_START == 1878);
	static_assert(ACT_SECONDARY_ALT1_VM_INSPECT_START == 1923);
	static_assert(ACT_MELEE_VM_INSPECT_START == 1881);
	static_assert(ACT_MELEE_ALT1_VM_INSPECT_START == 1926);
	static_assert(ACT_MELEE_ALT2_VM_INSPECT_START == 1929);
	static_assert(ACT_MELEE_ALT3_VM_INSPECT_START == 1932);
	static_assert(ACT_BUILDING_VM_INSPECT_START == 1935);
	static_assert(ACT_BREADMONSTER_VM_INSPECT_START == 1938);

	switch (inputAct)
	{
		case ACT_ITEM1_VM_INSPECT_START:
		case ACT_ITEM1_VM_INSPECT_IDLE:
		case ACT_ITEM1_VM_INSPECT_END:

		case ACT_ITEM2_VM_INSPECT_START:
		case ACT_ITEM2_VM_INSPECT_IDLE:
		case ACT_ITEM2_VM_INSPECT_END:

		case ACT_ITEM3_VM_INSPECT_START:
		case ACT_ITEM3_VM_INSPECT_IDLE:
		case ACT_ITEM3_VM_INSPECT_END:

		case ACT_ITEM4_VM_INSPECT_START:
		case ACT_ITEM4_VM_INSPECT_IDLE:
		case ACT_ITEM4_VM_INSPECT_END:

		case ACT_PRIMARY_VM_INSPECT_START:
		case ACT_PRIMARY_VM_INSPECT_IDLE:
		case ACT_PRIMARY_VM_INSPECT_END:
		case ACT_PRIMARY_ALT1_VM_INSPECT_START:
		case ACT_PRIMARY_ALT1_VM_INSPECT_IDLE:
		case ACT_PRIMARY_ALT1_VM_INSPECT_END:

		case ACT_SECONDARY_VM_INSPECT_START:
		case ACT_SECONDARY_VM_INSPECT_IDLE:
		case ACT_SECONDARY_VM_INSPECT_END:
		case ACT_SECONDARY_ALT1_VM_INSPECT_START:
		case ACT_SECONDARY_ALT1_VM_INSPECT_IDLE:
		case ACT_SECONDARY_ALT1_VM_INSPECT_END:

		case ACT_MELEE_VM_INSPECT_START:
		case ACT_MELEE_VM_INSPECT_IDLE:
		case ACT_MELEE_VM_INSPECT_END:
		case ACT_MELEE_ALT1_VM_INSPECT_START:
		case ACT_MELEE_ALT1_VM_INSPECT_IDLE:
		case ACT_MELEE_ALT1_VM_INSPECT_END:
		case ACT_MELEE_ALT2_VM_INSPECT_START:
		case ACT_MELEE_ALT2_VM_INSPECT_IDLE:
		case ACT_MELEE_ALT2_VM_INSPECT_END:
		case ACT_MELEE_ALT3_VM_INSPECT_START:
		case ACT_MELEE_ALT3_VM_INSPECT_IDLE:
		case ACT_MELEE_ALT3_VM_INSPECT_END:

		case ACT_BUILDING_VM_INSPECT_START:
		case ACT_BUILDING_VM_INSPECT_IDLE:
		case ACT_BUILDING_VM_INSPECT_END:

		case ACT_BREADMONSTER_VM_INSPECT_START:
		case ACT_BREADMONSTER_VM_INSPECT_IDLE:
		case ACT_BREADMONSTER_VM_INSPECT_END:
			return true;
	}

	return false;
}

void WeaponTools::RecvProxy_SequenceNum_Override(const CRecvProxyData *pData, void *pStruct, void *pOut)
{
	CBaseViewModel *model = (CBaseViewModel *)pStruct;
	if (pData->m_Value.m_Int != model->GetSequence())
	{
		MDLCACHE_CRITICAL_SECTION();

		auto const pStudioHdr = model->GetModelPtr();

		CRecvProxyData newData = *pData;

		const auto inputAct = SequenceToActivity(pStudioHdr, pData->m_Value.m_Int);
		UpdateVMIdleActivity(model, inputAct);

		if (ShouldDisableActivity(inputAct))
		{
			if (auto& found = m_ViewModelCache.find(model); found != m_ViewModelCache.end())
			{
				if (auto newSequence = ActivityToSequence(pStudioHdr, found->second); newSequence >= 0)
					newData.m_Value.m_Int = newSequence;
			}
		}

		m_ViewModelProxyFnOverride->GetOldValue()(&newData, pStruct, pOut);
	}
}

void WeaponTools::SkinDownsampleChanged(const ConVar* var)
{
	auto downsample = var->GetInt();

	auto vars = GetSkinDownsampleVars();
	if (!vars.first || !vars.second)
	{
		PluginWarning("Unable to change skin downsample amounts: Couldn't find addresses\n");
		return;
	}

	DWORD oldProtect1, oldProtect2;

	if (!VirtualProtect(vars.first, 1, PAGE_EXECUTE_READWRITE, &oldProtect1) ||
		!VirtualProtect(vars.second, 1, PAGE_EXECUTE_READWRITE, &oldProtect2))
	{
		PluginWarning("Unable to change skin downsample amounts: VirtualProtect failed\n");
		return;
	}

	*vars.first = (std::byte)downsample;
	*vars.second = (std::byte)downsample;

	DWORD dummy;
	if (!VirtualProtect(vars.first, 1, oldProtect1, &dummy) || !VirtualProtect(vars.second, 1, oldProtect2, &dummy))
		PluginWarning("Warning: Skin downsample amounts were changed, but VirtualProtect failed to restore original protections\n");
}

void WeaponTools::InspectBlockToggled(const ConVar* cv)
{
	if (cv->GetBool())
	{
		// C_BaseViewModel::m_nSequence
		{
			auto prop = Entities::FindRecvProp("CBaseViewModel", "m_nSequence", false);
			RecvVarProxyFn* existingFnVar = &prop->m_ProxyFn;
			Assert(existingFnVar);

			static auto patchFn = [](const CRecvProxyData* data, void* pStruct, void* out)
			{
				GetModule()->RecvProxy_SequenceNum_Override(data, pStruct, out);
			};

			m_ViewModelProxyFnOverride.emplace(*existingFnVar, patchFn);
		}

		// C_TFWeapon::m_nInspectStage
		{
			auto prop = Entities::FindRecvProp("CTFWeaponBase", "m_nInspectStage", false);

			static auto patchFn = [](const CRecvProxyData* data, void* pStruct, void* out)
			{
				*(int*)out = (int)InspectStage::None;
			};

			m_InspectStageProxyFnOverride.emplace(prop->m_ProxyFn, patchFn);
		}
	}
	else
	{
		m_ViewModelProxyFnOverride.reset();
		m_InspectStageProxyFnOverride.reset();
	}
}

void WeaponTools::UpdateVMIdleActivity(const CHandle<C_BaseViewModel>& vm, Activity newAct)
{
	static_assert(ACT_PRIMARY_VM_IDLE == 1561);
	static_assert(ACT_SECONDARY_VM_IDLE == 1581);
	static_assert(ACT_MELEE_VM_IDLE == 1599);
	static_assert(ACT_ENGINEER_BLD_VM_IDLE == 1627);
	static_assert(ACT_ITEM1_VM_IDLE == 1630);
	static_assert(ACT_ITEM2_VM_IDLE == 1650);
	static_assert(ACT_ITEM3_VM_IDLE == 1671);
	static_assert(ACT_ITEM4_VM_IDLE == 1688);
	static_assert(ACT_BREADMONSTER_VM_IDLE == 1842);

	switch (newAct)
	{
		case ACT_VM_IDLE_SPECIAL:
		case ACT_PRIMARY_VM_IDLE:
		case ACT_SECONDARY_VM_IDLE:
		case ACT_MELEE_VM_IDLE:
		case ACT_ENGINEER_BLD_VM_IDLE:
		case ACT_ITEM1_VM_IDLE:
		case ACT_ITEM2_VM_IDLE:
		case ACT_ITEM3_VM_IDLE:
		case ACT_ITEM4_VM_IDLE:
		case ACT_BREADMONSTER_VM_IDLE:
			break;

		default:
			return;
	}

	m_ViewModelCache[vm] = newAct;
}

std::pair<std::byte*, std::byte*> WeaponTools::GetSkinDownsampleVars()
{
	static const auto var1 = SignatureScan("client", "\xC1\xF8\x02\x3B\xC2\x0F\x4C\xC2\x89\x45\xF0", "xxxxxxxxxxx", 2);
	static const auto var2 = SignatureScan("client", "\xC1\xF8\x02\x3B\xC2\x0F\x4C\xC2\x89\x45\xF4", "xxxxxxxxxxx", 2);
	return std::pair<std::byte*, std::byte*>(var1, var2);
}

bool WeaponTools::CheckDependencies()
{
	bool success = true;

	try
	{
		GetInspectInterp();
	}
	catch (const bad_pointer&)
	{
		PluginWarning("Failed to find C_TFViewModel::CalcViewModelView()::s_inspectInterp for module %s\n", GetModuleName());
		success = false;
	}

	return success;
}

void WeaponTools::OnTick(bool inGame)
{
	if (!inGame)
		return;

	if (ce_weapon_inspect_debug.GetBool())
	{
		auto target = Player::AsPlayer(CameraState::GetLocalObserverTarget());
		if (!target)
			target = Player::GetLocalPlayer();

		if (target)
		{
			if (auto weapon = target->GetActiveWeapon())
			{
				auto basePlayer = target->GetBasePlayer();
				C_BaseViewModel* vms[] =
				{
					basePlayer->m_hViewModel[0].Get(),
					basePlayer->m_hViewModel[1].Get()
				};

				static const auto inspectAnimTimeOffset = Entities::GetEntityProp<float>(weapon, "m_flInspectAnimEndTime");
				static const auto inspectStageOffset = Entities::GetEntityProp<InspectStage>(weapon, "m_nInspectStage");

				const auto inspectAnimTime = inspectAnimTimeOffset.GetValue(weapon);
				const auto inspectStage = inspectStageOffset.GetValue(weapon);
				auto& inspectInterp = GetInspectInterp();

				int i = 0;

				engine->Con_NPrintf(i++, "m_flInspectAnimEndTime: %1.2f", inspectAnimTime);

				engine->Con_NPrintf(i++, "m_nInspectStage: %i", inspectStage);

				engine->Con_NPrintf(i++, "s_inspectInterp: %1.2f", inspectInterp);

				engine->Con_NPrintf(i++, "vm[0]: %s, vm[1]: %s",
					vms[0] ? "valid" : "NULL",
					vms[1] ? "valid" : "NULL");

				i++;

				for (size_t vmIndex = 0; vmIndex < std::size(vms); vmIndex++)
				{
					auto vm = vms[vmIndex];
					if (!vm)
						continue;

					static const auto sequenceOffset = Entities::GetEntityProp<int>(vm, "m_nSequence");
					auto sequence = sequenceOffset.GetValue(vm);
					engine->Con_NPrintf(i++, "vm[%i]: m_Sequence: %i", vmIndex, sequence);

					engine->Con_NPrintf(i++, "vm[%i]: sequence name: %s",
						vmIndex, GetSequenceName(vm->GetModelPtr(), sequence));

					engine->Con_NPrintf(i++, "vm[%i]: sequence activity name: %s",
						vmIndex, vm->GetSequenceActivityName(sequence)/*, vm->GetSequenceActivity(sequence)*/);

					continue;
				}
			}
		}
	}
}

void WeaponTools::LevelShutdown()
{
	m_ViewModelCache.clear();
}

float& WeaponTools::GetInspectInterp()
{
	static float* s_inspectInterp = nullptr;
	if (!s_inspectInterp)
	{
		constexpr auto SIG = "\xF3\x0F\x58\x05????\x0F\x2F\xD0";
		constexpr auto MASK = "xxxx????xxx";
		constexpr auto OFFSET = 4;

		if (auto target = (std::byte*)SignatureScan("client", SIG, MASK))
		{
			target += OFFSET;
			s_inspectInterp = *(float**)target;
		}

		if (!s_inspectInterp)
			throw bad_pointer("C_TFViewModel::CalcViewModelView()::s_inspectInterp");
	}

	return *s_inspectInterp;
}
