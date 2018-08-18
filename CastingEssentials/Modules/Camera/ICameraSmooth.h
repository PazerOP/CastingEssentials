#pragma once

#include "Modules/Camera/ICamera.h"

#include <variant>

class ICamera;

class ICameraSmooth
{
public:
	virtual ~ICameraSmooth() = default;

	virtual CameraConstPtr GetStartCamera() const = 0;
	virtual CameraConstPtr GetEndCamera() const = 0;
	virtual CameraConstPtr GetSmoothedCamera() const = 0;

	CameraPtr GetStartCamera() { return std::const_pointer_cast<ICamera>(std::as_const(*this).GetStartCamera()); }
	CameraPtr GetEndCamera() { return std::const_pointer_cast<ICamera>(std::as_const(*this).GetEndCamera()); }

	virtual void Update(float dt) = 0;
	virtual float GetProgress() const = 0;
	virtual void Reset() = 0;

	// Helpers
	bool IsSmoothComplete() const { return GetProgress() >= 1; }
};