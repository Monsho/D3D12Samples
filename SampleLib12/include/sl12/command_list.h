#pragma once

#include <sl12/util.h>


namespace sl12
{
	class Device;
	class CommandQueue;

	class CommandList
	{
	public:
		CommandList()
		{}
		~CommandList()
		{}

		bool Initialize(Device* pDev, CommandQueue* pQueue);
		void Destroy();

		void Reset();

		void Close();

		void Execute();

		// getter
		ID3D12CommandAllocator* GetCommandAllocator() { return pCmdAllocator_; }
		ID3D12GraphicsCommandList* GetCommandList() { return pCmdList_; };

	private:
		CommandQueue*				pParentQueue_{ nullptr };
		ID3D12CommandAllocator*		pCmdAllocator_{ nullptr };
		ID3D12GraphicsCommandList*	pCmdList_{ nullptr };
	};	// class CommandList

}	// namespace sl12

//	EOF
