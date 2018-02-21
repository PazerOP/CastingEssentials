#include "freezeinfo.h"

#include <client/iclientmode.h>
#include <tier3/tier3.h>
#include <toolframework/ienginetool.h>
#include <vgui/IVGui.h>
#include <vgui_controls/EditablePanel.h>
#include <vprof.h>

#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"

class FreezeInfo::Panel : public vgui::EditablePanel
{
public:
	Panel(vgui::Panel *parent, const char *panelName);
	virtual ~Panel() { }

	virtual void OnTick();

	void SetDisplayThreshold(float threshold);
	void PostEntityPacketReceivedHook();

private:
	float lastUpdate;
	float threshold;
};

FreezeInfo::FreezeInfo() :
	ce_freezeinfo_enabled("ce_freezeinfo_enabled", "0", FCVAR_NONE, "enables display of an info panel when a freeze is detected"),
	ce_freezeinfo_threshold("ce_freezeinfo_threshold", "1", FCVAR_NONE, "the time of a freeze (in seconds) before the info panel is displayed",
		[](IConVar *var, const char *pOldValue, float flOldValue) { GetModule()->ChangeThreshold(var, pOldValue, flOldValue); }),
	ce_freezeinfo_reload_settings("ce_freezeinfo_reload_settings", []() { GetModule()->ReloadSettings(); },
		"reload settings for the freeze info panel from the resource file", FCVAR_NONE)
{
	m_PostEntityPacketReceivedHook = GetHooks()->AddHook<IPrediction_PostEntityPacketReceived>(std::bind(&FreezeInfo::PostEntityPacketReceivedHook, this));
}

FreezeInfo::~FreezeInfo()
{
	if (m_PostEntityPacketReceivedHook && GetHooks()->RemoveHook<IPrediction_PostEntityPacketReceived>(m_PostEntityPacketReceivedHook, __FUNCSIG__))
		m_PostEntityPacketReceivedHook = 0;

	Assert(!m_PostEntityPacketReceivedHook);
}

bool FreezeInfo::CheckDependencies()
{
	bool ready = true;

	if (!Interfaces::GetEngineTool())
	{
		PluginWarning("Required interface IEngineTool for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Interfaces::GetPrediction())
	{
		PluginWarning("Required interface IPrediction for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Interfaces::AreVguiLibrariesAvailable())
	{
		PluginWarning("Required VGUI library for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!g_pVGui)
	{
		PluginWarning("Required interface vgui::IVGui for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!g_pVGuiPanel)
	{
		PluginWarning("Required interface vgui::IPanel for module %s not available!\n", GetModuleName());
		ready = false;
	}

	try
	{
		Interfaces::GetClientMode();
	}
	catch (bad_pointer)
	{
		PluginWarning("Module %s requires IClientMode, which cannot be verified at this time!\n", GetModuleName());
	}

	return ready;
}

void FreezeInfo::OnTick(bool inGame)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (inGame && ce_freezeinfo_enabled.GetBool())
	{
		if (!m_Panel)
		{
			try
			{
				vgui::Panel* const viewport = Interfaces::GetClientMode()->GetViewport();
				if (viewport)
				{
					m_Panel.reset(new Panel(viewport, "FreezeInfo"));
					m_Panel->SetDisplayThreshold(ce_freezeinfo_threshold.GetFloat());
				}
				else
					Warning("Could not initialize the panel!\n");
			}
			catch (bad_pointer)
			{
				Warning("Could not initialize the panel!\n");
			}
		}

		if (m_Panel)
		{
			if (!m_Panel->IsEnabled())
				m_Panel->SetEnabled(true);

			m_Panel->OnTick();
		}
	}
	else if (m_Panel)
		m_Panel.reset();
}

void FreezeInfo::PostEntityPacketReceivedHook()
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (m_Panel)
		m_Panel->PostEntityPacketReceivedHook();
}

void FreezeInfo::ChangeThreshold(IConVar *var, const char *pOldValue, float flOldValue)
{
	if (m_Panel)
		m_Panel->SetDisplayThreshold(ce_freezeinfo_threshold.GetFloat());
}

void FreezeInfo::ReloadSettings()
{
	if (m_Panel)
		m_Panel->LoadControlSettings("Resource/UI/FreezeInfo.res");
}

FreezeInfo::Panel::Panel(vgui::Panel *parent, const char *panelName) : vgui::EditablePanel(parent, panelName)
{
	LoadControlSettings("Resource/UI/FreezeInfo.res");
}

void FreezeInfo::Panel::OnTick()
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	const float interval = Interfaces::GetEngineTool()->HostTime() - lastUpdate;
	if (interval > threshold)
	{
		if (!IsVisible())
			SetVisible(true);

		const int seconds = int(floor(interval)) % 60;
		const int minutes = int(floor(interval)) / 60;

		char timeBuffer[32];
		V_snprintf(timeBuffer, sizeof(timeBuffer), "%i:%02i", minutes, seconds);
		SetDialogVariable("time", timeBuffer);
	}
	else
	{
		if (IsVisible())
			SetVisible(false);
	}
}

void FreezeInfo::Panel::SetDisplayThreshold(float displayThreshold)
{
	threshold = displayThreshold;
}

void FreezeInfo::Panel::PostEntityPacketReceivedHook()
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	lastUpdate = Interfaces::GetEngineTool()->HostTime();
}