#include "ICamera.h"

bool ICamera::TryCollapse(CameraPtr& ptr)
{
	if (ptr->IsCollapsible())
	{
		if (auto collapsed = ptr->GetCollapsedCamera())
		{
			ptr = collapsed;
			return true;
		}
	}

	return false;
}
