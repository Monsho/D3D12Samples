#pragma once

#define NOMINMAX

#include <stdio.h>
#include "types.h"
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <string>


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

	class HashString
	{
	public:
		HashString()
		{}
		HashString(const char* str)
			: str_(str)
		{
			hash_ = CalcFnv1a32(str_.c_str(), str_.size());
		}

		bool operator==(const HashString& rhs) const
		{
			if (hash_ != rhs.hash_) return false;
			return str_ == rhs.str_;
		}
		bool operator<(const HashString& rhs) const
		{
			if (hash_ != rhs.hash_) return hash_ < rhs.hash_;
			return str_ < rhs.str_;
		}
		bool operator>(const HashString& rhs) const
		{
			if (hash_ != rhs.hash_) return hash_ > rhs.hash_;
			return str_ > rhs.str_;
		}

		const std::string& GetString() const
		{
			return str_;
		}
		u32 GetHash() const
		{
			return hash_;
		}

	private:
		std::string	str_;
		u32			hash_ = 0;
	};	// class HashString

	class HashWString
	{
	public:
		HashWString()
		{}
		HashWString(const wchar_t* str)
			: str_(str)
		{
			hash_ = CalcFnv1a32(str_.c_str(), str_.size());
		}

		bool operator==(const HashWString& rhs) const
		{
			if (hash_ != rhs.hash_) return false;
			return str_ == rhs.str_;
		}
		bool operator<(const HashWString& rhs) const
		{
			if (hash_ != rhs.hash_) return hash_ < rhs.hash_;
			return str_ < rhs.str_;
		}
		bool operator>(const HashWString& rhs) const
		{
			if (hash_ != rhs.hash_) return hash_ > rhs.hash_;
			return str_ > rhs.str_;
		}

		const std::wstring& GetString() const
		{
			return str_;
		}
		u32 GetHash() const
		{
			return hash_;
		}

	private:
		std::wstring	str_;
		u32				hash_ = 0;
	};	// class HashWString

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
		LARGE_INTEGER			time_ = {0};
	};	// class CpuTimer

	class Random
	{
	public:
		Random()
		{}
		Random(u32 seed)
		{
			x_ = seed = 1812433253 * (seed ^ (seed >> 30));
			y_ = seed = 1812433253 * (seed ^ (seed >> 30)) + 1;
			z_ = seed = 1812433253 * (seed ^ (seed >> 30)) + 2;
			w_ = seed = 1812433253 * (seed ^ (seed >> 30)) + 3;
		}

		u32 GetValue()
		{
			u32 t = x_ ^ (x_ << 11);
			x_ = y_;
			y_ = z_;
			z_ = w_;
			return w_ = (w_ ^ (w_ >> 19)) ^ (t ^ (t >> 8));
		}

		float GetFValue()
		{
			return (float)GetValue() / (float)UINT_MAX;
		}

		float GetFValueRange(float minV, float maxV)
		{
			return minV + (maxV - minV) * GetFValue();
		}

	private:
		u32		x_ = 123456789;
		u32		y_ = 362436069;
		u32		z_ = 521288629;
		u32		w_ = 88675123;
	};	// class Random

	extern Random GlobalRandom;
}	// namespace sl12

//	EOF
