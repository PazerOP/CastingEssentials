// Based on https://developer.valvesoftware.com/wiki/VGUI_Image_Progress_Bar

#include "ImageProgressBar.h"

#include <vgui/ISurface.h>
#include <KeyValues.h>

namespace vgui
{
	DECLARE_BUILD_FACTORY(ImageProgressBar);

	ImageProgressBar::ImageProgressBar(Panel *parent, const char *panelName) : BaseClass(parent, panelName)
	{
		m_iTopTextureId = m_iBottomTextureId = -1;
		memset(m_szTopTextureName, 0, sizeof(m_szTopTextureName));
		memset(m_szBottomTextureName, 0, sizeof(m_szBottomTextureName));
	}

	ImageProgressBar::ImageProgressBar(vgui::Panel *parent, const char *panelName, const char *topTextureName, const char *bottomTextureName) : BaseClass(parent, panelName)
	{
		SetTopTexture(topTextureName);
		SetBottomTexture(bottomTextureName);
	}

	void ImageProgressBar::SetTopTexture(const char *topTextureName)
	{
		if (topTextureName != NULL && topTextureName[0])
		{
			Q_strncpy(m_szTopTextureName, topTextureName, 64);
			m_iTopTextureId = surface()->DrawGetTextureId(topTextureName);
			if (m_iTopTextureId == -1)
			{
				m_iTopTextureId = surface()->CreateNewTextureID();
				surface()->DrawSetTextureFile(m_iTopTextureId, topTextureName, false, true);
			}
		}
		else
		{
			memset(m_szTopTextureName, 0, sizeof(m_szTopTextureName));
			m_iTopTextureId = -1;
		}
	}

	void ImageProgressBar::SetBottomTexture(const char *bottomTextureName)
	{
		if (bottomTextureName != NULL && bottomTextureName[0])
		{
			Q_strncpy(m_szBottomTextureName, bottomTextureName, 64);
			m_iBottomTextureId = surface()->DrawGetTextureId(bottomTextureName);
			if (m_iBottomTextureId == -1)
			{
				m_iBottomTextureId = surface()->CreateNewTextureID();
				surface()->DrawSetTextureFile(m_iBottomTextureId, bottomTextureName, false, true);
			}
		}
		else
		{
			memset(m_szBottomTextureName, 0, sizeof(m_szBottomTextureName));
			m_iBottomTextureId = -1;
		}
	}

	void ImageProgressBar::ApplySettings(KeyValues *inResourceData)
	{
		const char *topTextureName = inResourceData->GetString("TopImage", "");
		SetTopTexture(topTextureName);

		const char *bottomTextureName = inResourceData->GetString("BottomImage", "");
		SetBottomTexture(bottomTextureName);

		const char *directionName = inResourceData->GetString("direction", "");
		if (stricmp(directionName, "east") == 0)
		{
			SetProgressDirection(PROGRESS_EAST);
		}
		else if (stricmp(directionName, "west") == 0)
		{
			SetProgressDirection(PROGRESS_WEST);
		}
		else if (stricmp(directionName, "north") == 0)
		{
			SetProgressDirection(PROGRESS_NORTH);
		}
		else if (stricmp(directionName, "south") == 0)
		{
			SetProgressDirection(PROGRESS_SOUTH);
		}

		BaseClass::ApplySettings(inResourceData);
	}

	void ImageProgressBar::GetSettings(KeyValues *outResourceData)
	{
		BaseClass::GetSettings(outResourceData);
		outResourceData->SetString("TopImage", m_szTopTextureName);
		outResourceData->SetString("BottomImage", m_szBottomTextureName);
		if (GetProgressDirection() == PROGRESS_EAST)
		{
			outResourceData->SetString("direction", "east");
		}
		else if (GetProgressDirection() == PROGRESS_WEST)
		{
			outResourceData->SetString("direction", "west");
		}
		else if (GetProgressDirection() == PROGRESS_NORTH)
		{
			outResourceData->SetString("direction", "north");
		}
		else if (GetProgressDirection() == PROGRESS_SOUTH)
		{
			outResourceData->SetString("direction", "south");
		}
	}

	void ImageProgressBar::Paint(void)
	{
		// If there is no TopImage, revert to ContinousProgressBar::Paint()
		if (m_iTopTextureId == -1)
			return BaseClass::Paint();

		int x = 0, y = 0;
		int wide, tall;
		GetSize(wide, tall);

		if (m_iBottomTextureId != -1)
		{
			surface()->DrawSetTexture(m_iBottomTextureId);
			surface()->DrawSetColor(255, 255, 255, 255);
			surface()->DrawTexturedRect(x, y, wide, tall);
		}

		surface()->DrawSetTexture(m_iTopTextureId);
		surface()->DrawSetColor(255, 255, 255, 255);
		switch (m_iProgressDirection)
		{
			case PROGRESS_EAST:
				surface()->DrawTexturedSubRect(x, y, x + (int)(wide * _progress), y + tall,
					0.0f, 0.0f, _progress, 1.0f);
				break;
			case PROGRESS_WEST:
				surface()->DrawTexturedSubRect(x + (int)(wide * (1.0f - _progress)), y, x + wide, y + tall,
					1.0f - _progress, 0.0f, 1.0f, 1.0f);
				break;
			case PROGRESS_NORTH:
				surface()->DrawTexturedSubRect(x, y + (int)(tall * (1.0f - _progress)), x + wide, y + tall,
					0.0f, 0.0f, 1.0f, _progress);
				break;
			case PROGRESS_SOUTH:
				surface()->DrawTexturedSubRect(x, y, x + wide, y + (int)(tall * _progress),
					0.0f, 1.0f - _progress, 1.0f, 1.0f);
				break;
		}
	}
}