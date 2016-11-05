#pragma once

#include <string>
#include <vgui/VGUI.h>
#include <vgui_controls/Label.h>

namespace vgui
{
	class VariableLabel : public Label
	{
		DECLARE_CLASS_SIMPLE(VariableLabel, Label);

	public:
		VariableLabel(Panel *parent, const char *panelName, const char *labelText);
		virtual ~VariableLabel() { }

		virtual void ApplySettings(KeyValues *inResourceData);
		virtual void GetSettings(KeyValues *outResourceData);

	protected:
		MESSAGE_FUNC_PARAMS(OnDialogVariablesChanged, "DialogVariables", dialogVariables);

	private:
		std::string m_sLabelText;
	};
}