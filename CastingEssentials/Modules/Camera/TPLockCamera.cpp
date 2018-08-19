#include "Modules/Camera/TPLockCamera.h"

#include "Misc/HLTVCameraHack.h"

#include <client/c_baseentity.h>
#include <client/c_baseplayer.h>
#include <shared/util_shared.h>

TPLockCamera::TPLockCamera(CHandle<C_BaseEntity> entity) :
	m_Entity(entity)
{
	m_IsFirstPerson = false;
}

void TPLockCamera::Update(float dt)
{
	C_BaseEntity* const ent = m_Entity.Get();
	if (!ent)
		return;

	Vector targetPos;
	bool fallbackPos = true;
	C_BaseAnimating* const baseAnimating = ent->GetBaseAnimating();
	if (baseAnimating)
	{
		const int targetBone = baseAnimating->LookupBone(m_Bone.c_str());
		if (targetBone < 0)
		{
			DevWarning(2, "[Third person lock] Unable to find bone \"%s\"! Reverting to eye position.\n", m_Bone.c_str());
		}
		else
		{
			QAngle dummy;
			baseAnimating->GetBonePosition(targetBone, targetPos, dummy);
			fallbackPos = false;
		}
	}

	if (fallbackPos)
		targetPos = ent->EyePosition();

	QAngle idealAngles = ent->EyeAngles();
	for (uint_fast8_t i = 0; i < 3; i++)
	{
		idealAngles[i] = AngleNormalize(m_AngOffset[i].GetValue(idealAngles[i]));

		if (m_DPS[i] >= 0)
			idealAngles[i] = ApproachAngle(idealAngles[i], m_Angles[i], m_DPS[i] * dt);
	}

	const Vector idealPos = CalcPosForAngle(targetPos, idealAngles);

	m_Angles = idealAngles;
	m_Origin = idealPos;
}

Vector TPLockCamera::CalcPosForAngle(const Vector& orbitCenter, const QAngle& angle) const
{
	Vector forward, right, up;
	AngleVectors(angle, &forward, &right, &up);

	Vector idealPos = orbitCenter + forward * m_PosOffset[1];
	idealPos += right * m_PosOffset[0];
	idealPos += up * m_PosOffset[2];

	const Vector camDir = (idealPos - orbitCenter).Normalized();
	const float dist = orbitCenter.DistTo(idealPos);

	// clip against walls
	trace_t trace;

	CTraceFilterNoNPCsOrPlayer noPlayers(nullptr, COLLISION_GROUP_NONE);
	UTIL_TraceHull(orbitCenter, idealPos, WALL_MIN, WALL_MAX, MASK_OPAQUE | MASK_SOLID, &noPlayers, &trace);

	const float wallDist = (trace.endpos - orbitCenter).Length();

	return orbitCenter + camDir * wallDist;;
}

float TPLockCamera::TPLockValue::GetValue(float input) const
{
	switch (m_Mode)
	{
		case Mode::Set:
			return m_Value;
		case Mode::Add:
			return input + m_Value;
		case Mode::Scale:
			return input * m_Value;
		case Mode::ScaleAdd:
			return input * m_Value + m_Base;
	}

	Assert(!"Should never get here...");
	return NAN;
}
