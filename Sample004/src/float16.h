#pragma once

#include <algorithm>

typedef unsigned short	float16;

float16 ToFloat16(float f)
{
	unsigned long fbit = *reinterpret_cast<unsigned int*>(&f);
	if (fbit == 0x0)
	{
		return 0x0;
	}
	else if (fbit == 0x80000000)
	{
		return 0x8000;
	}

	float16 sign = static_cast<float16>((fbit & 0x80000000) >> 16);
	float16 mant = static_cast<float16>((fbit & 0x007fffff) >> 13);
	int e = static_cast<int>((fbit & 0x7f800000) >> 23);
	e = e - 127 + 15;
	std::max<int>(0, std::min<int>(e, 31));

	return sign | static_cast<float16>(e << 10) | mant;
}

float ToFloat32(float16 f)
{
	typedef unsigned int	u32;
	u32 sign = static_cast<u32>(f & 0x8000) << 16;
	u32 mant = static_cast<u32>(f & 0x03ff) << 13;
	int e = static_cast<int>(f & 0x7c00 >> 10);
	e = e - 15 + 127;
	u32 result = sign | static_cast<u32>(e << 23) | mant;
	return *reinterpret_cast<float*>(&result);
}

//	EOF
