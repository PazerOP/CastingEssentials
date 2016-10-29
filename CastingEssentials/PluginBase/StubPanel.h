#pragma once
#include <vgui/IClientPanel.h>

namespace vgui
{
	// A panel whose sole purporse is to hook into OnTick() (once-per-frame) events.
	class StubPanel : public IClientPanel
	{
	public:
		StubPanel();
		virtual ~StubPanel();

	private:
		virtual VPANEL GetVPanel() override final { return m_VPanel; }
		VPANEL m_VPanel;

		virtual void Think() override final { }
		virtual void PerformApplySchemeSettings() override final { }
		virtual void PaintTraverse(bool, bool) override final { }
		virtual void Repaint() override final { }
		virtual VPANEL IsWithinTraverse(int, int, bool) override final { return 0; }
		virtual void GetInset(int&, int&, int&, int&) override final { }
		virtual void GetClipRect(int&, int&, int&, int&) override final { }
		virtual void OnChildAdded(VPANEL) override final { }
		virtual void OnSizeChanged(int, int) override final { }

		virtual void InternalFocusChanged(bool) override final { }
		virtual bool RequestInfo(KeyValues*) override final { return false; }
		virtual void RequestFocus(int) override final { }
		virtual bool RequestFocusPrev(VPANEL) override final { return false; }
		virtual bool RequestFocusNext(VPANEL) override final { return false; }
		virtual void OnMessage(const KeyValues*, VPANEL) override final { }
		virtual VPANEL GetCurrentKeyFocus() override final { return 0; }
		virtual int GetTabPosition() override final { return 0; }

		virtual const char* GetName() override final { return "CastingEssentialsStubPanel"; }
		virtual const char* GetClassName() override final { return "vgui::CastingEssentialsStubPanel"; }

		virtual HScheme GetScheme() override final { return 0; }
		virtual bool IsProportional() override final { return false; }
		virtual bool IsAutoDeleteSet() override final { return false; }
		virtual void DeletePanel() override final { delete this; }

		virtual void* QueryInterface(EInterfaceID) override final { return nullptr; }

		virtual Panel* GetPanel() override final { return nullptr; }

		virtual const char* GetModuleName() override final { return "CastingEssentials"; }
	};
}