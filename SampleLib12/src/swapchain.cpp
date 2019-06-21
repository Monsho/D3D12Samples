#include <sl12/swapchain.h>

#include <sl12/device.h>
#include <sl12/descriptor.h>
#include <sl12/descriptor_heap.h>
#include <sl12/command_queue.h>


namespace sl12
{
	//----
	bool Swapchain::Initialize(Device* pDev, CommandQueue* pQueue, HWND hWnd, uint32_t width, uint32_t height, DXGI_FORMAT format)
	{
		{
			DXGI_SWAP_CHAIN_DESC1 desc = {};
			desc.BufferCount = kMaxBuffer;
			desc.Width = width;
			desc.Height = height;
			desc.Format = format;
			desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			desc.SampleDesc.Count = 1;
			desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

			IDXGISwapChain1* pSwap;
			auto hr = pDev->GetFactoryDep()->CreateSwapChainForHwnd(pQueue->GetQueueDep(), hWnd, &desc, nullptr, nullptr, &pSwap);
			if (FAILED(hr))
			{
				return false;
			}

			hr = pSwap->QueryInterface(IID_PPV_ARGS(&pSwapchain_));
			if (FAILED(hr))
			{
				return false;
			}

			frameIndex_ = pSwapchain_->GetCurrentBackBufferIndex();
			swapchainEvent_ = pSwapchain_->GetFrameLatencyWaitableObject();

			pSwap->Release();
		}

		// テクスチャを初期化する
		for (u32 i = 0; i < kMaxBuffer; i++)
		{
			if (!textures_[i].InitializeFromSwapchain(pDev, this, i))
			{
				return false;
			}

			if (!views_[i].Initialize(pDev, &textures_[i]))
			{
				return false;
			}
		}

		return true;
	}

	//----
	void Swapchain::Destroy()
	{
		for (auto& v : views_) v.Destroy();
		for (auto& v : textures_) v.Destroy();
		SafeRelease(pSwapchain_);
	}

	//----
	void Swapchain::Present(int syncInterval)
	{
		pSwapchain_->Present(syncInterval, 0);
		frameIndex_ = pSwapchain_->GetCurrentBackBufferIndex();
	}

	//----
	void Swapchain::WaitPresent()
	{
		WaitForSingleObjectEx(swapchainEvent_, 100, FALSE);
	}

	//----
	D3D12_CPU_DESCRIPTOR_HANDLE Swapchain::GetDescHandle(int index)
	{
		return views_[index].GetDescInfo().cpuHandle;
	}

	//----
	D3D12_CPU_DESCRIPTOR_HANDLE Swapchain::GetCurrentDescHandle(int offset)
	{
		int index = (frameIndex_ + offset) % kMaxBuffer;
		return GetDescHandle(index);
	}

}	// namespace sl12

//	EOF
