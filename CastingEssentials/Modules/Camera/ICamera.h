#pragma once

class Vector;
class QAngle;

class ICamera;
using CameraPtr = std::shared_ptr<ICamera>;
using CameraConstPtr = std::shared_ptr<const ICamera>;

class ICamera
{
public:
	virtual ~ICamera() = default;

	virtual void Reset() = 0;
	virtual void Update(float dt) = 0;

	// If we are attached to an entity (say, with TPLock), then return its entindex here (otherwise return 0).
	// This helps us keep our camera system more in sync with the game's.
	virtual int GetAttachedEntity() const = 0;

	// Code readability
	void ApplySettings() { Update(0); }

	__forceinline const Vector& GetOrigin() const { return m_Origin; }
	__forceinline const QAngle& GetAngles() const { return m_Angles; }
	__forceinline float GetFOV() const { return m_FOV; }

	__forceinline bool IsFirstPerson() const { return m_IsFirstPerson; }

	static void TryCollapse(CameraPtr& ptr);

protected:
	virtual bool IsCollapsible() const = 0;
	virtual CameraPtr GetCollapsedCamera() { return nullptr; }

	Vector m_Origin;
	QAngle m_Angles;
	float m_FOV;
	bool m_IsFirstPerson;
};