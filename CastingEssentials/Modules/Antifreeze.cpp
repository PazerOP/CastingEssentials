
#include "antifreeze.h"
#include "PluginBase/Interfaces.h"

#include <client/iclientmode.h>
#include <tier3/tier3.h>
#include <toolframework/ienginetool.h>
#include <vgui/IPanel.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Panel.h>
#include <vprof.h>

class AntiFreeze::Panel : public vgui::Panel
{
public:
	Panel(vgui::Panel *parent, const char *panelName) : vgui::Panel(parent, panelName) { }
	virtual ~Panel() { }

	virtual void OnTick();
};

AntiFreeze::AntiFreeze() :
	ce_antifreeze_enabled("ce_antifreeze_enabled", "0", FCVAR_NONE, "enable antifreeze (forces the spectator GUI to refresh)")
{
}

bool AntiFreeze::CheckDependencies()
{
	bool ready = true;

	if (!Interfaces::GetEngineTool())
	{
		PluginWarning("Required interface IEngineTool for module %s not available!\n", GetModuleName());
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

void AntiFreeze::OnTick(bool inGame)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);

	if (inGame && ce_antifreeze_enabled.GetBool())
	{
		if (!m_Panel)
		{
			try
			{
				vgui::Panel* const viewport = Interfaces::GetClientMode()->GetViewport();
				if (viewport)
				{
					for (int i = 0; i < g_pVGuiPanel->GetChildCount(viewport->GetVPanel()); i++)
					{
						vgui::VPANEL childPanel = g_pVGuiPanel->GetChild(viewport->GetVPanel(), i);

						if (!strcmp(g_pVGuiPanel->GetName(childPanel), "specgui"))
						{
							m_Panel.reset(new Panel(viewport, "AntiFreeze"));
							m_Panel->SetParent(childPanel);
							return;
						}
					}

					Warning("Could not initialize the panel!\n");
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

		Interfaces::GetEngineTool()->ForceSend();
		Interfaces::GetEngineTool()->ForceUpdateDuringPause();
	}
	else if (m_Panel)
		m_Panel.reset();
}

void AntiFreeze::Panel::OnTick()
{
	g_pVGuiPanel->GetPanel(GetVParent(), "ClientDLL")->OnCommand("performlayout");
}