#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXTex.h>
#include <array>

#include "file.h"


namespace
{
	static const wchar_t* kWindowTitle = L"D3D12Sample";
	static const int kWindowWidth = 1920;
	static const int kWindowHeight = 1080;

	template <typename T>
	void SafeRelease(T*& p)
	{
		if (p) { p->Release(); p = nullptr; }
	}

	HWND	g_hWnd_;
}

// Window Proc
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// Handle destroy/shutdown messages.
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	// Handle any messages the switch statement didn't.
	return DefWindowProc(hWnd, message, wParam, lParam);
}

// Windowの初期化
void InitWindow(HINSTANCE hInstance, int nCmdShow)
{
	// Initialize the window class.
	WNDCLASSEX windowClass = { 0 };
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = hInstance;
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.lpszClassName = L"WindowClass1";
	RegisterClassEx(&windowClass);

	RECT windowRect = { 0, 0, kWindowWidth, kWindowHeight };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	// Create the window and store a handle to it.
	g_hWnd_ = CreateWindowEx(NULL,
		L"WindowClass1",
		kWindowTitle,
		WS_OVERLAPPEDWINDOW,
		300,
		300,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		NULL,		// We have no parent window, NULL.
		NULL,		// We aren't using menus, NULL.
		hInstance,
		NULL);		// We aren't using multiple windows, NULL.

	ShowWindow(g_hWnd_, nCmdShow);
}

class Device;

class CommandQueue
{
	friend class CommandList;

public:
	CommandQueue()
	{}
	~CommandQueue()
	{
		Destroy();
	}

	bool Initialize(ID3D12Device* pDev, D3D12_COMMAND_LIST_TYPE type)
	{
		D3D12_COMMAND_QUEUE_DESC desc{};
		desc.Type = type;
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;		// GPUタイムアウトが有効
		desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		auto hr = pDev->CreateCommandQueue(&desc, IID_PPV_ARGS(&pQueue_));
		if (FAILED(hr))
		{
			return false;
		}

		listType_ = type;
		return true;
	}

	void Destroy()
	{
		SafeRelease(pQueue_);
	}

	ID3D12CommandQueue* GetQueueDep() { return pQueue_; }

private:
	ID3D12CommandQueue*	pQueue_{ nullptr };
	D3D12_COMMAND_LIST_TYPE listType_{ D3D12_COMMAND_LIST_TYPE_DIRECT };
};	// class CommandQueue

class CommandList
{
public:
	CommandList()
	{}
	~CommandList()
	{}

	bool Initialize(ID3D12Device* pDev, CommandQueue* pQueue)
	{
		auto hr = pDev->CreateCommandAllocator(pQueue->listType_, IID_PPV_ARGS(&pCmdAllocator_));
		if (FAILED(hr))
		{
			return false;
		}

		hr = pDev->CreateCommandList(0, pQueue->listType_, pCmdAllocator_, nullptr, IID_PPV_ARGS(&pCmdList_));
		if (FAILED(hr))
		{
			return false;
		}

		pCmdList_->Close();

		pParentQueue_ = pQueue;
		return true;
	}

	void Destroy()
	{
		pParentQueue_ = nullptr;
		SafeRelease(pCmdList_);
		SafeRelease(pCmdAllocator_);
	}

	void Reset()
	{
		auto hr = pCmdAllocator_->Reset();
		assert(SUCCEEDED(hr));

		hr = pCmdList_->Reset(pCmdAllocator_, nullptr);
		assert(SUCCEEDED(hr));
	}

	void Close()
	{
		auto hr = pCmdList_->Close();
		assert(SUCCEEDED(hr));
	}

	void Execute()
	{
		ID3D12CommandList* lists[] = { pCmdList_ };
		pParentQueue_->GetQueueDep()->ExecuteCommandLists(ARRAYSIZE(lists), lists);
	}

	// getter
	ID3D12CommandAllocator* GetCommandAllocator() { return pCmdAllocator_; }
	ID3D12GraphicsCommandList* GetCommandList() { return pCmdList_; };

private:
	CommandQueue*				pParentQueue_{ nullptr };
	ID3D12CommandAllocator*		pCmdAllocator_{ nullptr };
	ID3D12GraphicsCommandList*	pCmdList_{ nullptr };
};	// class CommandList

class Descriptor
{
	friend class DescriptorHeap;

public:
	Descriptor()
	{}
	~Descriptor()
	{
		Destroy();
	}

	void Destroy()
	{}

