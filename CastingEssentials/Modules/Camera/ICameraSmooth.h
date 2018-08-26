#pragma once

#include "Modules/Camera/ICamera.h"

#include <variant>

class ICameraSmooth : public ICamera
{
public:
	virtual ~ICameraSmooth() = default;

	// ICamera
	int GetAttachedEntity() const override
	{
		if (GetProgress() < 0.5)
		{
			if (auto startCam = GetStartCamera())
				return startCam->GetAttachedEntity();
		}

		if (auto endCam = GetEndCamera())
			return endCam->GetAttachedEntity();

		return 0;
	}

	virtual CameraConstPtr GetStartCamera() const = 0;
	virtual CameraConstPtr GetEndCamera() const = 0;

	CameraPtr GetStartCamera() { return std::const_pointer_cast<ICamera>(std::as_const(*this).GetStartCamera()); }
	CameraPtr GetEndCamera() { return std::const_pointer_cast<ICamera>(std::as_const(*this).GetEndCamera()); }

	virtual float GetProgress() const = 0;

	// Helpers
	bool IsSmoothComplete() const { return GetProgress() >= 1; }

	// Gets/sets a camera to collapse to when this smooth is complete, INSTEAD OF the end camera.
	void SetQueuedCamera(const CameraPtr& camera) { m_QueuedCamera = camera; }
	const CameraPtr& GetQueuedCamera() const { return m_QueuedCamera; }

protected:
	// ICamera
	CameraPtr GetCollapsedCamera() override
	{
		if (IsSmoothComplete())
		{
			if (m_QueuedCamera)
				return m_QueuedCamera;
			else
				return GetEndCamera();
		}

		return nullptr;
	}

private:
	CameraPtr m_QueuedCamera;
};
