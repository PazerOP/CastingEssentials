#pragma once
#include "PluginBase/ICameraOverride.h"
#include "PluginBase/Modules.h"

#include <mathlib/vector.h>

enum class TFTeam;
enum class TFClassType;

class CCommand;
class ConCommand;
class ConVar;
class KeyValues;
class IConVar;
class C_HLTVCamera;
class C_BaseEntity;
class Player;

class CameraTools final : public Module, public ICameraOverride
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

	ConVar* ce_cameratools_show_mode;
	ConVar* m_ForceMode;
	ConVar* m_ForceTarget;
	ConVar* m_ForceValidTarget;
	ConVar* m_SpecPlayerAlive;
	ConCommand* m_SpecPosition;

	ConVar* m_TPXShift;
	ConVar* m_TPYShift;
	ConVar* m_TPZShift;
	ConVar* m_TPLockEnabled;

	ConVar* m_TPLockPitch;
	ConVar* m_TPLockYaw;
	ConVar* m_TPLockRoll;

	ConVar* m_TPLockXDPS;
	ConVar* m_TPLockYDPS;
	ConVar* m_TPLockZDPS;

	ConVar* m_TPLockBone;

	ConVar* m_NextPlayerMode;
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

	void OnTick(bool inGame) override;

	Vector CalcPosForAngle(const Vector& orbitCenter, const QAngle& angle);

	bool InToolModeOverride() const override { return m_ViewOverride; }
	bool IsThirdPersonCameraOverride() const override { return m_ViewOverride; }
	bool SetupEngineViewOverride(Vector& origin, QAngle& angles, float& fov) override;
	bool m_ViewOverride;
	QAngle m_LastFrameAngle;
	Player* m_LastTargetPlayer;
};