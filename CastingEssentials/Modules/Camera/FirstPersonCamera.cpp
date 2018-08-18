#include "Modules/Camera/FirstPersonCamera.h"

#include <client/c_baseentity.h>
#include <client/c_baseplayer.h>

FirstPersonCamera::FirstPersonCamera(CHandle<C_BaseEntity> entity) :
	m_Entity(entity)
{
	m_IsFirstPerson = true;
}

void FirstPersonCamera::Update(float dt)
{
	if (auto ent = m_Entity.Get())
	{
		m_Origin = ent->EyePosition();
		m_Angles = ent->EyeAngles();
		m_FOV = 90;
	}
	else
	{
		m_Origin = vec3_origin;
		m_Angles = vec3_angle;
		m_FOV = 150;
	}
}
