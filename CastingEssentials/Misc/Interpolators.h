#pragma once

namespace Interpolators
{
	__forceinline constexpr float Linear(float x)
	{
		return x;
	}
	__forceinline constexpr float Smoothstep(float x)
	{
		return 3*(x*x) - 2*(x*x*x);
	};
	__forceinline constexpr float Smootherstep(float x)
	{
		return 6*(x*x*x*x*x) - 15*(x*x*x*x) + 10*(x*x*x);
	}
	__forceinline float CircleEaseOut(float x, float bias = 0.5)
	{
		return 1 - std::pow(1 - std::pow(x, bias), 1 / bias);
	}
	__forceinline float CircleEaseIn(float x, float bias = 0.5)
	{
		return std::pow(1 - std::pow(1 - x, bias), 1 / bias);
	}
	__forceinline float EaseIn2(float x, float bias = 0.5)
	{
		return std::pow(x, 1 / bias);
	}
	__forceinline float EaseOut2(float x, float bias = 0.5)
	{
		return 1 - std::pow(-x + 1, 1 / bias);
	}
};