	// getter
	D3D12_CPU_DESCRIPTOR_HANDLE	GetCpuHandle(uint32_t no = 0)
	{
		assert(no < numDescs_);
		D3D12_CPU_DESCRIPTOR_HANDLE ret = cpuHandle_;
		ret.ptr += descSize_ * no;
		return ret;
	}
	D3D12_GPU_DESCRIPTOR_HANDLE	GetGpuHandle(uint32_t no = 0)
	{
		assert(no < numDescs_);
		D3D12_GPU_DESCRIPTOR_HANDLE ret = gpuHandle_;
		ret.ptr += descSize_ * no;
		return ret;
	}
	uint32_t GetDescSize() const { return descSize_; }
	uint32_t GetNumDescs() const { return numDescs_; }

private:
	D3D12_CPU_DESCRIPTOR_HANDLE	cpuHandle_{ 0 };
	D3D12_GPU_DESCRIPTOR_HANDLE	gpuHandle_{ 0 };
	uint32_t					descSize_{ 0 };
	uint32_t					numDescs_{ 0 };
};	// class Descriptor

class DescriptorHeap
{
public:
	DescriptorHeap()
	{}
	~DescriptorHeap()
	{
		Destroy();
	}

	bool Initialize(ID3D12Device* pDev, const D3D12_DESCRIPTOR_HEAP_DESC& desc)
	{
		auto hr = pDev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pHeap_));
		if (FAILED(hr))
		{
			return false;
		}

		heapDesc_ = desc;
		descSize_ = pDev->GetDescriptorHandleIncrementSize(desc.Type);

		return true;
	}

	void Destroy()
	{
		SafeRelease(pHeap_);
	}

	Descriptor CreateDescriptor(uint32_t index, uint32_t num)
	{
		assert(heapDesc_.NumDescriptors >= index + num);

		Descriptor ret;
		ret.cpuHandle_ = pHeap_->GetCPUDescriptorHandleForHeapStart();
		ret.gpuHandle_ = pHeap_->GetGPUDescriptorHandleForHeapStart();
		ret.cpuHandle_.ptr += descSize_ * index;
		ret.gpuHandle_.ptr += descSize_ * index;
		ret.descSize_ = descSize_;
		ret.numDescs_ = num;

		return ret;
	}

	// getter
	ID3D12DescriptorHeap* GetHeap() { return pHeap_; }

private:
	ID3D12DescriptorHeap*		pHeap_{ nullptr };
	D3D12_DESCRIPTOR_HEAP_DESC	heapDesc_{};
	uint32_t					descSize_{ 0 };
};	// class DescriptorHeap

class Swapchain
{
public:
	static const uint32_t	kMaxBuffer = 2;

public:
	Swapchain()
	{}
	~Swapchain()
	{
		Destroy();
	}

	bool Initialize(IDXGIFactory4* pFactory, ID3D12Device* pDev, CommandQueue* pQueue, HWND hWnd, uint32_t width, uint32_t height, DXGI_FORMAT format)
	{
		{
			DXGI_SWAP_CHAIN_DESC desc = {};
			desc.BufferCount = 2;			// フレームバッファとバックバッファで2枚
			desc.BufferDesc.Width = width;
			desc.BufferDesc.Height = height;
			desc.BufferDesc.Format = format;
			desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			desc.OutputWindow = hWnd;
			desc.SampleDesc.Count = 1;
			desc.Windowed = true;

			IDXGISwapChain* pSwap;
			auto hr = pFactory->CreateSwapChain(pQueue->GetQueueDep(), &desc, &pSwap);
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

		// スワップチェインをRenderTargetとして使用するためのDescriptorHeapを作成
		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.NumDescriptors = kMaxBuffer;
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;		// RenderTargetView
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;	// シェーダからアクセスしないのでNONEでOK

			if (!descHeap_.Initialize(pDev, desc))
			{
				return false;
			}

			rtvDescs_ = descHeap_.CreateDescriptor(0, kMaxBuffer);
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

				pDev->CreateRenderTargetView(pRenderTargets_[i], nullptr, rtvDescs_.GetCpuHandle(i));
			}
		}

		return true;
	}

	void Destroy()
	{
		for (auto rtv : pRenderTargets_)
		{
			SafeRelease(rtv);
		}
		memset(pRenderTargets_, 0, sizeof(pRenderTargets_));
		rtvDescs_.Destroy();
		descHeap_.Destroy();
		SafeRelease(pSwapchain_);
	}

	void Present()
	{
		pSwapchain_->Present(1, 0);
		frameIndex_ = pSwapchain_->GetCurrentBackBufferIndex();
	}

	// getter
	ID3D12Resource* GetCurrentRenderTarget() { return pRenderTargets_[frameIndex_]; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentDescHandle()
	{
		return rtvDescs_.GetCpuHandle(frameIndex_);
	}
	int32_t GetFrameIndex() const { return frameIndex_; }

private:
	IDXGISwapChain3*		pSwapchain_{ nullptr };
	DescriptorHeap			descHeap_{};
	Descriptor				rtvDescs_{};
	ID3D12Resource*			pRenderTargets_[kMaxBuffer]{ nullptr };
	int32_t					frameIndex_{ 0 };
};	// class Swapchain

