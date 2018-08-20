#pragma once
#include "Modules/Camera/ICamera.h"

#include <shared/ehandle.h>

class C_BaseEntity;

class FirstPersonCamera final : public ICamera
{
public:
	FirstPersonCamera(CHandle<C_BaseEntity> entity);

	void Reset() override {}
	int GetAttachedEntity() const override { return m_Entity.GetEntryIndex(); }
	const char* GetDebugName() const override { return "FirstPersonCamera"; }

protected:
	bool IsCollapsible() const override { return false; }
	void Update(float dt, uint32_t frame) override;

private:
	CHandle<C_BaseEntity> m_Entity;
};