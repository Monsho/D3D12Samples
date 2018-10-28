#include <sl12/device.h>

#include <sl12/swapchain.h>
#include <sl12/command_queue.h>
#include <sl12/descriptor_heap.h>


namespace sl12
{
	//----
	bool Device::Initialize(HWND hWnd, u32 screenWidth, u32 screenHeight, const std::array<u32, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES>& numDescs)
	{
		uint32_t factoryFlags = 0;
#ifdef _DEBUG
		// デバッグレイヤーの有効化
		{
			ID3D12Debug* debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
				debugController->Release();
			}
		}

		// ファクトリをデバッグモードで作成する
		//factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
		// ファクトリの生成
		auto hr = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&pFactory_));
		if (FAILED(hr))
		{
			return false;
		}

		// アダプタを取得する
		bool isWarp = false;
		IDXGIAdapter1* pAdapter{ nullptr };
		hr = pFactory_->EnumAdapters1(0, &pAdapter);
		if (FAILED(hr))
		{
			// 取得できない場合はWarpアダプタを取得
			SafeRelease(pAdapter);

			hr = pFactory_->EnumWarpAdapter(IID_PPV_ARGS(&pAdapter));
			if (FAILED(hr))
			{
				SafeRelease(pAdapter);
				return false;
			}
			isWarp = true;
		}
		hr = pAdapter->QueryInterface(IID_PPV_ARGS(&pAdapter_));
		SafeRelease(pAdapter);
		if (FAILED(hr))
		{
			return false;
		}

		if (!isWarp)
		{
			// ディスプレイを取得する
			IDXGIOutput* pOutput{ nullptr };
			hr = pAdapter_->EnumOutputs(0, &pOutput);
			if (FAILED(hr))
			{
				return false;
			}

			hr = pOutput->QueryInterface(IID_PPV_ARGS(&pOutput_));
			SafeRelease(pOutput);
			if (FAILED(hr))
			{
				return false;
			}
		}

		// デバイスの生成
		hr = D3D12CreateDevice(pAdapter_, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDevice_));
		if (FAILED(hr))
		{
			return false;
		}

		// DirectX Raytracingが使用可能ならDXR用のデバイスを作成する
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureSupportData{};
		if (SUCCEEDED(pDevice_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &featureSupportData, sizeof(featureSupportData))))
		{
			isDxrSupported_ = featureSupportData.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
			if (isDxrSupported_)
			{
				pDevice_->QueryInterface(IID_PPV_ARGS(&pDxrDevice_));
			}
		}

		// Queueの作成
		pGraphicsQueue_ = new CommandQueue();
		pComputeQueue_ = new CommandQueue();
		pCopyQueue_ = new CommandQueue();
		if (!pGraphicsQueue_ || !pComputeQueue_ || !pCopyQueue_)
		{
			return false;
		}
		if (!pGraphicsQueue_->Initialize(this, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_QUEUE_PRIORITY_HIGH))
		{
			return false;
		}
		if (!pComputeQueue_->Initialize(this, D3D12_COMMAND_LIST_TYPE_COMPUTE))
		{
			return false;
		}
		if (!pCopyQueue_->Initialize(this, D3D12_COMMAND_LIST_TYPE_COPY, D3D12_COMMAND_QUEUE_PRIORITY_HIGH))
		{
			return false;
		}

		// DescriptorHeapの作成
		pDescHeaps_ = new DescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
		for (u32 i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
		{
			D3D12_DESCRIPTOR_HEAP_DESC desc{
				(D3D12_DESCRIPTOR_HEAP_TYPE)i,
				numDescs[i],
				(i == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) || (i == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER) ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
				1
			};
			if (!pDescHeaps_[i].Initialize(this, desc))
			{
				return false;
			}
		}

		// Swapchainの作成
		pSwapchain_ = new Swapchain();
		if (!pSwapchain_)
		{
			return false;
		}
		if (!pSwapchain_->Initialize(this, pGraphicsQueue_, hWnd, screenWidth, screenHeight, DXGI_FORMAT_R8G8B8A8_UNORM))
		{
			return false;
		}

		// Fenceの作成
		hr = pDevice_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence_));
		if (FAILED(hr))
		{
			return false;
		}
		fenceValue_ = 1;

		fenceEvent_ = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
		if (fenceEvent_ == nullptr)
		{
			return false;
		}

		return true;
	}

	//----
	void Device::Destroy()
	{
		//CloseHandle(fenceEvent_);
		SafeRelease(pFence_);

		SafeDelete(pSwapchain_);

		SafeDeleteArray(pDescHeaps_);

		SafeDelete(pGraphicsQueue_);
		SafeDelete(pComputeQueue_);
		SafeDelete(pCopyQueue_);

		SafeRelease(pDxrDevice_);

		SafeRelease(pDevice_);
		SafeRelease(pOutput_);
		SafeRelease(pAdapter_);
		SafeRelease(pFactory_);
	}

	//----
	void Device::Present(int syncInterval)
	{
		if (pSwapchain_)
		{
			pSwapchain_->Present(syncInterval);
		}
	}

	//----
	void Device::WaitDrawDone()
	{
		if (pGraphicsQueue_)
		{
			// 現在のFence値がコマンド終了後にFenceに書き込まれるようにする
			UINT64 fvalue = fenceValue_;
			pGraphicsQueue_->GetQueueDep()->Signal(pFence_, fvalue);
			fenceValue_++;

			// まだコマンドキューが終了していないことを確認する
			// ここまででコマンドキューが終了してしまうとイベントが一切発火されなくなるのでチェックしている
			if (pFence_->GetCompletedValue() < fvalue)
			{
				// このFenceにおいて、fvalue の値になったらイベントを発火させる
				pFence_->SetEventOnCompletion(fvalue, fenceEvent_);
				// イベントが発火するまで待つ
				WaitForSingleObject(fenceEvent_, INFINITE);
			}
		}
	}

	//----
	void Device::WaitPresent()
	{
		if (pSwapchain_)
		{
			pSwapchain_->WaitPresent();
		}
	}

	//----
	DescriptorHeap& Device::GetDescriptorHeap(u32 no)
	{
		return pDescHeaps_[no];
	}

}	// namespace sl12

//	EOF
