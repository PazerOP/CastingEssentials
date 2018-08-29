#pragma once

#include "Modules/Camera/ICamera.h"

#include <convar.h>
#include <shared/usercmd.h>

class IClientEntity;

class RoamingCamera final : public ICamera
{
public:
	RoamingCamera();

	void Reset() override;
	int GetAttachedEntity() const override { return 0; }
	const char* GetDebugName() const override { return "RoamingCamera"; }

	void SetPosition(const Vector& pos, const QAngle& angles);
	void CreateMove(const CUserCmd& cmd);

	void GotoEntity(IClientEntity* ent);

	// Should we be processing the last usercmd, or should we just let velocity decay?
	bool m_InputEnabled = true;

protected:
	bool IsCollapsible() const override { return false; }
	void Update(float dt, uint32_t frame) override;

private:
	void Accelerate(Vector& wishdir, float wishspeed, float accel, float dt);

	Vector m_Velocity;
	CUserCmd m_LastCmd;
};