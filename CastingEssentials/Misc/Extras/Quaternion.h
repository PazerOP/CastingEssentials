#pragma once

#include <mathlib/vector.h>

inline Quaternion operator+(const Quaternion& p, const Quaternion& q)
{
	Quaternion retVal;
	QuaternionAdd(p, q, retVal);
	return retVal;
}
inline Quaternion operator-(const Quaternion& p)
{
	return Quaternion(-p.x, -p.y, -p.z, -p.w);
}
inline Quaternion& operator+=(Quaternion& p, const Quaternion& q)
{
	QuaternionAdd(p, q, p);
	return p;
}
inline Quaternion operator*(const Quaternion& a, const Quaternion& b)
{
	Quaternion temp;
	QuaternionMult(a, b, temp);
	return temp;
}
inline Quaternion operator*(const Quaternion& a, float scalar)
{
	Quaternion temp;
	QuaternionScale(a, scalar, temp);
	return temp;
}
inline Quaternion& operator*=(Quaternion& a, const Quaternion& b)
{
	QuaternionMult(Quaternion(a), b, a);
	return a;
}
inline Quaternion& operator*=(Quaternion& p, float scalar)
{
	QuaternionScale(p, scalar, p);
	return p;
}

inline Quaternion QuaternionAdd(const Quaternion& a, const Quaternion& b)
{
	Quaternion temp;
	QuaternionAdd(a, b, temp);
	return temp;
}
inline QAngle QuaternionAngles(const Quaternion& q)
{
	QAngle temp;
	QuaternionAngles(q, temp);
	return temp;
}
inline Quaternion QuaternionInvert(const Quaternion& a)
{
	Quaternion temp;
	QuaternionInvert(a, temp);
	return temp;
}
