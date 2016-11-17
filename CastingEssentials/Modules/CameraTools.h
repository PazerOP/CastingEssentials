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
	static const char* GetModuleName() { return Modules().GetModuleName<CameraTools>().c_str(); }

	void SpecPosition(const Vector& pos, const QAngle& angle);

private:
	int m_SetModeHook;
	int m_SetPrimaryTargetHook;
	KeyValues* m_SpecGUISettings;

	void SetModeOverride(int iMode);
	void SetPrimaryTargetOverride(int nEntity);

	ConVar* m_ForceMode;
	ConVar* m_ForceTarget;
	ConVar* m_ForceValidTarget;
	ConVar* m_SpecPlayerAlive;
	ConCommand* m_SpecPosition;

	ConCommand* m_SpecClass;
	ConCommand* m_SpecSteamID;

	ConCommand* m_ShowUsers;
	void ShowUsers(const CCommand& command);

	void ChangeForceMode(IConVar *var, const char *pOldValue, float flOldValue);
	void ChangeForceTarget(IConVar *var, const char *pOldValue, float flOldValue);
	void SpecPosition(const CCommand &command);
	void ToggleForceValidTarget(IConVar *var, const char *pOldValue, float flOldValue);

	void SpecClass(const CCommand& command);
	void SpecClass(TFTeam team, TFClassType playerClass, int classIndex);
	void SpecSteamID(const CCommand& command);

	void SpecPlayer(int playerIndex);
};