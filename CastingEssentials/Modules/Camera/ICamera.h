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
	void TryUpdate(float dt, uint32_t frame)
	{
		if (dt == 0 || frame != m_LastFrameUpdate)
		{
			m_LastFrameUpdate = frame;
			Update(dt, frame);
		}
	}

	// If we are attached to an entity (say, with TPLock), then return its entindex here (otherwise return 0).
	// This helps us keep our camera system more in sync with the game's.
	virtual int GetAttachedEntity() const = 0;

	virtual const char* GetDebugName() const = 0;

	// Code readability
	void ApplySettings() { Update(0, 0); }

	__forceinline const Vector& GetOrigin() const { return m_Origin; }
	__forceinline const QAngle& GetAngles() const { return m_Angles; }
	__forceinline float GetFOV() const { return m_FOV; }

	__forceinline bool IsFirstPerson() const { return m_IsFirstPerson; }

	static bool TryCollapse(CameraPtr& ptr);

protected:
	virtual bool IsCollapsible() const = 0;
	virtual CameraPtr GetCollapsedCamera() { return nullptr; }
	virtual void Update(float dt, uint32_t frame) = 0;

	Vector m_Origin;
	QAngle m_Angles;
	float m_FOV;
	bool m_IsFirstPerson;

private:
	uint32_t m_LastFrameUpdate = 0;
};