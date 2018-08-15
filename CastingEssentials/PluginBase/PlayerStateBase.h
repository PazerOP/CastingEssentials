#pragma once

class Player;

class PlayerStateBase
{
public:
	constexpr PlayerStateBase(Player& player) : m_Player(player) {}
	PlayerStateBase(const PlayerStateBase& other) = delete;
	PlayerStateBase& operator=(const PlayerStateBase& other) = delete;

	virtual ~PlayerStateBase() = default;

	void Update();

protected:
	virtual void UpdateInternal(bool tickUpdate, bool frameUpdate) {}

	Player& GetPlayer() { return m_Player; }
	const Player& GetPlayer() const { return m_Player; }

private:
	int m_LastTickUpdate = -1;
	int m_LastFrameUpdate = -1;

	Player& m_Player;
};