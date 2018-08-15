#include "PlayerStateBase.h"

#include "PluginBase/Interfaces.h"

#include <toolframework/ienginetool.h>

void PlayerStateBase::Update()
{
	auto tool = Interfaces::GetEngineTool();
	if (!tool)
		return;

	const auto tick = tool->ClientTick();
	const auto frame = tool->HostFrameCount();

	UpdateInternal(tick != m_LastTickUpdate, frame != m_LastFrameUpdate);

	m_LastTickUpdate = tick;
	m_LastFrameUpdate = frame;
}