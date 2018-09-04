#include "RoamingCamera.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"

#include <cdll_int.h>
#include <client/c_baseplayer.h>
#include <shared/in_buttons.h>

#include <algorithm>

static float GetSpecSpeed()
{
	static ConVarRef sv_specspeed("sv_specspeed");
	return sv_specspeed.GetFloat();
}
static float GetMaxSpeed()
{
	static ConVarRef sv_maxspeed("sv_maxspeed");
	return sv_maxspeed.GetFloat();
}
static float GetSpecAccelerate()
{
	static ConVarRef sv_specaccelerate("sv_specaccelerate");
	return sv_specaccelerate.GetFloat();
}
static float GetFriction()
{
	static ConVarRef sv_friction("sv_friction");
	return sv_friction.GetFloat();
}

ConVar ce_cam_roaming_fov("ce_cam_roaming_fov", "90", FCVAR_NONE, "Roaming camera FOV.");

RoamingCamera::RoamingCamera() : m_Velocity(0)
{
	m_Type = CameraType::Roaming;
	m_Origin.Init();
	m_FOV = ce_cam_roaming_fov.GetFloat();

	if (engine)
		engine->GetViewAngles(m_Angles);
	else
		m_Angles.Init();
}

void RoamingCamera::Reset()
{
	m_Velocity.Init();
}

void RoamingCamera::Update(float dt, uint32_t frame)
{
	float factor = GetSpecSpeed();
	float maxspeed = GetMaxSpeed() * factor;

	auto cmd = &m_LastCmd;

	Vector forward, right, up;
	AngleVectors(m_Angles, &forward, &right, &up);  // Determine movement angles

	if (cmd->buttons & IN_SPEED)
		factor /= 2.0f;

	// Copy movement amounts
	float fmove = cmd->forwardmove * factor;
	float smove = cmd->sidemove * factor;

	VectorNormalize(forward);  // Normalize remainder of vectors
	VectorNormalize(right);    //

	Vector wishvel(0, 0, 0);
	for (int i = 0; i < 3; i++)       // Determine x and y parts of velocity
		wishvel[i] = forward[i] * fmove + right[i] * smove;

	wishvel[2] += cmd->upmove * factor;

	Vector wishdir = wishvel;   // Determine magnitude of speed of move
	float wishspeed = VectorNormalize(wishdir);

	//
	// Clamp to server defined max speed
	//
	if (wishspeed > maxspeed)
	{
		wishvel *= maxspeed / wishspeed;
		wishspeed = maxspeed;
	}

	if (GetSpecAccelerate() > 0.0)
	{
		// Set move velocity
		Accelerate(wishdir, wishspeed, GetSpecAccelerate(), dt);

		float spd = VectorLength(m_Velocity);
		if (spd < 1.0f)
		{
			m_Velocity.Init();
		}
		else
		{
			// Bleed off some speed, but if we have less than the bleed
			//  threshold, bleed the threshold amount.
			float control = (spd < maxspeed / 4.0) ? maxspeed / 4.0 : spd;

			float friction = GetFriction();

			// Add the amount to the drop amount.
			float drop = control * friction * dt;

			// scale the velocity
			float newspeed = spd - drop;
			if (newspeed < 0)
				newspeed = 0;

			// Determine proportion of old speed we are using.
			newspeed /= spd;
			m_Velocity *= newspeed;
		}
	}
	else
	{
		m_Velocity = wishvel;
	}

	// Just move ( don't clip or anything )
	m_Origin += m_Velocity * dt;

	// get camera angle directly from engine
	Assert(engine);
	//if (auto clientdll = Interfaces::GetClientDLL(); clientdll && m_InputEnabled && engine)
	{
		//m_Angles = cmd->viewangles;
	}

	// Zero out velocity if in noaccel mode
	if (GetSpecAccelerate() < 0.0f)
		m_Velocity.Init();
}

void RoamingCamera::SetPosition(const Vector& pos, const QAngle& angles)
{
	m_Origin = pos;

	m_Angles = angles;

	QAngle copy(m_Angles);
	engine->SetViewAngles(copy);

	m_Velocity.Init();
}

void RoamingCamera::CreateMove(const CUserCmd& cmd)
{
	if (!m_InputEnabled)
		return;

	m_LastCmd = cmd;

	static ConVarRef m_yaw("m_yaw");
	static ConVarRef m_pitch("m_pitch");
	static ConVarRef cl_pitchup("cl_pitchup");
	static ConVarRef cl_pitchdown("cl_pitchdown");

	static ConVarRef cl_leveloverview("cl_leveloverview");

	if (cl_leveloverview.GetFloat() <= 0)
	{
		m_Angles[PITCH] = clamp(m_Angles.x + (m_pitch.GetFloat() * cmd.mousedy), -cl_pitchup.GetFloat(), cl_pitchdown.GetFloat());
		m_Angles[YAW] -= m_yaw.GetFloat() * cmd.mousedx;
	}
	else
	{
		m_Angles[PITCH] = 0;
		m_Angles[YAW] = 90;
	}
}

void RoamingCamera::GotoEntity(IClientEntity* ent)
{
	if (!ent)
		return;

	if (auto baseEnt = ent->GetBaseEntity())
		SetPosition(baseEnt->EyePosition(), baseEnt->EyeAngles());
	else
		SetPosition(ent->GetAbsOrigin(), ent->GetAbsAngles());
}

void RoamingCamera::SetInputEnabled(bool enabled)
{
	if (!enabled && m_InputEnabled)
		m_LastCmd.MakeInert();

	m_InputEnabled = enabled;
}

void RoamingCamera::Accelerate(Vector& wishdir, float wishspeed, float accel, float dt)
{
	float addspeed, accelspeed, currentspeed;

	// See if we are changing direction a bit
	currentspeed = m_Velocity.Dot(wishdir);

	// Reduce wishspeed by the amount of veer.
	addspeed = wishspeed - currentspeed;

	// If not going to add any speed, done.
	if (addspeed <= 0)
		return;

	// Determine amount of acceleration.
	accelspeed = accel * dt * wishspeed;

	// Cap at addspeed
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	// Adjust velocity.
	for (int i = 0; i < 3; i++)
		m_Velocity[i] += accelspeed * wishdir[i];
}
