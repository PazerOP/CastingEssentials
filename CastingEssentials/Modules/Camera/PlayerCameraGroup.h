#pragma once
#include "Modules/Camera/DeathCamera.h"
#include "Modules/Camera/FirstPersonCamera.h"
#include "Modules/Camera/ICameraGroup.h"
#include "Modules/Camera/TPLockCamera.h"

#include <igameevents.h>

#include <optional>

class PlayerCameraGroup final : public ICameraGroup, IGameEventListener2, public std::enable_shared_from_this<PlayerCameraGroup>
{
public:
	PlayerCameraGroup(Player& player);
	~PlayerCameraGroup();

	void GetBestCamera(CameraPtr& targetCamera) override;

	IClientEntity* GetPlayerEnt() const { return m_PlayerEnt.Get(); }

protected:
	void FireGameEvent(IGameEvent* event) override;

private:
	FirstPersonCamera m_FirstPersonCam;
	TPLockCamera m_TPLockCam;
	TPLockCamera m_TPLockTauntCam;
	DeathCamera m_DeathCam;

	float m_DeathCameraCreateTime;

	CHandle<IClientEntity> m_PlayerEnt;
};
