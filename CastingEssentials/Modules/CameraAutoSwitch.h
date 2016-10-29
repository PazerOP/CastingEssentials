#pragma once

#include <igameevents.h>
#include "PluginBase/Modules.h"

#include <memory>

class CCommand;
class ConCommand;
class ConVar;
class IConVar;

class CameraAutoSwitch : public Module, IGameEventListener2
{
public:
	CameraAutoSwitch();
	virtual ~CameraAutoSwitch();

	static bool CheckDependencies();

	virtual void FireGameEvent(IGameEvent *event);
private:
	class Panel;
	std::unique_ptr<Panel> panel;

	ConVar *enabled;
	ConVar *killer;
	ConVar *killer_delay;
	void ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue);
	void ToggleKillerEnabled(IConVar *var, const char *pOldValue, float flOldValue);
};
