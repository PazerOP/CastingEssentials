#include "StubPanel.h"

#include <vgui/IVGui.h>
#include <vgui_controls/Controls.h>

using namespace vgui;

StubPanel::StubPanel()
{
	m_VPanel = ivgui()->AllocPanel();
	ipanel()->Init(GetVPanel(), this);

	ivgui()->AddTickSignal(GetVPanel());
}

vgui::StubPanel::~StubPanel()
{
	ivgui()->RemoveTickSignal(GetVPanel());
	ivgui()->FreePanel(GetVPanel());
	m_VPanel = 0;
}