#include "Modules/ClientTools.h"
#include "PluginBase/HookManager.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

MODULE_REGISTER(ClientTools);

ClientTools::ClientTools() :
	ce_clienttools_windowtitle("ce_clienttools_windowtitle", "", FCVAR_NONE, "Overrides the game window title",
		[](IConVar* var, const char* oldval, float foldval) { GetModule()->UpdateWindowTitle(oldval); })
{

}

HWND GetMainWindow()
{
	class IGame
	{
	public:
		virtual        ~IGame(void) { }
		virtual bool   Init(void *pvInstance) = 0;
		virtual bool   Shutdown(void) = 0;
		virtual bool   CreateGameWindow(void) = 0;
		virtual void   DestroyGameWindow(void) = 0;
		virtual void   SetGameWindow(void* hWnd) = 0;
		virtual bool   InputAttachToGameWindow() = 0;
		virtual void   InputDetachFromGameWindow() = 0;
		virtual void   PlayStartupVideos(void) = 0;
		virtual void*  GetMainWindow(void) = 0;
		virtual void** GetMainWindowAddress(void) = 0;
		virtual void   GetDesktopInfo(int &width, int &height, int &refreshrate) = 0;
		virtual void   SetWindowXY(int x, int y) = 0;
		virtual void   SetWindowSize(int w, int h) = 0;
		virtual void   GetWindowRect(int *x, int *y, int *w, int *h) = 0;
		virtual bool   IsActiveApp(void) = 0;
		virtual void   DispatchAllStoredGameMessages() = 0;
	};

	static IGame* g_pGame = nullptr;
	if (!g_pGame)
	{
		constexpr auto SIG = "\x55\x8B\xEC\x83\xEC\x14\x8B\x0D\x00\x00\x00\x00\xC7\x45\xEC\x14\x00\x00\x00\x8B\x01\xFF\x50\x24\x89\x45\xF0\x8D\x45\xEC\x50\xC7\x45\xF4\x03\x00\x00\x00\xC7\x45\xF8\x05\x00\x00\x00\xC7\x45\xFC\x00\x00\x00\x00\xFF\x15\x00\x00\x00\x00\x8B\xE5\x5D\xC3";
		constexpr auto MASK = "xxxxxxxx????xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxxx";
		constexpr auto OFFSET = 8;

		auto target = (IGame***)((char*)SignatureScan("engine", SIG, MASK) + OFFSET);
		if (target == (IGame***)OFFSET)
			throw bad_pointer("CGame");

		g_pGame = **target;
	}
	return (HWND)g_pGame->GetMainWindow();
}

#include "CameraTools.h"

bool ClientTools::CheckDependencies()
{
	bool ready = true;

	try
	{
		GetMainWindow();
	}
	catch (const bad_pointer& ptrException)
	{
		PluginWarning("Required interface %s for module %s not available!\n", ptrException.what(), GetModuleName());
		ready = false;
	}

	return ready;
}

void ClientTools::UpdateWindowTitle(const char* oldval)
{
	auto newval = ce_clienttools_windowtitle.GetString();
	if (!newval[0])
	{
		if (oldval[0])
			SetWindowText(GetMainWindow(), m_OriginalTitle.c_str());
	}
	else
	{
		if (m_OriginalTitle.empty())
		{
			const auto length = GetWindowTextLength(GetMainWindow()) + 1;
			m_OriginalTitle.resize(length);
			GetWindowText(GetMainWindow(), m_OriginalTitle.data(), length);
		}

		SetWindowText(GetMainWindow(), newval);
	}
}