#pragma once
#include "Modules/Camera/ICamera.h"

#include <shared/ehandle.h>

#include <array>

class C_BaseEntity;

class TPLockCamera final : public ICamera
{
public:
	TPLockCamera(CHandle<C_BaseEntity> entity);

	void Reset() override {}
	int GetAttachedEntity() const override { return m_Entity.GetEntryIndex(); }

	const char* GetDebugName() const override { return "TPLockCamera"; }

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

	std::string m_Bone;
	Vector m_PosOffset;
	std::array<TPLockValue, 3> m_AngOffset;
	std::array<float, 3> m_DPS;
	using ICamera::m_FOV;

	CHandle<C_BaseEntity> m_Entity;

protected:
	// Based off of player position, never collapse
	bool IsCollapsible() const override { return false; }
	void Update(float dt, uint32_t frame) override;

private:
	Vector CalcPosForAngle(const Vector& orbitCenter, const QAngle& angle) const;
};