#pragma once

#include <shared/shareddefs.h>

#include <optional>

template<typename T> struct ValueDelta
{
	T oldVal;
	std::optional<T> newVal;
	bool changed;

	T GetLatestValue() const { return newVal.value_or(oldVal); }

	void ArchiveNewValue()
	{
		if (newVal.has_value())
			oldVal = std::move(newVal.value());

		changed = false;
	}
};

struct CameraStateDelta
{
	ValueDelta<ObserverMode> m_Mode;
	ValueDelta<int> m_Target1;
	ValueDelta<float> m_FOV;

	void ArchiveNewValues()
	{
		m_Mode.ArchiveNewValue();
		m_Target1.ArchiveNewValue();
		m_FOV.ArchiveNewValue();
	}
};