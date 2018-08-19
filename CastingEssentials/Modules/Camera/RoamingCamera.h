#pragma once

#include "Modules/Camera/ICamera.h"

#include <convar.h>
#include <shared/usercmd.h>

class RoamingCamera final : public ICamera
{
public:
	RoamingCamera();

	void Reset() override;
	void Update(float dt) override;
	int GetAttachedEntity() const override { return 0; }

	void SetPosition(const Vector& pos, const QAngle& angles);
	void CreateMove(const CUserCmd& cmd);

protected:
	bool IsCollapsible() const override { return false; }

private:
	void Accelerate(Vector& wishdir, float wishspeed, float accel, float dt);

	Vector m_Velocity;
	CUserCmd m_LastCmd;
};