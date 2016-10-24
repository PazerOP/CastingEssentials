#pragma once

#include <engine/iserverplugin.h>

class Plugin : public IServerPluginCallbacks
{
public:
	virtual void Unload() override { }
	virtual void Pause() override { }
	virtual void UnPause() override { }
	virtual void LevelInit(const char* mapName) override { }
	virtual void ServerActivate(edict_t* edictList, int edictCount, int clientMax) override { }
	virtual void GameFrame(bool simulating) override { }
	virtual void LevelShutdown() override { }
	virtual void ClientActive(edict_t* entity) override { }
	virtual void ClientDisconnect(edict_t* entity) override { }
	virtual void ClientPutInServer(edict_t* entity, const char* playerName) override { }
	virtual void SetCommandClient(int index) override { }
	virtual void ClientSettingsChanged(edict_t* edict) override { }
	virtual PLUGIN_RESULT ClientConnect(bool* allowConnect, edict_t* entity, const char* name, const char* address, char* rejectMsg, int maxRejectMsgLength) override { return PLUGIN_CONTINUE; }
	virtual PLUGIN_RESULT ClientCommand(edict_t* entity, const CCommand& args) override { return PLUGIN_CONTINUE; }
	virtual PLUGIN_RESULT NetworkIDValidated(const char* userName, const char* networkID) override { return PLUGIN_CONTINUE; }
	virtual void OnQueryCvarValueFinished(QueryCvarCookie_t cookie, edict_t* playerEntity, EQueryCvarValueStatus status, const char* cvarName, const char* cvarValue) override { }
	virtual void OnEdictAllocated(edict_t* edict) override { }
	virtual void OnEdictFreed(const edict_t* edict) override { }
};