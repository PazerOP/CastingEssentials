#include "Modules/Camera/DeathCamera.h"
#include "PluginBase/Entities.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"

#include <client/c_baseplayer.h>

static bool s_TypeCheckersInit = false;
static EntityOffset<TFGrenadePipebombType> s_GrenadeTypeOffset;
static const ClientClass* s_RocketClass;

static ConVar ce_cam_death_anglerate("ce_cam_death_anglesmooth_rate", "0.75", FCVAR_NONE, "Speed of smoothed angles for deathcam.");

DeathCamera::DeathCamera()
{
	m_Type = CameraType::Fixed;
	m_VictimClass = TFClassType::Unknown;

	if (!s_TypeCheckersInit)
	{
		s_GrenadeTypeOffset = Entities::GetEntityProp<TFGrenadePipebombType>("CTFGrenadePipebombProjectile", "m_iType");
		s_RocketClass = Entities::GetClientClass("CTFProjectile_Rocket");
		s_TypeCheckersInit = true;
	}
}

void DeathCamera::Reset()
{
	m_ElapsedTime = 0;
	m_Projectiles.clear();
	m_ShouldUpdateKillerPosition = true;
}

void DeathCamera::Update(float dt, uint32_t frame)
{
	auto victim = Player::GetPlayer(m_Victim, __FUNCSIG__);

	if (m_ElapsedTime == 0)
	{
		Assert(victim);
		if (!victim)
			return;

		m_Origin = victim->GetEyePosition();
		m_Angles = victim->GetEyeAngles();
		m_VictimClass = victim->GetClass();

		FindProjectiles();
	}

	if (dt > 0)
		m_ElapsedTime += dt;

	// 3/4 of the way there per second
	const float rate = ce_cam_death_anglerate.GetFloat() * dt;

	if (m_Projectiles.size()) // Track the averaged origin of the projectiles
	{
		Vector sumPos(0, 0, 0);
		for (auto& proj : m_Projectiles)
		{
			// Update projectile origins if we still can
			if (auto ent = proj.second.Get())
				proj.first = ent->GetAbsOrigin();

			sumPos += proj.first / m_Projectiles.size();
		}

		SmoothCameraAngle(sumPos, rate);
	}
	else if (m_Killer) // Smoothly track the killer
	{
		if (m_ShouldUpdateKillerPosition)
			UpdateKillerPosition();

		SmoothCameraAngle(m_KillerPosition, rate);
	}
	else // Just smoothly return to horizontal pitch/no roll at original view yaw
	{
		m_Angles = SmoothCameraAngle(QAngle(0, m_Angles[YAW], 0), rate);
	}
}

void DeathCamera::FindProjectiles()
{
	if (m_VictimClass != TFClassType::DemoMan && m_VictimClass != TFClassType::Soldier)
		return;

	auto entityList = Interfaces::GetClientEntityList();
	if (!entityList)
		return;

	const auto maxEntIndex = entityList->GetHighestEntityIndex();
	for (int i = 0; i < maxEntIndex; i++)
	{
		auto clientEnt = entityList->GetClientEntity(i);
		if (!clientEnt)
			continue;

		auto networkable = clientEnt->GetClientNetworkable();
		if (!networkable)
			continue;

		auto baseEnt = clientEnt->GetBaseEntity();
		if (!baseEnt)
			continue;

		if (m_VictimClass == TFClassType::Soldier)
		{
			if (networkable->GetClientClass() == s_RocketClass)
				m_Projectiles.push_back(std::make_pair(vec3_invalid, baseEnt));
		}
		else if (m_VictimClass == TFClassType::DemoMan)
		{
			if (auto type = s_GrenadeTypeOffset.TryGetValue(networkable); type && *type == TFGrenadePipebombType::Pill)
				m_Projectiles.push_back(std::make_pair(vec3_invalid, baseEnt));
		}
	}
}

void DeathCamera::UpdateKillerPosition()
{
	auto entityList = Interfaces::GetClientEntityList();
	if (!entityList)
		return;

	auto ent = entityList->GetClientEntity(m_Killer);
	if (!ent)
		return;

	auto baseEnt = ent->GetBaseEntity();
	if (!baseEnt)
		return;

	if (!baseEnt->IsAlive())
		m_ShouldUpdateKillerPosition = false;
	else
		m_KillerPosition = baseEnt->EyePosition();
}

QAngle DeathCamera::SmoothCameraAngle(const QAngle& targetAngle, float rate) const
{
	QAngle retVal;

	for (int i = 0; i < 3; i++)
		retVal[i] = ApproachAngle(targetAngle[i], m_Angles[i], rate * AngleDistance(targetAngle[i], m_Angles[i]));

	return retVal;
}

QAngle DeathCamera::SmoothCameraAngle(const Vector& targetPos, float rate) const
{
	auto delta = targetPos - m_Origin;

	QAngle angles;
	VectorAngles(delta, angles);

	return SmoothCameraAngle(angles, rate);
}
