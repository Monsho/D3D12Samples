#include <sl12/device.h>

#include <sl12/util.h>
#include <sl12/swapchain.h>
#include <sl12/command_queue.h>
#include <sl12/descriptor_heap.h>
#include <sl12/texture.h>
#include <sl12/command_list.h>
#include <sl12/ring_buffer.h>
#include <string>

namespace sl12
{
	LARGE_INTEGER CpuTimer::frequency_;

	//----
	Device::Device()
	{}

	//----
	Device::~Device()
	{
		Destroy();
	}

	//----
	bool Device::Initialize(HWND hWnd, u32 screenWidth, u32 screenHeight, const std::array<u32, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES>& numDescs, ColorSpaceType csType)
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
		LatestDevice* pLatestDevice = nullptr;
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
					pDevice->QueryInterface(IID_PPV_ARGS(&pLatestDevice));
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
		pLatestDevice_ = pLatestDevice;

		// ディスプレイを列挙する
		IDXGIOutput* pOutput{ nullptr };
		int OutputIndex = 0;
		bool enableHDR = csType != ColorSpaceType::Rec709;
		while (pAdapter_->EnumOutputs(OutputIndex, &pOutput) != DXGI_ERROR_NOT_FOUND)
		{
			hr = pOutput->QueryInterface(IID_PPV_ARGS(&pOutput_));
			SafeRelease(pOutput);
			if (FAILED(hr))
			{
				SafeRelease(pOutput_);
				continue;
			}

			// get desc1.
			DXGI_OUTPUT_DESC1 OutDesc;
			pOutput_->GetDesc1(&OutDesc);

			if (!enableHDR)
			{
				// if HDR mode disabled, choose first output.
				desktopCoordinates_ = OutDesc.DesktopCoordinates;
				minLuminance_ = OutDesc.MinLuminance;
				maxLuminance_ = OutDesc.MaxLuminance;
				maxFullFrameLuminance_ = OutDesc.MaxFullFrameLuminance;
				colorSpaceType_ = ColorSpaceType::Rec709;
				break;
			}

			if (OutDesc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
			{
				desktopCoordinates_ = OutDesc.DesktopCoordinates;
				minLuminance_ = OutDesc.MinLuminance;
				maxLuminance_ = OutDesc.MaxLuminance;
				maxFullFrameLuminance_ = OutDesc.MaxFullFrameLuminance;
				colorSpaceType_ = ColorSpaceType::Rec2020;
				break;
			}

			SafeRelease(pOutput_);
			OutputIndex++;
		}
		// if HDR display not found, choose first output.
		if (!pOutput_)
		{
			pAdapter_->EnumOutputs(0, &pOutput);
			hr = pOutput->QueryInterface(IID_PPV_ARGS(&pOutput_));
			SafeRelease(pOutput);
			if (FAILED(hr))
			{
				SafeRelease(pOutput_);
				return false;
			}

			DXGI_OUTPUT_DESC1 OutDesc;
			pOutput_->GetDesc1(&OutDesc);

			desktopCoordinates_ = OutDesc.DesktopCoordinates;
			minLuminance_ = OutDesc.MinLuminance;
			maxLuminance_ = OutDesc.MaxLuminance;
			maxFullFrameLuminance_ = OutDesc.MaxFullFrameLuminance;
			colorSpaceType_ = ColorSpaceType::Rec709;
		}

#ifdef _DEBUG
		// COPY_DESCRIPTORS_INVALID_RANGESエラーを回避
		ID3D12InfoQueue* pD3DInfoQueue;
		if (SUCCEEDED(pDevice_->QueryInterface(__uuidof(ID3D12InfoQueue), reinterpret_cast<void**>(&pD3DInfoQueue))))
		{
#if 1
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
#endif

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
		if (!pSwapchain_->Initialize(this, pGraphicsQueue_, hWnd, screenWidth, screenHeight))
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

		// create copy ring buffer.
		pRingBuffer_ = std::make_unique<CopyRingBuffer>(this);

		return true;
	}

	//----
	void Device::Destroy()
	{
		// before clear death list.
		pRingBuffer_.reset();

		// clear death list.
		SyncKillObjects(true);

		// shutdown system.
		dummyTextureViews_.clear();
		dummyTextures_.clear();

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

		SafeRelease(pLatestDevice_);
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

		// begin new frame.
		pRingBuffer_->BeginNewFrame();
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
	bool Device::CreateDummyTextures(CommandList* pCmdList)
	{
		dummyTextures_.clear();
		dummyTextureViews_.clear();
		dummyTextures_.resize(DummyTex::Max);
		dummyTextureViews_.resize(DummyTex::Max);

		// Black
		{
			TextureDesc desc{};
			desc.initialState = D3D12_RESOURCE_STATE_COPY_DEST;
			desc.width = desc.height = 4;
			desc.depth = 1;
			desc.dimension = TextureDimension::Texture2D;
			desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.mipLevels = 1;

			std::vector<sl12::u32> bin;
			bin.resize(64 * 4);
			for (auto&& pix : bin)
			{
				pix = 0x00000000;
			}

			dummyTextures_[DummyTex::Black] = std::make_unique<Texture>();
			if (!dummyTextures_[DummyTex::Black]->InitializeFromImageBin(this, pCmdList, desc, bin.data()))
			{
				return false;
			}

			dummyTextureViews_[DummyTex::Black] = std::make_unique<TextureView>();
			if (!dummyTextureViews_[DummyTex::Black]->Initialize(this, dummyTextures_[DummyTex::Black].get()))
			{
				return false;
			}

		}

		// White
		{
			TextureDesc desc{};
			desc.initialState = D3D12_RESOURCE_STATE_COPY_DEST;
			desc.width = desc.height = 4;
			desc.depth = 1;
			desc.dimension = TextureDimension::Texture2D;
			desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.mipLevels = 1;

			std::vector<sl12::u32> bin;
			bin.resize(64 * 4);
			for (auto&& pix : bin)
			{
				pix = 0xFFFFFFFF;
			}

			dummyTextures_[DummyTex::White] = std::make_unique<Texture>();
			if (!dummyTextures_[DummyTex::White]->InitializeFromImageBin(this, pCmdList, desc, bin.data()))
			{
				return false;
			}

			dummyTextureViews_[DummyTex::White] = std::make_unique<TextureView>();
			if (!dummyTextureViews_[DummyTex::White]->Initialize(this, dummyTextures_[DummyTex::White].get()))
			{
				return false;
			}
		}

		// FlatNormal
		{
			TextureDesc desc{};
			desc.initialState = D3D12_RESOURCE_STATE_COPY_DEST;
			desc.width = desc.height = 4;
			desc.depth = 1;
			desc.dimension = TextureDimension::Texture2D;
			desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.mipLevels = 1;

			std::vector<sl12::u32> bin;
			bin.resize(64 * 4);
			for (auto&& pix : bin)
			{
				pix = 0xFF7F7FFF;
			}

			dummyTextures_[DummyTex::FlatNormal] = std::make_unique<Texture>();
			if (!dummyTextures_[DummyTex::FlatNormal]->InitializeFromImageBin(this, pCmdList, desc, bin.data()))
			{
				return false;
			}

			dummyTextureViews_[DummyTex::FlatNormal] = std::make_unique<TextureView>();
			if (!dummyTextureViews_[DummyTex::FlatNormal]->Initialize(this, dummyTextures_[DummyTex::FlatNormal].get()))
			{
				return false;
			}
		}

		for (auto&& tex : dummyTextures_)
		{
			pCmdList->TransitionBarrier(tex.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
		}

		return true;
	}

	//----
	void Device::CopyToBuffer(CommandList* pCmdList, Buffer* pDstBuffer, u32 dstOffset, const void* pSrcData, u32 srcSize)
	{
		pRingBuffer_->CopyToBuffer(pCmdList, pDstBuffer, dstOffset, pSrcData, srcSize);
	}

}	// namespace sl12

//	EOF
