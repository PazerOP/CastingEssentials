#include "Misc/Interpolators.h"
#include "Modules/Camera/HybridPlayerCameraSmooth.h"

#include <algorithm>

#undef min

HybridPlayerCameraSmooth::HybridPlayerCameraSmooth(CameraPtr&& startCamera, CameraPtr&& endCamera) :
	m_StartCamera(std::move(startCamera)), m_EndCamera(std::move(endCamera))
{
}

void HybridPlayerCameraSmooth::Update(float dt, uint32_t frame)
{
	m_StartCamera->TryUpdate(dt, frame);
	TryCollapse(m_StartCamera);

	m_EndCamera->TryUpdate(dt, frame);
	TryCollapse(m_EndCamera);

	if (!IsSmoothComplete())
	{
		if (m_ElapsedTime == 0)
		{
			m_Angles = m_StartCamera->GetAngles();
			m_FOV = m_StartCamera->GetFOV();
			m_Origin = m_StartCamera->GetOrigin();
			m_IsFirstPerson = m_StartCamera->IsFirstPerson();

			if (dt == 0)
				return;
		}

		Assert(dt >= 0);
		m_ElapsedTime += dt;
	}
	else
	{
		m_Angles = m_EndCamera->GetAngles();
		m_FOV = m_EndCamera->GetFOV();
		m_Origin = m_EndCamera->GetOrigin();
		m_IsFirstPerson = m_EndCamera->IsFirstPerson();
	}

	const Vector targetPos = m_EndCamera->GetOrigin();
	const float distToTarget = m_Origin.DistTo(targetPos);

	float percent;
	const float targetDist = ComputeSmooth(m_ElapsedTime, 0, m_StartDist, m_LinearSpeed, m_BezierDist, m_BezierDuration, percent);

	if (m_Percent >= 1)
	{
		m_Origin = m_EndCamera->GetOrigin();
		m_Angles = m_EndCamera->GetAngles();
		m_FOV = m_EndCamera->GetFOV();
		m_IsFirstPerson = m_EndCamera->IsFirstPerson();
	}
	else if (m_ElapsedTime > 0)
	{
		if (m_SmoothingMode == SmoothingMode::Approach)
			m_Origin = ApproachVector(targetPos, m_Origin, targetDist);
		else
			m_Origin = VectorLerp(m_StartCamera->GetOrigin(), targetPos, RemapVal(targetDist, m_StartDist, 0, 0, 1));

		// Angles
		{
			// Percentage this frame
			const float percentThisFrame = percent - m_Percent;

			const float adjustedPercentage = percentThisFrame / (1 - percent);

			// Angle percentage is determined by overall progress towards our goal position
			const float angPercentage = Interpolators::CircleEaseIn(percent, m_AngleBias);

			const float angPercentThisFrame = angPercentage - m_LastAngPercent;
			const float adjustedAngPercentage = angPercentThisFrame / (1 - angPercentage);

			const auto& targetAng = m_EndCamera->GetAngles();
			const float distx = AngleDistance(targetAng.x, m_Angles.x);
			const float disty = AngleDistance(targetAng.y, m_Angles.y);
			const float distz = AngleDistance(targetAng.z, m_Angles.z);

			m_Angles.x = ApproachAngle(targetAng.x, m_Angles.x, distx * adjustedAngPercentage);
			m_Angles.y = ApproachAngle(targetAng.y, m_Angles.y, disty * adjustedAngPercentage);
			m_Angles.z = ApproachAngle(targetAng.z, m_Angles.z, distz * adjustedAngPercentage);

			m_LastAngPercent = angPercentage;
		}

		m_Percent = percent;
		m_IsFirstPerson = false;
	}
}

void HybridPlayerCameraSmooth::Reset()
{
	m_Percent = 0;
	m_ElapsedTime = 0;
	m_LastAngPercent = 0;
}

// Returns the distance to target. See https://www.desmos.com/calculator/fspe3a4hd6
float HybridPlayerCameraSmooth::ComputeSmooth(float time, float startTime, float totalDist, float maxSpeed,
	float bezierDist, float bezierDuration, float& percent)
{
	// x = time in seconds
	// y = distance to target

	const float slope = maxSpeed;

	const float y2 = totalDist;
	const float x2 = y2 / slope;

	const float x3 = x2 + bezierDuration;
	const float y3 = y2;

	if (totalDist <= bezierDist)
	{
		constexpr float splitPoint = 0;

		constexpr float x1 = 0;
		constexpr float y1 = 0;

		percent = (time - startTime) / x3;

		return totalDist - Bezier(percent, y1, y2, y3);
	}
	else
	{
		const float splitPoint = 1 - (bezierDist / totalDist);

		const float x1 = splitPoint * (y3 / slope);
		const float y1 = splitPoint * totalDist;

		const float bezierBeginX = x1;
		//const float bezierBeginY = y1;

		const float t = time - startTime;
		percent = t / x3;

		if (t < bezierBeginX)
		{
			// Simple linear interp between 0 and y1
			const float lerp = Lerp<float>(t / bezierBeginX, 0, y1);
			return totalDist - lerp;
		}
		else
		{
			const float bezier = Bezier((t - bezierBeginX) / (x3 - bezierBeginX), y1, y2, y3);
			return totalDist - bezier;
		}
	}
}
