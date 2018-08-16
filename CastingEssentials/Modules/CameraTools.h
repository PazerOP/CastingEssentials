#pragma once
#include "PluginBase/EntityOffset.h"
#include "PluginBase/Hook.h"
#include "PluginBase/ICameraOverride.h"
#include "PluginBase/Modules.h"

#include <convar.h>
#include <mathlib/vector.h>
#include <shareddefs.h>

#include <array>
#include <optional>

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
	static constexpr __forceinline const char* GetModuleName() { return "Camera Tools"; }

	void SpecPosition(const Vector& pos, const QAngle& angle, ObserverMode mode = OBS_MODE_FIXED, float fov = -1);

	static float CollisionTest3D(const Vector& startPos, const Vector& targetPos, float scale,
	                             const IHandleEntity* ignoreEnt = nullptr);

	ModeSwitchReason GetModeSwitchReason() const { return m_SwitchReason; }

protected:
	bool InToolModeOverride() const override;
	bool IsThirdPersonCameraOverride() const override { return m_IsTaunting; }
	bool SetupEngineViewOverride(Vector& origin, QAngle& angles, float& fov) override;

private:
	Hook<HookFunc::C_HLTVCamera_SetMode> m_SetModeHook;
	Hook<HookFunc::C_HLTVCamera_SetPrimaryTarget> m_SetPrimaryTargetHook;
	KeyValues* m_SpecGUISettings;

	void SetModeOverride(int iMode);
	void SetPrimaryTargetOverride(int nEntity);

	ConVar ce_cameratools_show_mode;
	ConVar ce_cameratools_autodirector_mode;
	ConVar ce_cameratools_force_target;
	ConVar ce_cameratools_force_valid_target;
	ConVar ce_cameratools_spec_player_alive;
	ConVar ce_cameratools_fix_view_heights;
	ConVar ce_cameratools_disable_view_punches;

	ConVar ce_tplock_enable;
	ConVar ce_tplock_taunt_enable;

	ConVar ce_tplock_default_pos;
	ConVar ce_tplock_default_angle;
	ConVar ce_tplock_default_dps;

	ConVar ce_tplock_taunt_pos;
	ConVar ce_tplock_taunt_angle;
	ConVar ce_tplock_taunt_dps;

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

	struct TPLockValue
	{
		enum class Mode
		{
			Set,
			Add,
			Scale,
			ScaleAdd,
		};

		Mode m_Mode;
		float m_Value;
		float m_Base;

		float GetValue(float input) const;
	};
	static bool ParseTPLockValues(const CCommand& valuesIn, std::array<TPLockValue, 3>& valuesOut);
	static void ParseTPLockValuesInto(ConVar* cvar, const char* oldVal, std::array<TPLockValue, 3>& values);
	static void ParseTPLockValuesInto(ConVar* cvar, const char* oldVal, std::array<float, 3>& values);
	void TPLockBoneUpdated(ConVar* cvar);

	struct TPLockRuleset
	{
		std::array<float, 3> m_Pos;
		std::array<TPLockValue, 3> m_Angle;
		std::array<float, 3> m_DPS;
		std::string m_Bone;
	};

	TPLockRuleset m_TPLockDefault;
	TPLockRuleset m_TPLockTaunt;

	void SpecClass(const CCommand& command);
	void SpecClass(TFTeam team, TFClassType playerClass, int classIndex);
	void SpecSteamID(const CCommand& command);
	void SpecIndex(const CCommand& command);
	void SpecEntIndex(const CCommand& command);

	bool m_IsTaunting;
	void UpdateIsTaunting();

	void SpecPlayer(int playerIndex);

	void OnTick(bool inGame) override;

	std::optional<VariablePusher<Vector>> m_OldViewHeight;
	std::optional<VariablePusher<Vector>> m_OldDuckViewHeight;
	static EntityOffset<float> s_ViewOffsetZOffset;
	bool FixViewHeights();

	void ToggleDisableViewPunches(const ConVar* var);
	VariablePusher<RecvVarProxyFn> m_vecPunchAngleProxy;
	VariablePusher<RecvVarProxyFn> m_vecPunchAngleVelProxy;

	Vector CalcPosForAngle(const TPLockRuleset& ruleset, const Vector& orbitCenter, const QAngle& angle) const;

	QAngle m_LastFrameAngle;
	Player* m_LastTargetPlayer;

	bool PerformTPLock(const TPLockRuleset& ruleset, Vector& origin, QAngle& angles, float& fov);

	void AttachHooks(bool attach);

	ModeSwitchReason m_SwitchReason;
};