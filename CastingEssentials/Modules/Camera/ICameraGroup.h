#pragma once

#include "Modules/Camera/ICamera.h"

enum class BestCameraResult
{
	// Some general failure.
	Error,


};

class ICameraGroup
{
public:
	virtual ~ICameraGroup() = default;

	virtual void GetBestCamera(CameraPtr& targetCamera) = 0;
};

using CameraGroupPtr = std::shared_ptr<ICameraGroup>;
