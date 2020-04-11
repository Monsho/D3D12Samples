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
		IDXGIAdapter1* pAdapter = nullptr;
		ID3D12Device* pDevice = nullptr;
		ID3D12Device5* pDxrDevice = nullptr;
		UINT adapterIndex = 0;
		while (true)
		{
			SafeRelease(pAdapter);
			SafeRelease(pDevice);
			SafeRelease(pDxrDevice);

			hr = pFactory_->EnumAdapters1(adapterIndex++, &pAdapter);
			if (FAILED(hr))
				break;

			DXGI_ADAPTER_DESC ad;
			pAdapter->GetDesc(&ad);

			hr = D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&pDevice));
			if (FAILED(hr))
				continue;

			D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureSupportData{};
			if (SUCCEEDED(pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &featureSupportData, sizeof(featureSupportData))))
			{
				isDxrSupported_ = featureSupportData.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
				if (isDxrSupported_)
				{
					pDevice->QueryInterface(IID_PPV_ARGS(&pDxrDevice));
					break;
				}
			}
		}
		if (!pDevice)
		{
			hr = pFactory_->EnumAdapters1(0, &pAdapter);
			if (FAILED(hr))
			{
				hr = pFactory_->EnumWarpAdapter(IID_PPV_ARGS(&pAdapter));
				if (FAILED(hr))
				{
					SafeRelease(pAdapter);
					return false;
				}
				isWarp = true;
			}

			hr = D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&pDevice));
			if (FAILED(hr))
				return false;
		}

		hr = pAdapter->QueryInterface(IID_PPV_ARGS(&pAdapter_));
		SafeRelease(pAdapter);
		if (FAILED(hr))
		{
			return false;
		}

		pDevice_ = pDevice;
		pDxrDevice_ = pDxrDevice;

		// ディスプレイを取得する
		IDXGIOutput* pOutput{ nullptr };
		hr = pAdapter_->EnumOutputs(0, &pOutput);
		if (SUCCEEDED(hr))
		{
			hr = pOutput->QueryInterface(IID_PPV_ARGS(&pOutput_));
			SafeRelease(pOutput);
			if (FAILED(hr))
				SafeRelease(pOutput_);
		}

		// COPY_DESCRIPTORS_INVALID_RANGESエラーを回避
		ID3D12InfoQueue* pD3DInfoQueue;
		if (SUCCEEDED(pDevice_->QueryInterface(__uuidof(ID3D12InfoQueue), reinterpret_cast<void**>(&pD3DInfoQueue))))
		{
#if 0
			// エラー等が出たときに止めたい場合は有効にする
			pD3DInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
			pD3DInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
			pD3DInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
#endif

			D3D12_MESSAGE_ID blockedIds[] = { D3D12_MESSAGE_ID_COPY_DESCRIPTORS_INVALID_RANGES };
			D3D12_INFO_QUEUE_FILTER filter = {};
			filter.DenyList.pIDList = blockedIds;
			filter.DenyList.NumIDs = _countof(blockedIds);
			pD3DInfoQueue->AddRetrievalFilterEntries(&filter);
			pD3DInfoQueue->AddStorageFilterEntries(&filter);
			pD3DInfoQueue->Release();
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
		{
			D3D12_DESCRIPTOR_HEAP_DESC desc{};
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.NumDescriptors = 500000;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			desc.NodeMask = 1;
			pGlobalViewDescHeap_ = new GlobalDescriptorHeap();
			if (!pGlobalViewDescHeap_->Initialize(this, desc))
			{
				return false;
			}

			desc.NumDescriptors = numDescs[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			pViewDescHeap_ = new DescriptorAllocator();
			if (!pViewDescHeap_->Initialize(this, desc))
			{
				return false;
			}

			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			desc.NumDescriptors = numDescs[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			pSamplerDescHeap_ = new DescriptorAllocator();
			if (!pSamplerDescHeap_->Initialize(this, desc))
			{
				return false;
			}

			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			desc.NumDescriptors = numDescs[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			pRtvDescHeap_ = new DescriptorAllocator();
			if (!pRtvDescHeap_->Initialize(this, desc))
			{
				return false;
			}

			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			desc.NumDescriptors = numDescs[D3D12_DESCRIPTOR_HEAP_TYPE_DSV];
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			pDsvDescHeap_ = new DescriptorAllocator();
			if (!pDsvDescHeap_->Initialize(this, desc))
			{
				return false;
			}

			defaultViewDescInfo_ = pViewDescHeap_->Allocate();
			defaultSamplerDescInfo_ = pSamplerDescHeap_->Allocate();
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
		SyncKillObjects(true);

		SafeRelease(pFence_);

		SafeDelete(pSwapchain_);

		defaultSamplerDescInfo_.Free();
		defaultViewDescInfo_.Free();

		SafeDelete(pDsvDescHeap_);
		SafeDelete(pRtvDescHeap_);
		SafeDelete(pSamplerDescHeap_);
		SafeDelete(pViewDescHeap_);
		SafeDelete(pGlobalViewDescHeap_);

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

}	// namespace sl12

//	EOF
