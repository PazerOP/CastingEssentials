#include "Hooking/IBaseHook.h"

#include <client/c_baseanimating.h>
#include <client/c_baseplayer.h>
#include <client/c_fire_smoke.h>
#include <client/game_controls/baseviewport.h>
#include <../materialsystem/texturemanager.h>
#include <shared/baseviewmodel_shared.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/ProgressBar.h>

#define OFFSET_CHECK(className, memberName, expectedOffset)														\
	static_assert(!(offsetof(className, memberName) > expectedOffset), "Actual offset greater than expected!");	\
	static_assert(!(offsetof(className, memberName) < expectedOffset), "Actual offset less than expected!");

// Don't trust intellisense... just use the actual errors the compiler emits and
// adjust your padding until you get it right
class OffsetChecking
{
	OFFSET_CHECK(C_BaseAnimating, m_nHitboxSet, 1376);
	OFFSET_CHECK(C_BaseAnimating, m_bDynamicModelPending, 2185);
	OFFSET_CHECK(C_BaseAnimating, m_pStudioHdr, 2192);

	OFFSET_CHECK(C_BaseEntity, m_lifeState, 165);
	OFFSET_CHECK(C_BaseEntity, m_bDormant, 426);
	OFFSET_CHECK(C_BaseEntity, m_Particles, 596);

	OFFSET_CHECK(C_BasePlayer, m_iFOVStart, 4152);
	OFFSET_CHECK(C_BasePlayer, m_iDefaultFOV, 4160);
	OFFSET_CHECK(C_BasePlayer, m_hVehicle, 4300);
	OFFSET_CHECK(C_BasePlayer, m_hViewModel, 4472);

	OFFSET_CHECK(C_BaseViewModel, m_nViewModelIndex, 2232);
	OFFSET_CHECK(C_BaseViewModel, m_nAnimationParity, 2248);
	OFFSET_CHECK(C_BaseViewModel, m_sVMName, 2252);
	OFFSET_CHECK(C_BaseViewModel, m_sAnimationPrefix, 2256);

	OFFSET_CHECK(C_EntityFlame, m_hEffect, 1360);

	OFFSET_CHECK(vgui::ContinuousProgressBar, _unknown0, 388);
	OFFSET_CHECK(vgui::ContinuousProgressBar, _unknown1, 392);
	OFFSET_CHECK(vgui::ContinuousProgressBar, _unknown2, 396);

	OFFSET_CHECK(studiohdr_t, numbones, 156);

	OFFSET_CHECK(vgui::PanelMessageMap, baseMap, 24);

	OFFSET_CHECK(vgui::Panel, m_pDragDrop, 44);
	OFFSET_CHECK(vgui::Panel, m_bToolTipOverridden, 60);
	OFFSET_CHECK(vgui::Panel, _panelName, 84);
	OFFSET_CHECK(vgui::Panel, _border, 88);
	OFFSET_CHECK(vgui::Panel, _flags, 92);
	OFFSET_CHECK(vgui::Panel, _cursor, 156);
	OFFSET_CHECK(vgui::Panel, _buildModeFlags, 160);
	OFFSET_CHECK(vgui::Panel, m_flAlpha, 296);
	OFFSET_CHECK(vgui::Panel, m_nPaintBackgroundType, 304);
	OFFSET_CHECK(vgui::Panel, m_nBgTextureId1, 312);
	OFFSET_CHECK(vgui::Panel, m_nBgTextureId2, 320);
	OFFSET_CHECK(vgui::Panel, m_nBgTextureId3, 328);
	OFFSET_CHECK(vgui::Panel, m_nBgTextureId4, 336);
	OFFSET_CHECK(vgui::Panel, m_roundedCorners, 340);

	OFFSET_CHECK(vgui::ImagePanel, m_pImage, 348);
	OFFSET_CHECK(vgui::ImagePanel, m_pszImageName, 352);
	OFFSET_CHECK(vgui::ImagePanel, m_FillColor, 376);
	OFFSET_CHECK(vgui::ImagePanel, m_DrawColor, 380);

