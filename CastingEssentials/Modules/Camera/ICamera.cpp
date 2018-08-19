#include "ICamera.h"

void ICamera::TryCollapse(CameraPtr& ptr)
{
	if (ptr->IsCollapsible())
	{
		if (auto collapsed = ptr->GetCollapsedCamera())
			ptr = collapsed;
	}
}