class Device
{
public:
	Device()
	{}
	~Device()
	{
		Destroy();
	}

	bool Initialize(HWND hWnd, uint32_t screenWidth, uint32_t screenHeight)
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
		factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
		// ファクトリの生成
		auto hr = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&pFactory_));
		if (FAILED(hr))
		{
			return false;
		}

		// アダプタを取得する
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
		}
		hr = pAdapter->QueryInterface(IID_PPV_ARGS(&pAdapter_));
		SafeRelease(pAdapter);
		if (FAILED(hr))
		{
			return false;
		}

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

		// デバイスの生成
		hr = D3D12CreateDevice(pAdapter_, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDevice_));
		if (FAILED(hr))
		{
			return false;
		}

		// Queueの作成
		if (!graphicsQueue_.Initialize(pDevice_, D3D12_COMMAND_LIST_TYPE_DIRECT))
		{
			return false;
		}
		if (!computeQueue_.Initialize(pDevice_, D3D12_COMMAND_LIST_TYPE_COMPUTE))
		{
			return false;
		}
		if (!copyQueue_.Initialize(pDevice_, D3D12_COMMAND_LIST_TYPE_COPY))
		{
			return false;
		}

		// Swapchainの作成
		if (!swapchain_.Initialize(pFactory_, pDevice_, &graphicsQueue_, hWnd, screenWidth, screenHeight, DXGI_FORMAT_R8G8B8A8_UNORM))
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

	void Destroy()
	{
		SafeRelease(pFence_);

		swapchain_.Destroy();

		graphicsQueue_.Destroy();
		computeQueue_.Destroy();
		copyQueue_.Destroy();

		SafeRelease(pDevice_);
		SafeRelease(pOutput_);
		SafeRelease(pAdapter_);
		SafeRelease(pFactory_);
	}

	void Present()
	{
		swapchain_.Present();
	}

	void WaitDrawDone()
	{
		// 現在のFence値がコマンド終了後にFenceに書き込まれるようにする
		UINT64 fvalue = fenceValue_;
		graphicsQueue_.GetQueueDep()->Signal(pFence_, fvalue);
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

	// getter
	ID3D12Device* GetDevice() { return pDevice_; }
	CommandQueue& GetGraphicsQueue() { return graphicsQueue_; }
	CommandQueue& GetComputeQueue() { return computeQueue_; }
	CommandQueue& GetCopyQueue() { return copyQueue_; }
	Swapchain&	  GetSwapchain() { return swapchain_; }
	int32_t       GetFrameIndex() const { return swapchain_.GetFrameIndex(); }

private:
	IDXGIFactory4*	pFactory_{ nullptr };
	IDXGIAdapter3*	pAdapter_{ nullptr };
	IDXGIOutput4*	pOutput_{ nullptr };
	ID3D12Device*	pDevice_{ nullptr };

	CommandQueue	graphicsQueue_;
	CommandQueue	computeQueue_;
	CommandQueue	copyQueue_;

	Swapchain		swapchain_;

	ID3D12Fence*	pFence_{ nullptr };
	uint32_t		fenceValue_{ 0 };
	HANDLE			fenceEvent_{ nullptr };
};	// class Device

namespace
{
	//static const DXGI_FORMAT	kDepthBufferFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
	//static const DXGI_FORMAT	kDepthViewFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	static const DXGI_FORMAT	kDepthBufferFormat = DXGI_FORMAT_R32_TYPELESS;
	static const DXGI_FORMAT	kDepthViewFormat = DXGI_FORMAT_D32_FLOAT;

	Device			g_Device_;
	CommandList		g_mainCmdList_;
	DescriptorHeap	g_DescHeaps_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	ID3D12Resource*	g_pDepthBuffer_ = nullptr;
	Descriptor		g_DepthBufferDesc_;

	static const uint32_t kMaxCBs = 4;
	ID3D12Resource*	g_pCBScenes_[kMaxCBs] = { nullptr };
	void*			g_pCBSceneBuffers_[kMaxCBs] = { nullptr };
	Descriptor		g_CBSceneDescs_[kMaxCBs];

	ID3D12Resource*	g_pVB0_ = nullptr;
	ID3D12Resource*	g_pVB1_ = nullptr;
	ID3D12Resource*	g_pVB2_ = nullptr;
	ID3D12Resource*	g_pIB_ = nullptr;
	D3D12_VERTEX_BUFFER_VIEW g_VB0View_, g_VB1View_, g_VB2View_;
	D3D12_INDEX_BUFFER_VIEW g_IBView_;

	ID3D12Resource* g_pTexture_ = nullptr;
	Descriptor		g_TextureDesc_;
	Descriptor		g_SamplerDesc_;

	File	g_VShader_, g_PShader_;

	ID3D12RootSignature*	g_pRootSig_ = nullptr;

