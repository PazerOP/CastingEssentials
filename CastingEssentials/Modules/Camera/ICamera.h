#pragma once

class Vector;
class QAngle;

class ICamera
{
public:
	virtual ~ICamera() = default;

	virtual void Reset() = 0;
	virtual void Update(float dt) = 0;

	__forceinline const Vector& GetOrigin() const { return m_Origin; }
	__forceinline const QAngle& GetAngles() const { return m_Angles; }
	__forceinline float GetFOV() const { return m_FOV; }

	__forceinline bool IsFirstPerson() const { return m_IsFirstPerson; }

protected:
	Vector m_Origin;
	QAngle m_Angles;
	float m_FOV;
	bool m_IsFirstPerson;
};

using CameraPtr = std::shared_ptr<ICamera>;
using CameraConstPtr = std::shared_ptr<const ICamera>;