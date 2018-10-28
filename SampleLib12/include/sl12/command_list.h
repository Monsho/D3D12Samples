#pragma once

#include <sl12/util.h>


namespace sl12
{
	class Device;
	class CommandQueue;
	class Texture;
	class Buffer;

	class CommandList
	{
	public:
		CommandList()
		{}
		~CommandList()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, CommandQueue* pQueue, bool forDxr = false);
		void Destroy();

		void Reset();

		void Close();

		void Execute();

		void TransitionBarrier(Texture* p, D3D12_RESOURCE_STATES nextState);
		void TransitionBarrier(Buffer* p, D3D12_RESOURCE_STATES nextState);

		void TransitionBarrier(Texture* p, D3D12_RESOURCE_STATES prevState, D3D12_RESOURCE_STATES nextState);
		void TransitionBarrier(Buffer* p, D3D12_RESOURCE_STATES prevState, D3D12_RESOURCE_STATES nextState);

		void UAVBarrier(Texture* p);
		void UAVBarrier(Buffer* p);

		// getter
		CommandQueue* GetParentQueue() { return pParentQueue_; }
		ID3D12CommandAllocator* GetCommandAllocator() { return pCmdAllocator_; }
		ID3D12GraphicsCommandList* GetCommandList() { return pCmdList_; };
		ID3D12GraphicsCommandList4* GetDxrCommandList() { return pDxrCmdList_; };

	private:
		CommandQueue*				pParentQueue_{ nullptr };
		ID3D12CommandAllocator*		pCmdAllocator_{ nullptr };
		ID3D12GraphicsCommandList*	pCmdList_{ nullptr };
		ID3D12GraphicsCommandList4*	pDxrCmdList_{ nullptr };
	};	// class CommandList

}	// namespace sl12

//	EOF
