#pragma once

class Player;

namespace vgui
{
	typedef unsigned int VPANEL;
	class Panel;
	class EditablePanel;
	class ImagePanel;
}

class HUDHacking
{
public:
	HUDHacking() = delete;
	~HUDHacking() = delete;

	static vgui::VPANEL GetSpecGUI();
	static Player* GetPlayerFromPanel(vgui::EditablePanel* playerPanel);

	// Like normal FindChildByName, but doesn't care about the fact we're in a different module.
	static vgui::Panel* FindChildByName(const char* name, bool recursive = false);

	static Color* GetFillColor(vgui::ImagePanel* imgPanel);
	static Color* GetDrawColor(vgui::ImagePanel* imgPanel);
};