	OFFSET_CHECK(vgui::ProgressBar, m_iProgressDirection, 352);
	OFFSET_CHECK(vgui::ProgressBar, _progress, 356);
	OFFSET_CHECK(vgui::ProgressBar, _segmentGap, 364);
	OFFSET_CHECK(vgui::ProgressBar, _segmentWide, 368);
	OFFSET_CHECK(vgui::ProgressBar, m_iBarInset, 372);
	OFFSET_CHECK(vgui::ProgressBar, m_iBarMargin, 376);

public:
	OffsetChecking()
	{
#ifdef DEBUG
		int offset;
#endif

		Assert((offset = Hooking::VTableOffset(&Panel::SetVisible)) == 33);
		Assert((offset = Hooking::VTableOffset(&Panel::SetEnabled)) == 54);

		{
			Assert((offset = Hooking::VTableOffset(&EditablePanel::LoadControlSettings)) == 212);
			Assert((offset = Hooking::VTableOffset(&EditablePanel::LoadUserConfig)) == 213);
			Assert((offset = Hooking::VTableOffset(&EditablePanel::SaveUserConfig)) == 214);
			Assert((offset = Hooking::VTableOffset(&EditablePanel::LoadControlSettingsAndUserConfig)) == 215);
			Assert((offset = Hooking::VTableOffset(&EditablePanel::ActivateBuildMode)) == 216);
			Assert((offset = Hooking::VTableOffset(&EditablePanel::GetBuildGroup)) == 217);
			Assert((offset = Hooking::VTableOffset(&EditablePanel::CreateControlByName)) == 218);

			Assert((offset = Hooking::VTableOffset(&EditablePanel::GetControlInt)) == 222);
			Assert((offset = Hooking::VTableOffset(&EditablePanel::SetControlEnabled)) == 225);
			Assert((offset = Hooking::VTableOffset(&EditablePanel::SetControlVisible)) == 226);

			Assert((offset = Hooking::VTableOffset(
				static_cast<void(EditablePanel::*)(const char*, float)>(&EditablePanel::SetDialogVariable))) == 227);
			Assert((offset = Hooking::VTableOffset(
				static_cast<void(EditablePanel::*)(const char*, int)>(&EditablePanel::SetDialogVariable))) == 228);
			Assert((offset = Hooking::VTableOffset(
				static_cast<void(EditablePanel::*)(const char*, const wchar_t*)>(&EditablePanel::SetDialogVariable))) == 229);
			Assert((offset = Hooking::VTableOffset(
				static_cast<void (EditablePanel::*)(const char*, const char*)>(&EditablePanel::SetDialogVariable))) == 230);
		}

		Assert((offset = Hooking::VTableOffset(&EditablePanel::RegisterControlSettingsFile)) == 231);
		Assert((offset = Hooking::VTableOffset(&CBaseViewport::RegisterControlSettingsFile)) == 231);

		Assert((offset = Hooking::VTableOffset(&CBaseViewport::CreatePanelByName)) == 237);
		//Assert((offset = Hooking::VTableOffset(&CBaseViewport::FindPanelByName)) == 238);
		//Assert((offset = Hooking::VTableOffset(&CBaseViewport::GetActivePanel)) == 239);
		Assert((offset = Hooking::VTableOffset(&CBaseViewport::AddNewPanel)) == 239);
		Assert((offset = Hooking::VTableOffset(&CBaseViewport::CreateDefaultPanels)) == 240);
		Assert((offset = Hooking::VTableOffset(&CBaseViewport::Start)) == 241);
		Assert((offset = Hooking::VTableOffset(&CBaseViewport::ReloadScheme)) == 242);
		Assert((offset = Hooking::VTableOffset(&CBaseViewport::AllowedToPrintText)) == 245);
		Assert((offset = Hooking::VTableOffset(&CBaseViewport::GetAnimationController)) == 248);

		Assert((offset = Hooking::VTableOffset(&ITextureManager::FindNext)) == 32);
	}
};
#ifdef DEBUG
static OffsetChecking s_OffsetChecking;
#endif