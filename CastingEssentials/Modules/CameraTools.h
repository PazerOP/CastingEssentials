#pragma once
#include "PluginBase/ICameraOverride.h"
#include "PluginBase/Modules.h"

#include <mathlib/vector.h>
#include <shareddefs.h>

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

class CameraTools final : public Module<CameraTools>, public ICameraOverride
{
public:
	CameraTools();
	virtual ~CameraTools() = default;

	static bool CheckDependencies();

	void SpecPosition(const Vector& pos, const QAngle& angle, ObserverMode mode = OBS_MODE_FIXED);

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
	ConCommand* m_SpecPositionDelta;

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
	ConCommand* m_SpecIndex;

	ConCommand* m_ShowUsers;
	void ShowUsers(const CCommand& command);

	void ChangeForceMode(IConVar *var, const char *pOldValue, float flOldValue);
	void ChangeForceTarget(IConVar *var, const char *pOldValue, float flOldValue);
	void ToggleForceValidTarget(IConVar *var, const char *pOldValue, float flOldValue);

	bool ParseSpecPosCommand(const CCommand& command, Vector& pos, QAngle& angle, ObserverMode& mode,
							 const Vector& defaultPos, const QAngle& defaultAng, ObserverMode defaultMode) const;
	void SpecPosition(const CCommand &command);
	void SpecPositionDelta(const CCommand& command);

	void SpecClass(const CCommand& command);
	void SpecClass(TFTeam team, TFClassType playerClass, int classIndex);
	void SpecSteamID(const CCommand& command);
	void SpecIndex(const CCommand& command);

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