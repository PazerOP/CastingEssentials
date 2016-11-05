// Based on https://developer.valvesoftware.com/wiki/VGUI_Image_Progress_Bar
#pragma once

#include <vgui/VGUI.h>
#include <vgui_controls/ProgressBar.h>

namespace vgui
{
	class ImageProgressBar : public ContinuousProgressBar
	{
		DECLARE_CLASS_SIMPLE(ImageProgressBar, ContinuousProgressBar);

	public:
		ImageProgressBar(Panel *parent, const char *panelName);
		ImageProgressBar(Panel *parent, const char *panelName, const char *topTexturename, const char *bottomTextureName);
		virtual ~ImageProgressBar() { }

		virtual void Paint(void);
		virtual void ApplySettings(KeyValues *inResourceData);
		virtual void GetSettings(KeyValues *outResourceData);

		void	SetTopTexture(const char *topTextureName);
		void	SetBottomTexture(const char *bottomTextureName);

	private:
		int	m_iTopTextureId;
		int	m_iBottomTextureId;
		char	m_szTopTextureName[64];
		char	m_szBottomTextureName[64];
	};
}
