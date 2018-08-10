#pragma once

class Player;

class IPlayerState
{
public:
	constexpr IPlayerState(Player& player) : m_Player(player) {}
	IPlayerState(const IPlayerState& other) = delete;
	IPlayerState& operator=(const IPlayerState& other) = delete;

	virtual ~IPlayerState() = default;

	virtual void Update() {}

protected:
	Player& GetPlayer() { return m_Player; }
	const Player& GetPlayer() const { return m_Player; }

private:
	Player& m_Player;
};