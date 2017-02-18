#pragma once

#include <sl12/util.h>


namespace sl12
{
	class Device;
	class CommandQueue;

	class Fence
	{
	public:
		Fence()
		{}
		~Fence()
		{
			Destroy();
		}

		bool Initialize(Device* pDev);
		void Destroy();

		void Signal(CommandQueue* pQueue);
		void Signal(CommandQueue* pQueue, u32 value);

		void WaitSignal();
		void WaitSignal(u32 value);
		void WaitSignal(CommandQueue* pQueue);

		bool CheckSignal();

	private:
		ID3D12Fence*	pFence_{ nullptr };
		HANDLE			hEvent_{ nullptr };
		u32				value_{ 0 };
		u32				waitValue_{ 0 };
	};	// class Fence

}	// namespace sl12

//	EOF
