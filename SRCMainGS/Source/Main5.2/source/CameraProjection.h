#pragma once

#include "ZzzOpenglUtil.h"

extern int ScreenCenterX;
extern int ScreenCenterY;

namespace CameraProjection
{
	// Projects to the physical-pixel coordinate space used by CGMFontLayer and
	// other world overlays. Unlike the legacy Projection2 helper, this rejects
	// points behind the camera and cannot divide by a zero depth.
	inline bool WorldToScreen(const vec3_t position, int* screenX, int* screenY)
	{
		if (screenX == NULL || screenY == NULL || PerspectiveX == 0.0f || PerspectiveY == 0.0f)
			return false;

		vec3_t transformedPosition;
		VectorTransform(position, CameraMatrix, transformedPosition);

		if (transformedPosition[2] >= -0.001f)
			return false;

		*screenX = ScreenCenterX - (int)(transformedPosition[0] / PerspectiveX / transformedPosition[2]);
		*screenY = ScreenCenterY + (int)(transformedPosition[1] / PerspectiveY / transformedPosition[2]);
		return true;
	}
}