	ID3D12PipelineState*	g_pPipelineWriteS_ = nullptr;
	ID3D12PipelineState*	g_pPipelineUseS_ = nullptr;
}

bool InitializeAssets()
{
	ID3D12Device* pDev = g_Device_.GetDevice();

	// 深度バッファを作成
	{
		D3D12_HEAP_PROPERTIES prop{};
		prop.Type = D3D12_HEAP_TYPE_DEFAULT;
		prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		prop.CreationNodeMask = 1;
		prop.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC desc{};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Width = kWindowWidth;
		desc.Height = kWindowHeight;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = kDepthBufferFormat;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE;

		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = kDepthViewFormat;
		clearValue.DepthStencil.Depth = 1.0f;
		clearValue.DepthStencil.Stencil = 0;

		auto hr = pDev->CreateCommittedResource(&prop, flags, &desc, initState, &clearValue, IID_PPV_ARGS(&g_pDepthBuffer_));
		if (FAILED(hr))
		{
			return false;
		}

		g_DepthBufferDesc_ = g_DescHeaps_[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].CreateDescriptor(0, 1);

		D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc{};
		viewDesc.Format = kDepthViewFormat;
		viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipSlice = 0;

		pDev->CreateDepthStencilView(g_pDepthBuffer_, &viewDesc, g_DepthBufferDesc_.GetCpuHandle());
	}

	// 定数バッファを作成
	{
		D3D12_HEAP_PROPERTIES prop{};
		prop.Type = D3D12_HEAP_TYPE_UPLOAD;
		prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		prop.CreationNodeMask = 1;
		prop.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC desc{};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment = 0;
		desc.Width = 256;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		for (int i = 0; i < _countof(g_pCBScenes_); i++)
		{
			auto hr = pDev->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&g_pCBScenes_[i]));
			if (FAILED(hr))
			{
				return false;
			}

			g_CBSceneDescs_[i] = g_DescHeaps_[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].CreateDescriptor(i, 1);

			D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc{};
			viewDesc.BufferLocation = g_pCBScenes_[i]->GetGPUVirtualAddress();
			viewDesc.SizeInBytes = 256;
			pDev->CreateConstantBufferView(&viewDesc, g_CBSceneDescs_[i].GetCpuHandle());

			g_pCBScenes_[i]->Map(0, nullptr, &g_pCBSceneBuffers_[i]);
		}
	}

	// 頂点バッファを作成
	{
		{
			float positions[] = {
				-1.0f,  1.0f, 0.0f,
				 1.0f,  1.0f, 0.0f,
				-1.0f, -1.0f, 0.0f,
				 1.0f, -1.0f, 0.0f,
			};

			D3D12_HEAP_PROPERTIES prop{};
			prop.Type = D3D12_HEAP_TYPE_UPLOAD;
			prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			prop.CreationNodeMask = 1;
			prop.VisibleNodeMask = 1;

			D3D12_RESOURCE_DESC desc{};
			desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			desc.Alignment = 0;
			desc.Width = sizeof(positions);
			desc.Height = 1;
			desc.DepthOrArraySize = 1;
			desc.MipLevels = 1;
			desc.Format = DXGI_FORMAT_UNKNOWN;
			desc.SampleDesc.Count = 1;
			desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			desc.Flags = D3D12_RESOURCE_FLAG_NONE;

			auto hr = pDev->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&g_pVB0_));
			if (FAILED(hr))
			{
				return false;
			}

			// バッファにコピー
			UINT8* p;
			hr = g_pVB0_->Map(0, nullptr, reinterpret_cast<void**>(&p));
			if (FAILED(hr))
			{
				return false;
			}
			memcpy(p, positions, sizeof(positions));
			g_pVB0_->Unmap(0, nullptr);

			// Viewの初期化
			g_VB0View_.BufferLocation = g_pVB0_->GetGPUVirtualAddress();
			g_VB0View_.SizeInBytes = sizeof(positions);
			g_VB0View_.StrideInBytes = sizeof(float) * 3;
		}
		{
			uint32_t colors[] = {
				0xff0000ff, 0xff00ff00, 0xffff0000, 0xffffffff
			};

			D3D12_HEAP_PROPERTIES prop{};
			prop.Type = D3D12_HEAP_TYPE_UPLOAD;
			prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			prop.CreationNodeMask = 1;
			prop.VisibleNodeMask = 1;

			D3D12_RESOURCE_DESC desc{};
			desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			desc.Alignment = 0;
			desc.Width = sizeof(colors);
			desc.Height = 1;
			desc.DepthOrArraySize = 1;
			desc.MipLevels = 1;
			desc.Format = DXGI_FORMAT_UNKNOWN;
			desc.SampleDesc.Count = 1;
			desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			desc.Flags = D3D12_RESOURCE_FLAG_NONE;

			auto hr = pDev->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&g_pVB1_));
			if (FAILED(hr))
			{
				return false;
			}

			// バッファにコピー
			UINT8* p;
			hr = g_pVB1_->Map(0, nullptr, reinterpret_cast<void**>(&p));
			if (FAILED(hr))
			{
				return false;
			}
			memcpy(p, colors, sizeof(colors));
			g_pVB1_->Unmap(0, nullptr);

			// Viewの初期化
			g_VB1View_.BufferLocation = g_pVB1_->GetGPUVirtualAddress();
			g_VB1View_.SizeInBytes = sizeof(colors);
			g_VB1View_.StrideInBytes = sizeof(uint32_t);
		}
		{
			float uvs[] = {
				0.0f, 0.0f,
				1.0f, 0.0f,
				0.0f, 1.0f,
				1.0f, 1.0f
			};

			D3D12_HEAP_PROPERTIES prop{};
			prop.Type = D3D12_HEAP_TYPE_UPLOAD;
			prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			prop.CreationNodeMask = 1;
			prop.VisibleNodeMask = 1;

			D3D12_RESOURCE_DESC desc{};
			desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			desc.Alignment = 0;
			desc.Width = sizeof(uvs);
			desc.Height = 1;
			desc.DepthOrArraySize = 1;
			desc.MipLevels = 1;
			desc.Format = DXGI_FORMAT_UNKNOWN;
			desc.SampleDesc.Count = 1;
			desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			desc.Flags = D3D12_RESOURCE_FLAG_NONE;

			auto hr = pDev->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&g_pVB2_));
			if (FAILED(hr))
			{
				return false;
			}

			// バッファにコピー
			UINT8* p;
			hr = g_pVB2_->Map(0, nullptr, reinterpret_cast<void**>(&p));
			if (FAILED(hr))
			{
				return false;
			}
			memcpy(p, uvs, sizeof(uvs));
			g_pVB2_->Unmap(0, nullptr);

			// Viewの初期化
			g_VB2View_.BufferLocation = g_pVB2_->GetGPUVirtualAddress();
			g_VB2View_.SizeInBytes = sizeof(uvs);
			g_VB2View_.StrideInBytes = sizeof(float) * 2;
		}
	}
	// インデックスバッファを作成
	{
		uint32_t indices[] = {
			0, 1, 2, 1, 3, 2
		};

		D3D12_HEAP_PROPERTIES prop{};
		prop.Type = D3D12_HEAP_TYPE_UPLOAD;
		prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		prop.CreationNodeMask = 1;
		prop.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC desc{};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment = 0;
		desc.Width = sizeof(indices);
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		auto hr = pDev->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&g_pIB_));
		if (FAILED(hr))
		{
			return false;
		}

		// バッファにコピー
		UINT8* p;
		hr = g_pIB_->Map(0, nullptr, reinterpret_cast<void**>(&p));
		if (FAILED(hr))
		{
			return false;
		}
		memcpy(p, indices, sizeof(indices));
		g_pIB_->Unmap(0, nullptr);

		// Viewの初期化
		g_IBView_.BufferLocation = g_pIB_->GetGPUVirtualAddress();
		g_IBView_.SizeInBytes = sizeof(indices);
		g_IBView_.Format = DXGI_FORMAT_R32_UINT;
	}

	// テクスチャロード
	{
		DirectX::ScratchImage image;
		auto hr = DirectX::LoadFromTGAFile(L"data/icon.tga", nullptr, image);
		if (FAILED(hr))
		{
			return false;
		}

		hr = DirectX::CreateTexture(pDev, image.GetMetadata(), &g_pTexture_);
		if (FAILED(hr))
		{
			return false;
		}

		{
			ID3D12Resource* pSrcImage = nullptr;

			D3D12_HEAP_PROPERTIES prop{};
			prop.Type = D3D12_HEAP_TYPE_UPLOAD;
			prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			prop.CreationNodeMask = 1;
			prop.VisibleNodeMask = 1;

			const DirectX::Image* pImage = image.GetImage(0, 0, 0);

			D3D12_RESOURCE_DESC desc{};
			desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			desc.Alignment = 0;
			desc.Width = pImage->rowPitch * pImage->height;
			desc.Height = 1;
			desc.DepthOrArraySize = 1;
			desc.MipLevels = 1;
			desc.Format = DXGI_FORMAT_UNKNOWN;
			desc.SampleDesc.Count = 1;
			desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			desc.Flags = D3D12_RESOURCE_FLAG_NONE;

			auto hr = pDev->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&pSrcImage));
			if (FAILED(hr))
			{
				return false;
			}

			D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
			D3D12_RESOURCE_DESC texDesc = g_pTexture_->GetDesc();
			UINT numRows;
			UINT64 rowSize, totalSize;
			pDev->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, &numRows, &rowSize, &totalSize);

			void* pData;
			pSrcImage->Map(0, nullptr, &pData);
			memcpy(pData, pImage->pixels, pImage->rowPitch * pImage->height);
			pSrcImage->Unmap(0, nullptr);

			g_mainCmdList_.Reset();

			ID3D12GraphicsCommandList* pCmdList = g_mainCmdList_.GetCommandList();
			D3D12_TEXTURE_COPY_LOCATION srcLoc, dstLoc;
			srcLoc.pResource = pSrcImage;
			srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			srcLoc.PlacedFootprint = footprint;
			dstLoc.pResource = g_pTexture_;
			dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dstLoc.SubresourceIndex = 0;
			pCmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

			g_mainCmdList_.Close();
			g_mainCmdList_.Execute();
			g_Device_.WaitDrawDone();

			SafeRelease(pSrcImage);
		}

		g_TextureDesc_ = g_DescHeaps_[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].CreateDescriptor(10, 1);

		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = image.GetMetadata().format;
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		viewDesc.Texture2D.MipLevels = (UINT)image.GetMetadata().mipLevels;
		viewDesc.Texture2D.MostDetailedMip = 0;
		viewDesc.Texture2D.PlaneSlice = 0;
		viewDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		pDev->CreateShaderResourceView(g_pTexture_, &viewDesc, g_TextureDesc_.GetCpuHandle());
	}
	// サンプラ作成
	{
		g_SamplerDesc_ = g_DescHeaps_[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].CreateDescriptor(0, 1);

		D3D12_SAMPLER_DESC desc{};
		desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		pDev->CreateSampler(&desc, g_SamplerDesc_.GetCpuHandle());
	}

	// シェーダロード
	if (!g_VShader_.ReadFile("data/VSSample.cso"))
	{
		return false;
	}
	if (!g_PShader_.ReadFile("data/PSSample.cso"))
	{
		return false;
	}

	// ルートシグネチャを作成
	{
		D3D12_DESCRIPTOR_RANGE ranges[3];
		D3D12_ROOT_PARAMETER rootParameters[3];

		ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		ranges[0].NumDescriptors = 1;
		ranges[0].BaseShaderRegister = 0;
		ranges[0].RegisterSpace = 0;
		ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		ranges[1].NumDescriptors = 1;
		ranges[1].BaseShaderRegister = 0;
		ranges[1].RegisterSpace = 0;
		ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
		ranges[2].NumDescriptors = 1;
		ranges[2].BaseShaderRegister = 0;
		ranges[2].RegisterSpace = 0;
		ranges[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[0].DescriptorTable.pDescriptorRanges = &ranges[0];
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[1].DescriptorTable.pDescriptorRanges = &ranges[1];
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[2].DescriptorTable.pDescriptorRanges = &ranges[2];
		rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_SIGNATURE_DESC desc;
		desc.NumParameters = _countof(rootParameters);
		desc.pParameters = rootParameters;
		desc.NumStaticSamplers = 0;
		desc.pStaticSamplers = nullptr;
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		ID3DBlob* pSignature{ nullptr };
		ID3DBlob* pError{ nullptr };
		auto hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError);
		if (FAILED(hr))
		{
			SafeRelease(pSignature);
			SafeRelease(pError);
			return false;
		}

		hr = pDev->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&g_pRootSig_));
		SafeRelease(pSignature);
		SafeRelease(pError);
		if (FAILED(hr))
		{
			return false;
		}
	}

	// PSOを作成
	{
		D3D12_INPUT_ELEMENT_DESC elementDescs[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_RASTERIZER_DESC rasterDesc{};
		rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
		rasterDesc.FrontCounterClockwise = false;
		rasterDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		rasterDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		rasterDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		rasterDesc.DepthClipEnable = true;
		rasterDesc.MultisampleEnable = false;
		rasterDesc.AntialiasedLineEnable = false;
		rasterDesc.ForcedSampleCount = 0;
		rasterDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		D3D12_BLEND_DESC blendDesc{};
		blendDesc.AlphaToCoverageEnable = false;
		blendDesc.IndependentBlendEnable = false;
		blendDesc.RenderTarget[0].BlendEnable = false;
		blendDesc.RenderTarget[0].LogicOpEnable = false;
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		D3D12_DEPTH_STENCIL_DESC dsDesc{};
		D3D12_DEPTH_STENCILOP_DESC stencilWDesc{};
		stencilWDesc.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		stencilWDesc.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		stencilWDesc.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
		stencilWDesc.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		dsDesc.DepthEnable = true;
		dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		dsDesc.StencilEnable = true;
		dsDesc.StencilReadMask = dsDesc.StencilWriteMask = 0xff;
		dsDesc.FrontFace = stencilWDesc;
		dsDesc.BackFace = stencilWDesc;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
		desc.InputLayout = { elementDescs, _countof(elementDescs) };
		desc.pRootSignature = g_pRootSig_;
		desc.VS = { reinterpret_cast<UINT8*>(g_VShader_.GetData()), g_VShader_.GetSize() };
		desc.PS = { reinterpret_cast<UINT8*>(g_PShader_.GetData()), g_PShader_.GetSize() };
		desc.RasterizerState = rasterDesc;
		desc.BlendState = blendDesc;
		desc.DepthStencilState = dsDesc;
		desc.SampleMask = UINT_MAX;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.DSVFormat = kDepthViewFormat;
		desc.SampleDesc.Count = 1;

		auto hr = pDev->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&g_pPipelineWriteS_));
		if (FAILED(hr))
		{
			return false;
		}

		D3D12_DEPTH_STENCILOP_DESC stencilUDesc{};
		stencilUDesc.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		stencilUDesc.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		stencilUDesc.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		stencilUDesc.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
		dsDesc.DepthEnable = false;
		dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		dsDesc.StencilEnable = true;
		dsDesc.StencilReadMask = dsDesc.StencilWriteMask = 0xff;
		dsDesc.FrontFace = stencilUDesc;
		dsDesc.BackFace = stencilUDesc;
		desc.DepthStencilState = dsDesc;

		hr = pDev->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&g_pPipelineUseS_));
		if (FAILED(hr))
		{
			return false;
		}
	}

	return true;
}

