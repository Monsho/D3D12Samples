#pragma once

#define NOMINMAX

#include <stdio.h>
#include "types.h"
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>


namespace sl12
{
	template <typename T>
	void SafeRelease(T& p)
	{
		if (p != nullptr)
		{
			p->Release();
			p = nullptr;
		}
	}

	template <typename T>
	void SafeDelete(T& p)
	{
		if (p != nullptr)
		{
			delete p;
			p = nullptr;
		}
	}

	template <typename T>
	void SafeDeleteArray(T& p)
	{
		if (p != nullptr)
		{
			delete[] p;
			p = nullptr;
		}
	}

	inline void ConsolePrint(const char* format, ...)
	{
		va_list arg;

		char tsv[4096];
		va_start(arg, format);
		vsprintf_s(tsv, format, arg);
		va_end(arg);

		OutputDebugStringA(tsv);
	}

	static const u32 kFnv1aPrime32 = 16777619;
	static const u32 kFnv1aSeed32 = 0x811c9dc5;
	static const u64 kFnv1aPrime64 = 1099511628211L;
	static const u64 kFnv1aSeed64 = 0xcbf29ce484222325;

	inline u32 CalcFnv1a32(u8 oneByte, u32 hash = kFnv1aSeed32)
	{
		return (oneByte ^ hash) * kFnv1aPrime32;
	}
	inline u32 CalcFnv1a32(const void* data, size_t numBytes, u32 hash = kFnv1aSeed32)
	{
		assert(data);
		const u8* ptr = reinterpret_cast<const u8*>(data);
		while (numBytes--)
			hash = CalcFnv1a32(*ptr++, hash);
		return hash;
	}
	inline u64 CalcFnv1a64(u8 oneByte, u64 hash = kFnv1aSeed64)
	{
		return (oneByte ^ hash) * kFnv1aPrime32;
	}
	inline u64 CalcFnv1a64(const void* data, size_t numBytes, u64 hash = kFnv1aSeed64)
	{
		assert(data);
		const u8* ptr = reinterpret_cast<const u8*>(data);
		while (numBytes--)
			hash = CalcFnv1a64(*ptr++, hash);
		return hash;
	}

	class CpuTimer
	{
	public:
		static void Initialize()
		{
			QueryPerformanceFrequency(&frequency_);
		}

		static CpuTimer CurrentTime()
		{
			CpuTimer r;
			QueryPerformanceCounter(&r.time_);
			return r;
		}

		CpuTimer& operator=(const CpuTimer& v)
		{
			time_ = v.time_;
			return *this;
		}

		CpuTimer& operator+=(const CpuTimer& v)
		{
			time_.QuadPart += v.time_.QuadPart;
			return *this;
		}

		CpuTimer& operator-=(const CpuTimer& v)
		{
			time_.QuadPart -= v.time_.QuadPart;
			return *this;
		}

		CpuTimer operator+(const CpuTimer& v) const
		{
			CpuTimer r;
			r.time_.QuadPart = time_.QuadPart + v.time_.QuadPart;
			return r;
		}

		CpuTimer operator-(const CpuTimer& v) const
		{
			CpuTimer r;
			r.time_.QuadPart = time_.QuadPart - v.time_.QuadPart;
			return r;
		}

		float ToSecond() const
		{
			return (float)time_.QuadPart / (float)frequency_.QuadPart;
		}
		float ToMilliSecond() const
		{
			return (float)(time_.QuadPart * 1000) / (float)frequency_.QuadPart;
		}
		float ToMicroSecond() const
		{
			return (float)(time_.QuadPart * 1000 * 1000) / (float)frequency_.QuadPart;
		}
		float ToNanoSecond() const
		{
			return (float)(time_.QuadPart * 1000 * 1000 * 1000) / (float)frequency_.QuadPart;
		}

	private:
		static LARGE_INTEGER	frequency_;
		LARGE_INTEGER			time_;
	};	// class CpuTimer

}	// namespace sl12

//	EOF
