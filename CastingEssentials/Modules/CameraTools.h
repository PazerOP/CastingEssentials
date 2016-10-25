#pragma once
#include "PluginBase/Modules.h"

enum class TFTeam;
enum class TFClassType;

class CCommand;
class ConCommand;
class ConVar;
class KeyValues;
class IConVar;
class C_HLTVCamera;

class CameraTools final : public Module
{
public:
	CameraTools();
	virtual ~CameraTools() = default;

	static bool CheckDependencies();
	static CameraTools* GetModule() { return Modules().GetModule<CameraTools>(); }

private:
	int m_SetModeHook;
	int m_SetPrimaryTargetHook;
	KeyValues* m_SpecGUISettings;

	void SetModeOverride(C_HLTVCamera *hltvcamera, int &iMode);
	void SetPrimaryTargetOverride(C_HLTVCamera *hltvcamera, int &nEntity);

	ConVar* m_ForceMode;
	ConVar* m_ForceTarget;
	ConVar* m_ForceValidTarget;
	ConVar* m_SpecPlayerAlive;
	ConCommand* m_SpecPosition;

	ConCommand* m_SpecClass;
	ConCommand* m_SpecSteamID;

	ConCommand* m_ShowUsers;
	static void StaticShowUsers(const CCommand& command);
	void ShowUsers(const CCommand& command);

	static void StaticChangeForceMode(IConVar* var, const char* oldValue, float fOldValue);
	void ChangeForceMode(IConVar *var, const char *pOldValue, float flOldValue);
	static void StaticChangeForceTarget(IConVar* var, const char* oldValue, float fOldValue);
	void ChangeForceTarget(IConVar *var, const char *pOldValue, float flOldValue);
	static void StaticSpecPosition(const CCommand& command);
	void SpecPosition(const CCommand &command);
	static void StaticToggleForceValidTarget(IConVar* var, const char* oldValue, float fOldValue);
	void ToggleForceValidTarget(IConVar *var, const char *pOldValue, float flOldValue);

	static void StaticSpecClass(const CCommand& command);
	void SpecClass(const CCommand& command);
	void SpecClass(TFTeam team, TFClassType playerClass, int classIndex);
	static void StaticSpecSteamID(const CCommand& command);
	void SpecSteamID(const CCommand& command);

	void SpecPlayer(int playerIndex);
};