#include "VariableLabel.h"

#include <string>
#include <KeyValues.h>

inline void FindAndReplaceInString(std::string &str, const std::string &find, const std::string &replace)
{
	if (find.empty())
		return;

	size_t start_pos = 0;

	while ((start_pos = str.find(find, start_pos)) != std::string::npos)
	{
		str.replace(start_pos, find.length(), replace);
		start_pos += replace.length();
	}
}

using namespace vgui;

DECLARE_BUILD_FACTORY_DEFAULT_TEXT(VariableLabel, VariableLabel);

VariableLabel::VariableLabel(Panel *parent, const char *panelName, const char *labelText) : BaseClass(parent, panelName, labelText)
{
	m_sLabelText = labelText;
}

void VariableLabel::ApplySettings(KeyValues *inResourceData)
{
	m_sLabelText = inResourceData->GetString("labelText");

	BaseClass::ApplySettings(inResourceData);
}

void VariableLabel::GetSettings(KeyValues *outResourceData)
{
	BaseClass::GetSettings(outResourceData);

	outResourceData->SetString("labelText", m_sLabelText.c_str());
}

void VariableLabel::OnDialogVariablesChanged(KeyValues *dialogVariables)
{
	std::string text = m_sLabelText;

	FOR_EACH_VALUE(dialogVariables, variable)
	{
		std::string find = "%";
		find.append(variable->GetName());
		find.append("%");

		std::string replace = variable->GetString();

		FindAndReplaceInString(text, find, replace);
	}

	SetText(text.c_str());
}