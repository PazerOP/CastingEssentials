#include <Modules/SrcTVPlus.h>
#include <PluginBase/VariablePusher.h>
#include <PluginBase/Interfaces.h>
#include <client/c_baseentity.h>
#include <client/c_baseplayer.h>
#include <toolframework/ienginetool.h>
#include <set>

MODULE_REGISTER(SrcTVPlus);

bool SrcTVPlus::s_Detected = false;
std::map<int, std::function<SrcTVPlus::Callback>> SrcTVPlus::s_Listeners;
int SrcTVPlus::s_MaxListener = 0;

void SrcTVPlus::NotifyListeners() {
	for (auto& listener : s_Listeners)
		listener.second(s_Detected);
}
void SrcTVPlus::SetDetected(bool value, bool force_broadcast) {
	if (!force_broadcast && value == s_Detected) return; // In general, don't re-send but we need it for initial load
	s_Detected = value;
	NotifyListeners();
}

static RecvProp* g_Prop = nullptr;
static VariablePusher<RecvVarProxyFn> g_ProxyPusher;
static std::map<int, int> g_SeenEntities; // Entity ID + serial -> number of times seen

void SrcTVPlus::DetectorProxy(const CRecvProxyData *pData, void *pStruct, void *pOut) {
	auto ent = static_cast<C_BasePlayer*>(pStruct);
	if(ent != C_BasePlayer::GetLocalPlayer()) {
		auto id = ent->GetRefEHandle().ToInt();
		if (++g_SeenEntities[id] >= 10) {
			SrcTVPlus::DisableDetector();
			PluginMsg("SrcTV+ detected\n");
			SrcTVPlus::SetDetected(true);
		}
	}
	g_ProxyPusher.GetOldValue()(pData, pStruct, pOut);
}

bool SrcTVPlus::CheckDependencies()
{
	auto tbl = const_cast<RecvTable*>(Entities::FindRecvTable("DT_LocalPlayerExclusive"));
	if (!tbl) {
		PluginWarning("Required data table DT_LocalPlayerExclusive for module %s not found!\n", GetModuleName());
		return false;
	}
	g_Prop = Entities::FindRecvProp(tbl, "m_nTickBase");
	if (!g_Prop) {
		PluginWarning("Required recv prop DT_LocalPlayerExclusive.m_nTickBase for module %s not found!\n", GetModuleName());
		return false;
	}

	return true;
}

void SrcTVPlus::EnableDetector()
{
	if (s_Detected) return; // Already detected, no need to enable
	if (!g_ProxyPusher.IsEmpty()) return; // Already enabled
	g_SeenEntities.clear();
	g_ProxyPusher = CreateVariablePusher(g_Prop->m_ProxyFn, &DetectorProxy);
}

void SrcTVPlus::DisableDetector()
{
	g_ProxyPusher.Clear();
	g_SeenEntities.clear();
}