void DestroyAssets()
{
	SafeRelease(g_pPipelineWriteS_);
	SafeRelease(g_pPipelineUseS_);

	SafeRelease(g_pRootSig_);

	g_VShader_.Destroy();
	g_PShader_.Destroy();

	g_SamplerDesc_.Destroy();
	g_TextureDesc_.Destroy();
	SafeRelease(g_pTexture_);

	SafeRelease(g_pVB0_);
	SafeRelease(g_pVB1_);
	SafeRelease(g_pVB2_);
	SafeRelease(g_pIB_);

	for (auto& v : g_CBSceneDescs_) v.Destroy();
	for (auto& v : g_pCBScenes_)
	{
		v->Unmap(0, nullptr);
		SafeRelease(v);
	}

	g_DepthBufferDesc_.Destroy();
	SafeRelease(g_pDepthBuffer_);
}

void RenderScene()
{
	g_mainCmdList_.Reset();

	ID3D12Resource* rtRes = g_Device_.GetSwapchain().GetCurrentRenderTarget();
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_Device_.GetSwapchain().GetCurrentDescHandle();
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = g_DepthBufferDesc_.GetCpuHandle();
	ID3D12GraphicsCommandList* pCmdList = g_mainCmdList_.GetCommandList();

	{
		D3D12_RESOURCE_BARRIER barrier;
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = rtRes;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		pCmdList->ResourceBarrier(1, &barrier);
	}

	// 画面クリア
	const float kClearColor[] = { 0.0f, 0.0f, 0.6f, 1.0f };
	pCmdList->ClearRenderTargetView(rtvHandle, kClearColor, 0, nullptr);
	pCmdList->ClearDepthStencilView(g_DepthBufferDesc_.GetCpuHandle(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// レンダーターゲット設定
	pCmdList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

	// Viewport + Scissor設定
	D3D12_VIEWPORT viewport{ 0.0f, 0.0f, (float)kWindowWidth, (float)kWindowHeight, 0.0f, 1.0f };
	D3D12_RECT scissor{ 0, 0, kWindowWidth, kWindowHeight };
	pCmdList->RSSetViewports(1, &viewport);
	pCmdList->RSSetScissorRects(1, &scissor);

	// Scene定数バッファを更新
	int32_t frameIndex = g_Device_.GetFrameIndex();
	Descriptor& cbSceneDesc0 = g_CBSceneDescs_[frameIndex];
	Descriptor& cbSceneDesc1 = g_CBSceneDescs_[frameIndex + 2];
	{
		static float sAngle = 0.0f;
		void* p0 = g_pCBSceneBuffers_[frameIndex];
		DirectX::XMFLOAT4X4* pMtxs = reinterpret_cast<DirectX::XMFLOAT4X4*>(p0);
		DirectX::XMMATRIX mtxW = DirectX::XMMatrixRotationY(sAngle * DirectX::XM_PI / 180.0f);
		DirectX::FXMVECTOR eye = DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(0.0f, 5.0f, 10.0f));
		DirectX::FXMVECTOR focus = DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
		DirectX::FXMVECTOR up = DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f));
		DirectX::XMMATRIX mtxV = DirectX::XMMatrixLookAtRH(eye, focus, up);
		DirectX::XMMATRIX mtxP = DirectX::XMMatrixPerspectiveFovRH(60.0f * DirectX::XM_PI / 180.0f, (float)kWindowWidth / (float)kWindowHeight, 0.1f, 100.0f);
		DirectX::XMStoreFloat4x4(pMtxs + 0, mtxW);
		DirectX::XMStoreFloat4x4(pMtxs + 1, mtxV);
		DirectX::XMStoreFloat4x4(pMtxs + 2, mtxP);

		void* p1 = g_pCBSceneBuffers_[frameIndex + 2];
		memcpy(p1, p0, sizeof(DirectX::XMFLOAT4X4) * 3);
		pMtxs = reinterpret_cast<DirectX::XMFLOAT4X4*>(p1);
		mtxW = DirectX::XMMatrixTranslation(0.5f, 0.0f, 0.5f);
		DirectX::XMStoreFloat4x4(pMtxs + 0, mtxW);

		sAngle += 1.0f;
	}

	{
		// PSO設定
		pCmdList->SetPipelineState(g_pPipelineWriteS_);
		pCmdList->OMSetStencilRef(0xf);

		// ルートシグネチャを設定
		pCmdList->SetGraphicsRootSignature(g_pRootSig_);

		// DescriptorHeapを設定
		ID3D12DescriptorHeap* pDescHeaps[] = {
			g_DescHeaps_[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].GetHeap(),
			g_DescHeaps_[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].GetHeap()
		};
		pCmdList->SetDescriptorHeaps(_countof(pDescHeaps), pDescHeaps);
		pCmdList->SetGraphicsRootDescriptorTable(0, cbSceneDesc0.GetGpuHandle());
		pCmdList->SetGraphicsRootDescriptorTable(1, g_TextureDesc_.GetGpuHandle());
		pCmdList->SetGraphicsRootDescriptorTable(2, g_SamplerDesc_.GetGpuHandle());

		// DrawCall
		D3D12_VERTEX_BUFFER_VIEW views[] = { g_VB0View_, g_VB1View_, g_VB2View_ };
		pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCmdList->IASetVertexBuffers(0, _countof(views), views);
		pCmdList->IASetIndexBuffer(&g_IBView_);
		pCmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
	}

	{
		// PSO設定
		pCmdList->SetPipelineState(g_pPipelineUseS_);
		pCmdList->OMSetStencilRef(0xf);

		// ルートシグネチャを設定
		pCmdList->SetGraphicsRootSignature(g_pRootSig_);

		// DescriptorHeapを設定
		ID3D12DescriptorHeap* pDescHeaps[] = {
			g_DescHeaps_[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].GetHeap(),
			g_DescHeaps_[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].GetHeap()
		};
		pCmdList->SetDescriptorHeaps(_countof(pDescHeaps), pDescHeaps);
		pCmdList->SetGraphicsRootDescriptorTable(0, cbSceneDesc1.GetGpuHandle());
		pCmdList->SetGraphicsRootDescriptorTable(1, g_TextureDesc_.GetGpuHandle());
		pCmdList->SetGraphicsRootDescriptorTable(2, g_SamplerDesc_.GetGpuHandle());

		// DrawCall
		D3D12_VERTEX_BUFFER_VIEW views[] = { g_VB0View_, g_VB1View_, g_VB2View_ };
		pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCmdList->IASetVertexBuffers(0, _countof(views), views);
		pCmdList->IASetIndexBuffer(&g_IBView_);
		pCmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
	}

	{
		D3D12_RESOURCE_BARRIER barrier;
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = rtRes;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		pCmdList->ResourceBarrier(1, &barrier);
	}

	g_mainCmdList_.Close();
	g_mainCmdList_.Execute();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	InitWindow(hInstance, nCmdShow);

	auto ret = g_Device_.Initialize(g_hWnd_, kWindowWidth, kWindowHeight);
	assert(ret);
	ret = g_mainCmdList_.Initialize(g_Device_.GetDevice(), &g_Device_.GetGraphicsQueue());
	assert(ret);
	{
		std::array<uint32_t, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> kDescNums
		{ 100, 100, 20, 10 };
		std::array<D3D12_DESCRIPTOR_HEAP_FLAGS, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> kFlags
		{ D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, D3D12_DESCRIPTOR_HEAP_FLAG_NONE };
		for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.NumDescriptors = kDescNums[i];
			desc.Type = (D3D12_DESCRIPTOR_HEAP_TYPE)i;
			desc.Flags = kFlags[i];
			ret = g_DescHeaps_[i].Initialize(g_Device_.GetDevice(), desc);
			assert(ret);
		}
	}
	ret = InitializeAssets();
	assert(ret);

	// メインループ
	MSG msg = { 0 };
	while (true)
	{
		// Process any messages in the queue.
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
				break;
		}

		RenderScene();
		g_Device_.Present();
		g_Device_.WaitDrawDone();
	}

	g_Device_.WaitDrawDone();
	DestroyAssets();
	for (auto& dh : g_DescHeaps_)
	{
		dh.Destroy();
	}
	g_mainCmdList_.Destroy();
	g_Device_.Destroy();

	return static_cast<char>(msg.wParam);
}
