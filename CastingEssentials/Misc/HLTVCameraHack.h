#pragma once
#include <client/hltvcamera.h>

enum ObserverMode;

class HLTVCameraOverride : public C_HLTVCamera
{
public:
	ObserverMode GetMode() const { return (ObserverMode)m_nCameraMode; }
	using C_HLTVCamera::m_nCameraMode;
	using C_HLTVCamera::m_iCameraMan;
	using C_HLTVCamera::m_vCamOrigin;
	using C_HLTVCamera::m_aCamAngle;
	using C_HLTVCamera::m_iTraget1;
	using C_HLTVCamera::m_iTraget2;
	using C_HLTVCamera::m_flFOV;
	using C_HLTVCamera::m_flOffset;
	using C_HLTVCamera::m_flDistance;
	using C_HLTVCamera::m_flLastDistance;
	using C_HLTVCamera::m_flTheta;
	using C_HLTVCamera::m_flPhi;
	using C_HLTVCamera::m_flInertia;
	using C_HLTVCamera::m_flLastAngleUpdateTime;
	using C_HLTVCamera::m_bEntityPacketReceived;
	using C_HLTVCamera::m_nNumSpectators;
	using C_HLTVCamera::m_szTitleText;
	using C_HLTVCamera::m_LastCmd;
	using C_HLTVCamera::m_vecVelocity;

	using C_HLTVCamera::SetCameraAngle;
};

#undef WALL_OFFSET
static constexpr float WALL_OFFSET = 6;

static constexpr Vector WALL_MIN(-WALL_OFFSET, -WALL_OFFSET, -WALL_OFFSET);
static constexpr Vector WALL_MAX(WALL_OFFSET, WALL_OFFSET, WALL_OFFSET);
