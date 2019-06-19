#pragma once

#include <sl12/util.h>
#include <array>
#include <sl12/descriptor_heap.h>


namespace sl12
{
	class CommandQueue;
	class Swapchain;
	class DescriptorHeap;
	class DescriptorAllocator;
	class GlobalDescriptorHeap;

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

		void Present(int syncInterval = 1);

		void WaitDrawDone();
		void WaitPresent();

		// getter
		IDXGIFactory4*	GetFactoryDep()
		{
			return pFactory_;
		}
		ID3D12Device*	GetDeviceDep()
		{
			return pDevice_;
		}
		ID3D12Device5*	GetDxrDeviceDep()
		{
			return pDxrDevice_;
		}
		bool			IsDxrSupported() const
		{
			return isDxrSupported_;
		}
		CommandQueue&	GetGraphicsQueue()
		{
			return *pGraphicsQueue_;
		}
		CommandQueue&	GetComputeQueue()
		{
			return *pComputeQueue_;
		}
		CommandQueue&	GetCopyQueue()
		{
			return *pCopyQueue_;
		}
		DescriptorHeap&	GetDescriptorHeap(u32 no);
		GlobalDescriptorHeap& GetGlobalViewDescriptorHeap()
		{
			return *pGlobalViewDescHeap_;
		}
		DescriptorAllocator& GetViewDescriptorHeap()
		{
			return *pViewDescHeap_;
		}
		DescriptorAllocator& GetSamplerDescriptorHeap()
		{
			return *pSamplerDescHeap_;
		}
		DescriptorAllocator& GetRtvDescriptorHeap()
		{
			return *pRtvDescHeap_;
		}
		DescriptorAllocator& GetDsvDescriptorHeap()
		{
			return *pDsvDescHeap_;
		}
		DescriptorInfo& GetDefaultViewDescInfo()
		{
			return defaultViewDescInfo_;
		}
		DescriptorInfo& GetDefaultSamplerDescInfo()
		{
			return defaultSamplerDescInfo_;
		}
		Swapchain&		GetSwapchain()
		{
			return *pSwapchain_;
		}

	private:
		IDXGIFactory4*	pFactory_{ nullptr };
		IDXGIAdapter3*	pAdapter_{ nullptr };
		IDXGIOutput4*	pOutput_{ nullptr };
		ID3D12Device*	pDevice_{ nullptr };

		ID3D12Device5*	pDxrDevice_{ nullptr };
		bool			isDxrSupported_ = false;

		CommandQueue*	pGraphicsQueue_{ nullptr };
		CommandQueue*	pComputeQueue_{ nullptr };
		CommandQueue*	pCopyQueue_{ nullptr };

		DescriptorHeap*	pDescHeaps_{ nullptr };
		GlobalDescriptorHeap*	pGlobalViewDescHeap_ = nullptr;
		DescriptorAllocator*	pViewDescHeap_ = nullptr;
		DescriptorAllocator*	pSamplerDescHeap_ = nullptr;
		DescriptorAllocator*	pRtvDescHeap_ = nullptr;
		DescriptorAllocator*	pDsvDescHeap_ = nullptr;

		DescriptorInfo	defaultViewDescInfo_;
		DescriptorInfo	defaultSamplerDescInfo_;

		Swapchain*		pSwapchain_{ nullptr };

		ID3D12Fence*	pFence_{ nullptr };
		u32				fenceValue_{ 0 };
		HANDLE			fenceEvent_{ nullptr };
	};	// class Device

}	// namespace sl12

//	EOF
