#pragma once
#include "PluginBase/ICameraOverride.h"
#include "PluginBase/Modules.h"

#include <convar.h>
#include <mathlib/vector.h>
#include <shareddefs.h>

enum class TFTeam;
enum class TFClassType;

class CCommand;
class IHandleEntity;
class KeyValues;
class C_HLTVCamera;
class C_BaseEntity;
class Player;

enum class ModeSwitchReason
{
	Unknown,
	SpecPosition,
};

class CameraTools final : public Module<CameraTools>, public ICameraOverride
{
public:
	CameraTools();
	virtual ~CameraTools() = default;

	static bool CheckDependencies();

	void SpecPosition(const Vector& pos, const QAngle& angle, ObserverMode mode = OBS_MODE_FIXED, float fov = -1);

	static float CollisionTest3D(const Vector& startPos, const Vector& targetPos, float scale,
	                             const IHandleEntity* ignoreEnt = nullptr);

	ModeSwitchReason GetModeSwitchReason() const { return m_SwitchReason; }

private:
	int m_SetModeHook;
	int m_SetPrimaryTargetHook;
	int m_GetDefaultFOVHook;
	KeyValues* m_SpecGUISettings;

	void SetModeOverride(int iMode);
	void SetPrimaryTargetOverride(int nEntity);

	ConVar ce_cameratools_show_mode;
	ConVar ce_cameratools_autodirector_mode;
	ConVar ce_cameratools_force_target;
	ConVar ce_cameratools_force_valid_target;
	ConVar ce_cameratools_spec_player_alive;

	ConVar ce_cameratools_taunt_thirdperson;

	ConVar ce_tplock_enable;
	ConVar ce_tplock_xoffset;
	ConVar ce_tplock_yoffset;
	ConVar ce_tplock_zoffset;

	ConVar ce_tplock_force_pitch;
	ConVar ce_tplock_force_yaw;
	ConVar ce_tplock_force_roll;

	ConVar ce_tplock_dps_pitch;
	ConVar ce_tplock_dps_yaw;
	ConVar ce_tplock_dps_roll;

	ConVar ce_tplock_bone;

	ConCommand ce_cameratools_spec_pos;
	ConCommand ce_cameratools_spec_pos_delta;
	ConCommand ce_cameratools_spec_class;
	ConCommand ce_cameratools_spec_steamid;
	ConCommand ce_cameratools_spec_index;
	ConCommand ce_cameratools_spec_entindex;

	ConCommand ce_cameratools_show_users;
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
	void SpecEntIndex(const CCommand& command);

	bool m_IsTaunting;
	void UpdateIsTaunting();

	void SpecPlayer(int playerIndex);

	void OnTick(bool inGame) override;

	struct TPLockRuleset
	{
		Vector m_PosOffset;
		QAngle m_AngOffset;
		Vector m_DPSLimit;    // Degrees per second
		const char* m_Bone;
	};

	Vector CalcPosForAngle(const TPLockRuleset& ruleset, const Vector& orbitCenter, const QAngle& angle) const;

	bool InToolModeOverride() const override;
	bool IsThirdPersonCameraOverride() const override { return m_IsTaunting; }
	bool SetupEngineViewOverride(Vector& origin, QAngle& angles, float& fov) override;
	QAngle m_LastFrameAngle;
	Player* m_LastTargetPlayer;

	static constexpr float TPLOCK_IGNORE = -INFINITY;
	static float TPLockReadFloat(const ConVar& cvar);

	void LoadDefaultRuleset(TPLockRuleset& ruleset) const;
	bool PerformTPLock(const TPLockRuleset& ruleset, Vector& origin, QAngle& angles, float& fov);

	void AttachHooks(bool attach);

	ModeSwitchReason m_SwitchReason;
};