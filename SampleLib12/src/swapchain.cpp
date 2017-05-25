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
			DXGI_SWAP_CHAIN_DESC desc = {};
			desc.BufferCount = kMaxBuffer;			// フレームバッファとバックバッファで2枚
			desc.BufferDesc.Width = width;
			desc.BufferDesc.Height = height;
			desc.BufferDesc.Format = format;
			desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			desc.OutputWindow = hWnd;
			desc.SampleDesc.Count = 1;
			desc.Windowed = true;

			IDXGISwapChain* pSwap;
			auto hr = pDev->GetFactoryDep()->CreateSwapChain(pQueue->GetQueueDep(), &desc, &pSwap);
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

			pSwap->Release();
		}

		// RTV用のDescriptorを作成する
		DescriptorHeap& rtvHeap = pDev->GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		for (auto& p : pRtvDescs_)
		{
			p = rtvHeap.CreateDescriptor();
		}

		// スワップチェインのバッファを先に作成したDescriptorHeapに登録する
		{
			for (int i = 0; i < kMaxBuffer; i++)
			{
				auto hr = pSwapchain_->GetBuffer(i, IID_PPV_ARGS(&pRenderTargets_[i]));
				if (FAILED(hr))
				{
					return false;
				}

				pDev->GetDeviceDep()->CreateRenderTargetView(pRenderTargets_[i], nullptr, pRtvDescs_[i]->GetCpuHandle());
			}
		}

		return true;
	}

	//----
	void Swapchain::Destroy()
	{
		for (auto& p : pRtvDescs_)
		{
			SafeRelease(p);
		}
		for (auto& rtv : pRenderTargets_)
		{
			SafeRelease(rtv);
		}
		memset(pRenderTargets_, 0, sizeof(pRenderTargets_));
		SafeRelease(pSwapchain_);
	}

	//----
	void Swapchain::Present(int syncInterval)
	{
		pSwapchain_->Present(syncInterval, 0);
		frameIndex_ = pSwapchain_->GetCurrentBackBufferIndex();
	}

	//----
	D3D12_CPU_DESCRIPTOR_HANDLE Swapchain::GetDescHandle(int index)
	{
		assert(pRtvDescs_[index] != nullptr);
		return pRtvDescs_[index]->GetCpuHandle();
	}

	//----
	D3D12_CPU_DESCRIPTOR_HANDLE Swapchain::GetCurrentDescHandle(int offset)
	{
		int index = (frameIndex_ + offset) % kMaxBuffer;
		assert(pRtvDescs_[index] != nullptr);
		return pRtvDescs_[index]->GetCpuHandle();
	}

}	// namespace sl12

//	EOF
