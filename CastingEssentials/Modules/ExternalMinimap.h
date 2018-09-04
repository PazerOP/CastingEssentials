#pragma once

#include "PluginBase/Modules.h"

#include <convar.h>
#include <igamesystem.h>
#include <shared/shareddefs.h>

class ExternalMinimap final : public Module<ExternalMinimap>, CAutoGameSystemPerFrame
{
public:
	ExternalMinimap();
	~ExternalMinimap();
	static constexpr __forceinline const char* GetModuleName() { return "External Minimap"; }

	static bool CheckDependencies();

protected:
	void LevelInit() override;
	void Update(float frametime) override;
	void PostRender() override;
	void LevelShutdown() override;

private:
	ConVar ce_minimap_enabled;

	ConCommand ce_minimap_capture;
	bool m_CaptureQueued = false;
	void QueueCapture(const CCommand& cmd);
	void RunCapture();

#pragma pack(push, 1)
	struct MinimapData
	{
		struct Player
		{
			char m_Name[32];

			Vector m_Position;
			QAngle m_Angles;

			uint8_t m_Class;
			uint8_t m_Team;

			uint16_t m_Health;
			uint16_t m_MaxHealth;
			uint16_t m_MaxOverheal;
		};

		char m_MapName[260];

		uint8_t m_PlayerCount;
		Player m_Players[MAX_PLAYERS];
	};
#pragma pack(pop)

	static constexpr auto size = sizeof(MinimapData);
	static constexpr auto test = offsetof(MinimapData, m_Players);

	MinimapData* m_Data = nullptr;
	void* m_SharedMemHandle = nullptr;
};
