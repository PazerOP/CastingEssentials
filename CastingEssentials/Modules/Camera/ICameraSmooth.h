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

protected:
	// ICamera
	CameraPtr GetCollapsedCamera() override { return IsSmoothComplete() ? GetEndCamera() : nullptr; }
};