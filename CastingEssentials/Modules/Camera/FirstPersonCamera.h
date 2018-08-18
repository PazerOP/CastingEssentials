#pragma once
#include "Modules/Camera/ICamera.h"

#include <shared/ehandle.h>

class C_BaseEntity;

class FirstPersonCamera final : public ICamera
{
public:
	FirstPersonCamera(CHandle<C_BaseEntity> entity);

	void Update(float dt) override;

private:
	CHandle<C_BaseEntity> m_Entity;
};