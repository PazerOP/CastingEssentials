#include "VariableLabel.h"

#include <string>
#include <KeyValues.h>

inline void FindAndReplaceInString(std::string &str, const std::string &find, const std::string &replace)
{
	if (find.empty())
		return;

	const char* findstr = find.c_str();
	const char* found = nullptr;
	const char* base = str.c_str();

	while ((found = stristr(base, findstr)) != nullptr)
	{
		const auto offset = found - base;
		str.replace(offset, find.length(), replace);
		base = str.c_str() + offset + find.length();	// In case we get reallocated
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