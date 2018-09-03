#pragma once
#include "PluginBase/EntityOffset.h"
#include "PluginBase/Hook.h"
#include "PluginBase/Modules.h"

#include "Modules/Camera/CameraStateCallbacks.h"

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

class CameraTools final : public Module<CameraTools>, CameraStateCallbacks
{
public:
	CameraTools();
	virtual ~CameraTools() = default;

	static bool CheckDependencies();
	static constexpr __forceinline const char* GetModuleName() { return "Camera Tools"; }

	void SpecPosition(const Vector& pos, const QAngle& angle, ObserverMode mode = OBS_MODE_FIXED, float fov = INFINITY);

	static float CollisionTest3D(const Vector& startPos, const Vector& targetPos, float scale,
	                             const IHandleEntity* ignoreEnt = nullptr);

	static void GetSmoothTestSettings(const std::string_view& testsString, SmoothSettings& settings);

protected:
	void SpecModeChanged(ObserverMode oldMode, ObserverMode& newMode) override;
	void SpecTargetChanged(IClientEntity* oldEnt, IClientEntity*& newEnt) override;

private:
	ConVar ce_cameratools_autodirector_mode;
	ConVar ce_cameratools_force_target;
	ConVar ce_cameratools_disable_view_punches;

	ConCommand ce_cameratools_spec_pos;
	ConCommand ce_cameratools_spec_pos_delta;
	ConCommand ce_cameratools_spec_class;
	ConCommand ce_cameratools_spec_steamid;
	ConCommand ce_cameratools_spec_index;
	ConCommand ce_cameratools_spec_entindex;

	ConCommand ce_cameratools_show_users;
	void ShowUsers(const CCommand& command);

	bool ParseSpecPosCommand(const CCommand& command, Vector& pos, QAngle& angle, ObserverMode& mode,
							 const Vector& defaultPos, const QAngle& defaultAng, ObserverMode defaultMode) const;
	void SpecPosition(const CCommand &command);
	void SpecPositionDelta(const CCommand& command);

	void SpecClass(const CCommand& command);
	void SpecClass(TFTeam team, TFClassType playerClass, int classIndex);
	void SpecSteamID(const CCommand& command);
	void SpecIndex(const CCommand& command);
	void SpecEntIndex(const CCommand& command);
	void SpecPlayer(int playerIndex);

	ConCommand ce_cameratools_smoothto;
	static void SmoothTo(const CCommand& cmd);

	void ToggleDisableViewPunches(const ConVar* var);
	VariablePusher<RecvVarProxyFn> m_vecPunchAngleProxy;
	VariablePusher<RecvVarProxyFn> m_vecPunchAngleVelProxy;
};
