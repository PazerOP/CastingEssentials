#pragma once
#include "Modules/Camera/ICamera.h"

#include <shared/ehandle.h>

#include <vector>

class C_BaseEntity;
class IClientEntity;
enum class TFClassType;

class DeathCamera : public ICamera
{
public:
	DeathCamera();

	void Reset() override;
	int GetAttachedEntity() const override { return 0; }
	const char* GetDebugName() const override { return "DeathCamera"; }

	CHandle<IClientEntity> m_Victim;
	CHandle<IClientEntity> m_Killer;

protected:
	bool IsCollapsible() const override { return false; }
	void Update(float dt, uint32_t frame) override;

private:
	float m_ElapsedTime = 0;
	TFClassType m_VictimClass;

	void FindProjectiles();
	std::vector<std::pair<Vector, CHandle<C_BaseEntity>>> m_Projectiles;

	bool m_ShouldUpdateKillerPosition = true;
	Vector m_KillerPosition;
	void UpdateKillerPosition();

	QAngle SmoothCameraAngle(const QAngle& targetAngle, float rate) const;
	QAngle SmoothCameraAngle(const Vector& targetPos, float rate) const;
};
