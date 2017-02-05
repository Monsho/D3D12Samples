#pragma once

#include <sl12/util.h>
#include <array>


namespace sl12
{
	class CommandQueue;
	class Swapchain;
	class DescriptorHeap;

	class Device
	{
	public:
		Device()
		{}
		~Device()
		{
			Destroy();
		}

		bool Initialize(HWND hWnd, u32 screenWidth, u32 screenHeight, const std::array<u32, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES>& numDescs);
		void Destroy();

		void Present();

		void WaitDrawDone();

		// getter
		IDXGIFactory4* GetFactoryDep() { return pFactory_; }
		ID3D12Device* GetDeviceDep() { return pDevice_; }
		CommandQueue& GetGraphicsQueue() { return *pGraphicsQueue_; }
		CommandQueue& GetComputeQueue() { return *pComputeQueue_; }
		CommandQueue& GetCopyQueue() { return *pCopyQueue_; }
		DescriptorHeap& GetDescriptorHeap(u32 no);
		Swapchain&	  GetSwapchain() { return *pSwapchain_; }

	private:
		IDXGIFactory4*	pFactory_{ nullptr };
		IDXGIAdapter3*	pAdapter_{ nullptr };
		IDXGIOutput4*	pOutput_{ nullptr };
		ID3D12Device*	pDevice_{ nullptr };

		CommandQueue*	pGraphicsQueue_{ nullptr };
		CommandQueue*	pComputeQueue_{ nullptr };
		CommandQueue*	pCopyQueue_{ nullptr };

		DescriptorHeap*	pDescHeaps_{ nullptr };

		Swapchain*		pSwapchain_{ nullptr };

		ID3D12Fence*	pFence_{ nullptr };
		u32				fenceValue_{ 0 };
		HANDLE			fenceEvent_{ nullptr };
	};	// class Device

}	// namespace sl12

//	EOF
