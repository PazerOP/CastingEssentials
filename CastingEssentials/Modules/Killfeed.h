#pragma once
#include "PluginBase/Modules.h"

#include <client/hud_basedeathnotice.h>

class ConVarRef;

class Killfeed final : public Module<Killfeed>
{
public:
	Killfeed();
	virtual ~Killfeed() { }

private:
	ConVar* ce_killfeed_continuous_update;

	class DeathNoticePanelOverride : public CHudBaseDeathNotice
	{
	public:
		~DeathNoticePanelOverride() { }
		using CHudBaseDeathNotice::m_DeathNotices;
	};

	enum EDeathNoticeIconFormat
	{
		kDeathNoticeIcon_Standard,
		kDeathNoticeIcon_Inverted,			// used for display on lighter background when kill involved the local player
	};

	static DeathNoticePanelOverride* FindDeathNoticePanel();
	DeathNoticePanelOverride* m_DeathNoticePanel;

	static constexpr int DEATH_NOTICES_OFFSET = 448;
	CUtlVector<DeathNoticeItem>* m_DeathNotices;

	void OnTick(bool inGame) override;
};