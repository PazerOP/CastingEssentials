#include "Modules/Camera/DeathCamera.h"
#include "PluginBase/Entities.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"

#include <client/c_baseplayer.h>

static bool s_TypeCheckersInit = false;
static EntityOffset<TFGrenadePipebombType> s_GrenadeTypeOffset;
static EntityOffset<bool> s_GrenadeTouchedOffset;
static const ClientClass* s_RocketClass;

static ConVar ce_cam_death_anglesmooth_rate("ce_cam_death_anglesmooth_rate", "2", FCVAR_NONE, "Speed of smoothed angles for deathcam.");
static ConVar ce_cam_death_fov("ce_cam_death_fov", "90", FCVAR_NONE, "Deathcam fov.");
static ConVar ce_cam_death_debug("ce_cam_death_debug", "0");

static ConVar ce_cam_death_lookat_pills("ce_cam_death_lookat_pills", "1", FCVAR_NONE, "Look at alive, untouched demoman pipes with the deathcam?");
static ConVar ce_cam_death_lookat_rockets("ce_cam_death_lookat_rockets", "1", FCVAR_NONE, "Look at alive soldier rockets with the deathcam?");
static ConVar ce_cam_death_lookat_killer("ce_cam_death_lookat_killer", "1", FCVAR_NONE, "Look at the killer with the deathcam?");

DeathCamera::DeathCamera()
{
	m_Type = CameraType::Fixed;
	m_VictimClass = TFClassType::Unknown;
	m_FOV = ce_cam_death_fov.GetFloat();

	if (!s_TypeCheckersInit)
	{
		auto grenadeType = Entities::GetClientClass("CTFGrenadePipebombProjectile");

		Entities::GetEntityProp(s_GrenadeTypeOffset, grenadeType, "m_iType");
		Entities::GetEntityProp(s_GrenadeTouchedOffset, grenadeType, "m_bTouched");

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
	auto victim = Player::AsPlayer(m_Victim.Get());

	if (m_Victim && m_ElapsedTime == 0)
	{
		m_Origin = victim->GetEyePosition();
		m_Angles = victim->GetEyeAngles();
		m_VictimClass = victim->GetClass();

		FindProjectiles();
	}

	if (dt > 0)
		m_ElapsedTime += dt;

	// 3/4 of the way there per second
	const float rate = ce_cam_death_anglesmooth_rate.GetFloat() * dt;

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

		m_Angles = SmoothCameraAngle(sumPos, rate);
	}
	else if (m_Killer) // Smoothly track the killer
	{
		if (m_ShouldUpdateKillerPosition)
			UpdateKillerPosition();

		m_Angles = SmoothCameraAngle(m_KillerPosition, rate);
	}
	else // Just smoothly return to horizontal pitch/no roll at original view yaw
	{
		if (ce_cam_death_debug.GetBool())
		{
			if (!victim)
				Warning("Deathcam: No victim!\n");
			else
				Warning("Deathcam: No killer for %s!\n", victim->GetName());
		}

		m_Angles = SmoothCameraAngle(QAngle(0, m_Angles[YAW], 0), rate);
	}

	Assert(m_Origin.IsValid());
	Assert(m_Angles.IsValid());
	Assert(std::isfinite(m_FOV));
	Assert(!AlmostEqual(m_FOV, 0));
}

void DeathCamera::FindProjectiles()
{
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

		if (m_VictimClass == TFClassType::Soldier && ce_cam_death_lookat_rockets.GetBool())
		{
			if (networkable->GetClientClass() == s_RocketClass)
				m_Projectiles.push_back(std::make_pair(vec3_invalid, baseEnt));
		}
		else if (m_VictimClass == TFClassType::DemoMan && ce_cam_death_lookat_pills.GetBool())
		{
			if (auto type = s_GrenadeTypeOffset.TryGetValue(networkable); !type || *type != TFGrenadePipebombType::Pill)
				continue;

			if (auto touched = s_GrenadeTouchedOffset.TryGetValue(networkable); !touched || *touched)
				continue;

			m_Projectiles.push_back(std::make_pair(vec3_invalid, baseEnt));
		}
	}
}

void DeathCamera::UpdateKillerPosition()
{
	auto ent = m_Killer.Get();
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
