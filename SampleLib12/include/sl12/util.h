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
}	// namespace sl12

//	EOF
