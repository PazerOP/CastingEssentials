#include "RoamingCamera.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"

#include <cdll_int.h>
#include <client/c_baseplayer.h>
#include <shared/in_buttons.h>

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

RoamingCamera::RoamingCamera() : m_Velocity(0)
{
	m_IsFirstPerson = false;
	m_Origin.Init();
	m_Angles.Init();
	m_FOV = 90;
}

void RoamingCamera::Reset()
{
	m_Velocity.Init();
}

void RoamingCamera::Update(float dt)
{
	Vector wishvel;
	Vector forward, right, up;
	Vector wishdir;
	float wishspeed;
	float factor = GetSpecSpeed();
	float maxspeed = GetMaxSpeed() * factor;

	auto cmd = &m_LastCmd;

	AngleVectors(cmd->viewangles, &forward, &right, &up);  // Determine movement angles

	if (cmd->buttons & IN_SPEED)
		factor /= 2.0f;

	// Copy movement amounts
	float fmove = cmd->forwardmove * factor;
	float smove = cmd->sidemove * factor;

	VectorNormalize(forward);  // Normalize remainder of vectors
	VectorNormalize(right);    //

	for (int i = 0; i < 3; i++)       // Determine x and y parts of velocity
		wishvel[i] = forward[i] * fmove + right[i] * smove;
	wishvel[2] += cmd->upmove * factor;

	VectorCopy(wishvel, wishdir);   // Determine magnitude of speed of move
	wishspeed = VectorNormalize(wishdir);

	//
	// Clamp to server defined max speed
	//
	if (wishspeed > maxspeed)
	{
		VectorScale(wishvel, maxspeed / wishspeed, wishvel);
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
			VectorScale(m_Velocity, newspeed, m_Velocity);
		}
	}
	else
	{
		VectorCopy(wishvel, m_Velocity);
	}

	// Just move ( don't clip or anything )
	VectorMA(m_Origin, dt, m_Velocity, m_Origin);

	// get camera angle directly from engine
	Assert(engine);
	if (engine)
		engine->GetViewAngles(m_Angles);

	// Zero out velocity if in noaccel mode
	if (GetSpecAccelerate() < 0.0f)
		m_Velocity.Init();
}

void RoamingCamera::SetPosition(const Vector& pos, const QAngle& angles)
{
	m_Origin = pos;
	m_Angles = angles;
	m_Velocity.Init();
}

void RoamingCamera::CreateMove(const CUserCmd& cmd)
{
	m_LastCmd = cmd;
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
