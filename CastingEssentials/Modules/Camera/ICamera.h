#pragma once

class Vector;
class QAngle;

class ICamera;
using CameraPtr = std::shared_ptr<ICamera>;
using CameraConstPtr = std::shared_ptr<const ICamera>;

enum class CameraType
{
	Invalid = -1,

	FirstPerson,
	ThirdPerson,
	Roaming,

	// This doesn't mean that we're not moving, it just means the observer has no control
	Fixed,

	// Don't touch spec_mode upon changing to a camera with this type, other than making sure
	// we're not in spec_mode 4 (firstperson) which might cause some stuff not to draw
	Smooth,
};

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

	__forceinline CameraType GetCameraType() const { return m_Type; }

	static bool TryCollapse(CameraPtr& ptr);
	virtual bool IsCollapsible() const = 0;

protected:
	virtual CameraPtr GetCollapsedCamera() { return nullptr; }
	virtual void Update(float dt, uint32_t frame) = 0;

	Vector m_Origin;
	QAngle m_Angles;
	float m_FOV;
	CameraType m_Type;

private:
	uint32_t m_LastFrameUpdate = 0;
};
