#include <sl12/device.h>
#include <sl12/swapchain.h>
#include <sl12/command_queue.h>
#include <sl12/command_list.h>
#include <sl12/descriptor_heap.h>
#include <sl12/descriptor.h>
#include <sl12/texture.h>
#include <sl12/texture_view.h>
#include <sl12/sampler.h>
#include <sl12/fence.h>
#include <sl12/buffer.h>
#include <sl12/buffer_view.h>
#include <sl12/shader.h>
#include <sl12/gui.h>
#include <DirectXTex.h>
#include <windowsx.h>

#include "file.h"


namespace
{
	struct FFTKind
	{
		enum Type
		{
			X,
			Y,
			InvX,
			InvY,

			Max
		};
	};

	struct FFTTarget
	{
		static const int	kMaxBuffer = 8;

		sl12::Texture				tex_[kMaxBuffer];
		sl12::TextureView			srv_[kMaxBuffer];
		sl12::UnorderedAccessView	uav_[kMaxBuffer];

		void Destroy()
		{
			for (auto&& v : uav_) v.Destroy();
			for (auto&& v : srv_) v.Destroy();
			for (auto&& v : tex_) v.Destroy();
		}
	};	// struct FFTTarget

	struct TextureSet
	{
		sl12::Texture			tex_;
		sl12::TextureView		srv_;

		void Destroy()
		{
			srv_.Destroy();
			tex_.Destroy();
		}
	};	// struct TextureSet

	static const wchar_t* kWindowTitle = L"D3D12Sample";
	static const int kWindowWidth = 1920;
	static const int kWindowHeight = 1080;
	//static const DXGI_FORMAT	kDepthBufferFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
	//static const DXGI_FORMAT	kDepthViewFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	static const DXGI_FORMAT	kDepthBufferFormat = DXGI_FORMAT_R32_TYPELESS;
	static const DXGI_FORMAT	kDepthViewFormat = DXGI_FORMAT_D32_FLOAT;
	static const int kMaxFrameCount = sl12::Swapchain::kMaxBuffer;
	static const int kMaxComputeCmdList = 10;

	HWND	g_hWnd_;

	sl12::Device		g_Device_;
	sl12::CommandList	g_mainCmdLists_[kMaxFrameCount];
	sl12::CommandList	g_computeCmdLists_[kMaxComputeCmdList];
	sl12::CommandList	g_copyCmdList_;

	sl12::Texture			g_DepthBuffer_;
	sl12::DepthStencilView	g_DepthBufferView_;

	TextureSet					g_srcTexture_;
	TextureSet					g_filterTexture_;
	FFTTarget					g_srcTarget_;
	FFTTarget					g_filterTarget_;

	sl12::Fence					g_FFTFence_;

	sl12::Buffer				g_CBScenes_[kMaxFrameCount];
	void*						g_pCBSceneBuffers_[kMaxFrameCount] = { nullptr };
	sl12::ConstantBufferView	g_CBSceneViews_[kMaxFrameCount];

	sl12::Buffer			g_vbuffers_[3];
	sl12::VertexBufferView	g_vbufferViews_[3];

	sl12::Buffer			g_ibuffer_;
	sl12::IndexBufferView	g_ibufferView_;

	sl12::Sampler			g_sampler_;

	sl12::Shader			g_VShader_, g_PShader_;
	sl12::Shader			g_FFTViewShader_;
	sl12::Shader			g_FFTShaders_[FFTKind::Max];
	sl12::Shader			g_FFTMulShader_;

	ID3D12RootSignature*	g_pRootSigTex_ = nullptr;
	ID3D12RootSignature*	g_pRootSigFFT_ = nullptr;
	ID3D12RootSignature*	g_pComputeRootSig_ = nullptr;
	ID3D12RootSignature*	g_pComputeMulRootSig_ = nullptr;

	ID3D12PipelineState*	g_pPipelineStateTex_ = nullptr;
	ID3D12PipelineState*	g_pPipelineStateFFT_ = nullptr;
	ID3D12PipelineState*	g_pFFTPipelineStates_[4] = { nullptr };
	ID3D12PipelineState*	g_pPipelineStateMullFFT_ = nullptr;

	sl12::Gui	g_Gui_;
	sl12::InputData	g_InputData_{};

	struct FFTViewType
	{
		enum Type
		{
			Source,
			FFT_Result,
			IFFT_Result,

			Max
		};
	};
	FFTViewType::Type	g_fftViewType_ = FFTViewType::Source;
	int					g_SyncInterval = 1;

}

// FFTターゲットを初期化する
bool InitializeFFTTarget(FFTTarget* pTarget, sl12::u32 width, sl12::u32 height)
{
	sl12::TextureDesc texDesc{
		sl12::TextureDimension::Texture2D,
		width,
		height,
		1,
		1,
		DXGI_FORMAT_R32G32B32A32_FLOAT,
		1,
		{ 0.0f, 0.0f, 0.0f, 0.0f }, 1.0f, 0,
		false,
		false,
		true
	};

	for (int i = 0; i < FFTTarget::kMaxBuffer; ++i)
	{
		if (!pTarget->tex_[i].Initialize(&g_Device_, texDesc))
		{
			return false;
		}

		if (!pTarget->srv_[i].Initialize(&g_Device_, &pTarget->tex_[i]))
		{
			return false;
		}

		if (!pTarget->uav_[i].Initialize(&g_Device_, &pTarget->tex_[i]))
		{
			return false;
		}
	}

	return true;
}

// テクスチャを読み込む
bool LoadTexture(TextureSet* pTexSet, const char* filename)
{
	File texFile(filename);

	if (!pTexSet->tex_.InitializeFromTGA(&g_Device_, &g_copyCmdList_, texFile.GetData(), texFile.GetSize(), false))
	{
		return false;
	}
	if (!pTexSet->srv_.Initialize(&g_Device_, &pTexSet->tex_))
	{
		return false;
	}

	return true;
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

	case WM_LBUTTONDOWN:
		g_InputData_.mouseButton |= sl12::MouseButton::Left;
		return 0;
	case WM_RBUTTONDOWN:
		g_InputData_.mouseButton |= sl12::MouseButton::Right;
		return 0;
	case WM_MBUTTONDOWN:
		g_InputData_.mouseButton |= sl12::MouseButton::Middle;
		return 0;
	case WM_LBUTTONUP:
		g_InputData_.mouseButton &= ~sl12::MouseButton::Left;
		return 0;
	case WM_RBUTTONUP:
		g_InputData_.mouseButton &= ~sl12::MouseButton::Right;
		return 0;
	case WM_MBUTTONUP:
		g_InputData_.mouseButton &= ~sl12::MouseButton::Middle;
		return 0;
	case WM_MOUSEMOVE:
		g_InputData_.mouseX = GET_X_LPARAM(lParam);
		g_InputData_.mouseY = GET_Y_LPARAM(lParam);
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

bool InitializeAssets()
{
	ID3D12Device* pDev = g_Device_.GetDeviceDep();

	// 深度バッファを作成
	{
		sl12::TextureDesc texDesc{
			sl12::TextureDimension::Texture2D,
			kWindowWidth,
			kWindowHeight,
			1,
			1,
			kDepthViewFormat,
			1,
			{ 0.0f, 0.0f, 0.0f, 0.0f }, 1.0f, 0,
			false,
			true,
			false
		};
		if (!g_DepthBuffer_.Initialize(&g_Device_, texDesc))
		{
			return false;
		}

		if (!g_DepthBufferView_.Initialize(&g_Device_, &g_DepthBuffer_))
		{
			return false;
		}
	}

	if (!g_FFTFence_.Initialize(&g_Device_))
	{
		return false;
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

		for (int i = 0; i < _countof(g_CBScenes_); i++)
		{
			if (!g_CBScenes_[i].Initialize(&g_Device_, sizeof(DirectX::XMFLOAT4X4) * 3, 1, sl12::BufferUsage::ConstantBuffer, true, false))
			{
				return false;
			}

			if (!g_CBSceneViews_[i].Initialize(&g_Device_, &g_CBScenes_[i]))
			{
				return false;
			}

			g_pCBSceneBuffers_[i] = g_CBScenes_[i].Map(&g_mainCmdLists_[i]);
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

			if (!g_vbuffers_[0].Initialize(&g_Device_, sizeof(positions), sizeof(float) * 3, sl12::BufferUsage::VertexBuffer, false, false))
			{
				return false;
			}
			if (!g_vbufferViews_[0].Initialize(&g_Device_, &g_vbuffers_[0]))
			{
				return false;
			}

			g_vbuffers_[0].UpdateBuffer(&g_Device_, &g_copyCmdList_, positions, sizeof(positions));
		}
		{
			uint32_t colors[] = {
				0xff0000ff, 0xff00ff00, 0xffff0000, 0xffffffff
			};

			if (!g_vbuffers_[1].Initialize(&g_Device_, sizeof(colors), sizeof(sl12::u32), sl12::BufferUsage::VertexBuffer, false, false))
			{
				return false;
			}
			if (!g_vbufferViews_[1].Initialize(&g_Device_, &g_vbuffers_[1]))
			{
				return false;
			}

			g_vbuffers_[1].UpdateBuffer(&g_Device_, &g_copyCmdList_, colors, sizeof(colors));
		}
		{
			float uvs[] = {
				0.0f, 0.0f,
				1.0f, 0.0f,
				0.0f, 1.0f,
				1.0f, 1.0f
			};

			if (!g_vbuffers_[2].Initialize(&g_Device_, sizeof(uvs), sizeof(float) * 2, sl12::BufferUsage::VertexBuffer, false, false))
			{
				return false;
			}
			if (!g_vbufferViews_[2].Initialize(&g_Device_, &g_vbuffers_[2]))
			{
				return false;
			}

			g_vbuffers_[2].UpdateBuffer(&g_Device_, &g_copyCmdList_, uvs, sizeof(uvs));
		}
	}
	// インデックスバッファを作成
	{
		uint32_t indices[] = {
			0, 1, 2, 1, 3, 2
		};

		if (!g_ibuffer_.Initialize(&g_Device_, sizeof(indices), sizeof(sl12::u32), sl12::BufferUsage::IndexBuffer, false, false))
		{
			return false;
		}
		if (!g_ibufferView_.Initialize(&g_Device_, &g_ibuffer_))
		{
			return false;
		}

		g_ibuffer_.UpdateBuffer(&g_Device_, &g_copyCmdList_, indices, sizeof(indices));
	}

	// テクスチャロード
	if (!LoadTexture(&g_srcTexture_, "data/woman.tga"))
	{
		return false;
	}
	if (!LoadTexture(&g_filterTexture_, "data/kernel04.tga"))
	{
		return false;
	}

	// FFTターゲット初期化
	if (!InitializeFFTTarget(&g_srcTarget_, g_srcTexture_.tex_.GetTextureDesc().width, g_srcTexture_.tex_.GetTextureDesc().height))
	{
		return false;
	}
	if (!InitializeFFTTarget(&g_filterTarget_, g_filterTexture_.tex_.GetTextureDesc().width, g_filterTexture_.tex_.GetTextureDesc().height))
	{
		return false;
	}

	// サンプラ作成
	{
		D3D12_SAMPLER_DESC desc{};
		desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		if (!g_sampler_.Initialize(&g_Device_, desc))
		{
			return false;
		}
	}

	// シェーダロード
	if (!g_VShader_.Initialize(&g_Device_, sl12::ShaderType::Vertex, "data/VSSample.cso"))
	{
		return false;
	}
	if (!g_PShader_.Initialize(&g_Device_, sl12::ShaderType::Pixel, "data/PSSample.cso"))
	{
		return false;
	}
	if (!g_FFTViewShader_.Initialize(&g_Device_, sl12::ShaderType::Pixel, "data/PSFFTView.cso"))
	{
		return false;
	}
	int i = 0;
	static const char* kFFTShaderNames[] = {
		"data/CSFFTx.cso",
		"data/CSFFTy.cso",
		"data/CSIFFTx.cso",
		"data/CSIFFTy.cso",
	};
	for (auto& v : g_FFTShaders_)
	{
		if (!v.Initialize(&g_Device_, sl12::ShaderType::Compute, kFFTShaderNames[i++]))
		{
			return false;
		}
	}
	if (!g_FFTMulShader_.Initialize(&g_Device_, sl12::ShaderType::Compute, "data/CSFFTMul.cso"))
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
			sl12::SafeRelease(pSignature);
			sl12::SafeRelease(pError);
			return false;
		}

		hr = pDev->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&g_pRootSigTex_));
		sl12::SafeRelease(pSignature);
		sl12::SafeRelease(pError);
		if (FAILED(hr))
		{
			return false;
		}
	}
	{
		D3D12_DESCRIPTOR_RANGE ranges[]{
			{ D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
			{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
			{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
			{ D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
		};
		D3D12_ROOT_PARAMETER rootParameters[]{
			{ D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX },
			{ D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL },
			{ D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL },
			{ D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 1, &ranges[3], D3D12_SHADER_VISIBILITY_PIXEL },
		};

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
			sl12::SafeRelease(pSignature);
			sl12::SafeRelease(pError);
			return false;
		}

		hr = pDev->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&g_pRootSigFFT_));
		sl12::SafeRelease(pSignature);
		sl12::SafeRelease(pError);
		if (FAILED(hr))
		{
			return false;
		}
	}
	{
		D3D12_DESCRIPTOR_RANGE ranges[4];
		D3D12_ROOT_PARAMETER rootParameters[4];

		for (int i = 0; i < _countof(ranges); i++)
		{
			ranges[i].RangeType = (i < 2) ? D3D12_DESCRIPTOR_RANGE_TYPE_SRV : D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			ranges[i].NumDescriptors = 1;
			ranges[i].BaseShaderRegister = i % 2;
			ranges[i].RegisterSpace = 0;
			ranges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			rootParameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameters[i].DescriptorTable.NumDescriptorRanges = 1;
			rootParameters[i].DescriptorTable.pDescriptorRanges = &ranges[i];
			rootParameters[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		}

		D3D12_ROOT_SIGNATURE_DESC desc;
		desc.NumParameters = _countof(rootParameters);
		desc.pParameters = rootParameters;
		desc.NumStaticSamplers = 0;
		desc.pStaticSamplers = nullptr;
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		ID3DBlob* pSignature{ nullptr };
		ID3DBlob* pError{ nullptr };
		auto hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError);
		if (FAILED(hr))
		{
			sl12::SafeRelease(pSignature);
			sl12::SafeRelease(pError);
			return false;
		}

		hr = pDev->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&g_pComputeRootSig_));
		sl12::SafeRelease(pSignature);
		sl12::SafeRelease(pError);
		if (FAILED(hr))
		{
			return false;
		}
	}
	{
		D3D12_DESCRIPTOR_RANGE ranges[6];
		D3D12_ROOT_PARAMETER rootParameters[6];

		for (int i = 0; i < _countof(ranges); i++)
		{
			ranges[i].RangeType = (i < 4) ? D3D12_DESCRIPTOR_RANGE_TYPE_SRV : D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			ranges[i].NumDescriptors = 1;
			ranges[i].BaseShaderRegister = i % 4;
			ranges[i].RegisterSpace = 0;
			ranges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			rootParameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameters[i].DescriptorTable.NumDescriptorRanges = 1;
			rootParameters[i].DescriptorTable.pDescriptorRanges = &ranges[i];
			rootParameters[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		}

		D3D12_ROOT_SIGNATURE_DESC desc;
		desc.NumParameters = _countof(rootParameters);
		desc.pParameters = rootParameters;
		desc.NumStaticSamplers = 0;
		desc.pStaticSamplers = nullptr;
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		ID3DBlob* pSignature{ nullptr };
		ID3DBlob* pError{ nullptr };
		auto hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError);
		if (FAILED(hr))
		{
			sl12::SafeRelease(pSignature);
			sl12::SafeRelease(pError);
			return false;
		}

		hr = pDev->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&g_pComputeMulRootSig_));
		sl12::SafeRelease(pSignature);
		sl12::SafeRelease(pError);
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
		dsDesc.DepthEnable = true;
		dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		dsDesc.StencilEnable = false;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
		desc.InputLayout = { elementDescs, _countof(elementDescs) };
		desc.pRootSignature = g_pRootSigTex_;
		desc.VS = { g_VShader_.GetData(), g_VShader_.GetSize() };
		desc.PS = { g_PShader_.GetData(), g_PShader_.GetSize() };
		desc.RasterizerState = rasterDesc;
		desc.BlendState = blendDesc;
		desc.DepthStencilState = dsDesc;
		desc.SampleMask = UINT_MAX;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.DSVFormat = g_DepthBuffer_.GetTextureDesc().format;
		desc.SampleDesc.Count = 1;

		auto hr = pDev->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&g_pPipelineStateTex_));
		if (FAILED(hr))
		{
			return false;
		}

		// FFT確認用
		desc.pRootSignature = g_pRootSigFFT_;
		desc.PS = { g_FFTViewShader_.GetData(), g_FFTViewShader_.GetSize() };

		hr = pDev->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&g_pPipelineStateFFT_));
		if (FAILED(hr))
		{
			return false;
		}
	}
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = g_pComputeRootSig_;
		for (int i = 0; i < _countof(g_pFFTPipelineStates_); i++)
		{
			desc.CS = { g_FFTShaders_[i].GetData(), g_FFTShaders_[i].GetSize() };
			auto hr = pDev->CreateComputePipelineState(&desc, IID_PPV_ARGS(&g_pFFTPipelineStates_[i]));
			if (FAILED(hr))
			{
				return false;
			}
		}
	}
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = g_pComputeMulRootSig_;
		desc.CS = { g_FFTMulShader_.GetData(), g_FFTMulShader_.GetSize() };
		auto hr = pDev->CreateComputePipelineState(&desc, IID_PPV_ARGS(&g_pPipelineStateMullFFT_));
		if (FAILED(hr))
		{
			return false;
		}
	}

	// GUIの初期化
	if (!g_Gui_.Initialize(&g_Device_, DXGI_FORMAT_R8G8B8A8_UNORM, g_DepthBuffer_.GetTextureDesc().format))
	{
		return false;
	}
	if (!g_Gui_.CreateFontImage(&g_Device_, g_copyCmdList_))
	{
		return false;
	}

	return true;
}

void DestroyAssets()
{
	g_Gui_.Destroy();

	sl12::SafeRelease(g_pPipelineStateMullFFT_);
	for (auto& v : g_pFFTPipelineStates_)
	{
		sl12::SafeRelease(v);
	}
	sl12::SafeRelease(g_pPipelineStateTex_);
	sl12::SafeRelease(g_pPipelineStateFFT_);

	sl12::SafeRelease(g_pComputeMulRootSig_);
	sl12::SafeRelease(g_pComputeRootSig_);
	sl12::SafeRelease(g_pRootSigTex_);
	sl12::SafeRelease(g_pRootSigFFT_);

	g_srcTarget_.Destroy();
	g_filterTarget_.Destroy();
	g_srcTexture_.Destroy();
	g_filterTexture_.Destroy();

	for (auto& v : g_FFTShaders_)
	{
		v.Destroy();
	}
	g_VShader_.Destroy();
	g_PShader_.Destroy();

	g_sampler_.Destroy();

	for (auto& v : g_vbufferViews_)
	{
		v.Destroy();
	}
	for (auto& v : g_vbuffers_)
	{
		v.Destroy();
	}

	g_ibufferView_.Destroy();
	g_ibuffer_.Destroy();

	for (auto& v : g_CBSceneViews_)
	{
		v.Destroy();
	}
	for (auto& v : g_CBScenes_)
	{
		v.Unmap();
		v.Destroy();
	}

	g_FFTFence_.Destroy();

	g_DepthBufferView_.Destroy();
	g_DepthBuffer_.Destroy();
}

void MakeFFT(sl12::CommandList& cmdList, FFTTarget* pTarget, TextureSet* pTex)
{
	ID3D12GraphicsCommandList* pCmdList = cmdList.GetCommandList();

	// デスクリプタヒープの設定
	ID3D12DescriptorHeap* pDescHeaps[] = {
		g_Device_.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetHeap(),
		g_Device_.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER).GetHeap()
	};
	pCmdList->SetDescriptorHeaps(_countof(pDescHeaps), pDescHeaps);

	// row pass
	pCmdList->SetPipelineState(g_pFFTPipelineStates_[0]);
	pCmdList->SetComputeRootSignature(g_pComputeRootSig_);
	pCmdList->SetComputeRootDescriptorTable(0, pTex->srv_.GetDesc()->GetGpuHandle());
	pCmdList->SetComputeRootDescriptorTable(1, pTex->srv_.GetDesc()->GetGpuHandle());
	pCmdList->SetComputeRootDescriptorTable(2, pTarget->uav_[0].GetDesc()->GetGpuHandle());
	pCmdList->SetComputeRootDescriptorTable(3, pTarget->uav_[1].GetDesc()->GetGpuHandle());
	pCmdList->Dispatch(1, pTex->tex_.GetTextureDesc().height, 1);

	// バリアを張る
	cmdList.UAVBarrier(&pTarget->tex_[0]);
	cmdList.UAVBarrier(&pTarget->tex_[1]);

	// collumn pass
	pCmdList->SetPipelineState(g_pFFTPipelineStates_[1]);
	pCmdList->SetComputeRootSignature(g_pComputeRootSig_);
	pCmdList->SetComputeRootDescriptorTable(0, pTarget->uav_[0].GetDesc()->GetGpuHandle());
	pCmdList->SetComputeRootDescriptorTable(1, pTarget->uav_[1].GetDesc()->GetGpuHandle());
	pCmdList->SetComputeRootDescriptorTable(2, pTarget->uav_[2].GetDesc()->GetGpuHandle());
	pCmdList->SetComputeRootDescriptorTable(3, pTarget->uav_[3].GetDesc()->GetGpuHandle());
	pCmdList->Dispatch(1, pTex->tex_.GetTextureDesc().width, 1);

	// バリアを張る
	cmdList.UAVBarrier(&pTarget->tex_[2]);
	cmdList.UAVBarrier(&pTarget->tex_[3]);
}

void MakeMulFFT(sl12::CommandList& cmdList, FFTTarget* pTarget, FFTTarget* pFilter, TextureSet* pFilterTex)
{
	ID3D12GraphicsCommandList* pCmdList = cmdList.GetCommandList();

	// デスクリプタヒープの設定
	ID3D12DescriptorHeap* pDescHeaps[] = {
		g_Device_.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetHeap(),
		g_Device_.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER).GetHeap()
	};
	pCmdList->SetDescriptorHeaps(_countof(pDescHeaps), pDescHeaps);

	auto desc = pTarget->tex_[0].GetTextureDesc();

	// row pass
	pCmdList->SetPipelineState(g_pPipelineStateMullFFT_);
	pCmdList->SetComputeRootSignature(g_pComputeMulRootSig_);
	pCmdList->SetComputeRootDescriptorTable(0, pTarget->srv_[2].GetDesc()->GetGpuHandle());
	pCmdList->SetComputeRootDescriptorTable(1, pTarget->srv_[3].GetDesc()->GetGpuHandle());
	pCmdList->SetComputeRootDescriptorTable(2, pFilterTex->srv_.GetDesc()->GetGpuHandle());
	//pCmdList->SetComputeRootDescriptorTable(2, pFilter->srv_[2].GetDesc()->GetGpuHandle());
	pCmdList->SetComputeRootDescriptorTable(3, pFilter->srv_[3].GetDesc()->GetGpuHandle());
	pCmdList->SetComputeRootDescriptorTable(4, pTarget->uav_[6].GetDesc()->GetGpuHandle());
	pCmdList->SetComputeRootDescriptorTable(5, pTarget->uav_[7].GetDesc()->GetGpuHandle());
	pCmdList->Dispatch(desc.width / 32, desc.height / 32, 1);

	// バリアを張る
	cmdList.UAVBarrier(&pTarget->tex_[6]);
	cmdList.UAVBarrier(&pTarget->tex_[7]);
}

void MakeIFFT(sl12::CommandList& cmdList, FFTTarget* pTarget, TextureSet* pTex)
{
	ID3D12GraphicsCommandList* pCmdList = cmdList.GetCommandList();

	// デスクリプタヒープの設定
	ID3D12DescriptorHeap* pDescHeaps[] = {
		g_Device_.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetHeap(),
		g_Device_.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER).GetHeap()
	};
	pCmdList->SetDescriptorHeaps(_countof(pDescHeaps), pDescHeaps);

	// invert row pass
	pCmdList->SetPipelineState(g_pFFTPipelineStates_[2]);
	pCmdList->SetComputeRootSignature(g_pComputeRootSig_);
	pCmdList->SetComputeRootDescriptorTable(0, pTarget->uav_[6].GetDesc()->GetGpuHandle());
	pCmdList->SetComputeRootDescriptorTable(1, pTarget->uav_[7].GetDesc()->GetGpuHandle());
	pCmdList->SetComputeRootDescriptorTable(2, pTarget->uav_[0].GetDesc()->GetGpuHandle());
	pCmdList->SetComputeRootDescriptorTable(3, pTarget->uav_[1].GetDesc()->GetGpuHandle());
	pCmdList->Dispatch(1, pTex->tex_.GetTextureDesc().height, 1);

	// バリアを張る
	cmdList.UAVBarrier(&pTarget->tex_[0]);
	cmdList.UAVBarrier(&pTarget->tex_[1]);

	// invert collumn pass
	pCmdList->SetPipelineState(g_pFFTPipelineStates_[3]);
	pCmdList->SetComputeRootSignature(g_pComputeRootSig_);
	pCmdList->SetComputeRootDescriptorTable(0, pTarget->uav_[0].GetDesc()->GetGpuHandle());
	pCmdList->SetComputeRootDescriptorTable(1, pTarget->uav_[1].GetDesc()->GetGpuHandle());
	pCmdList->SetComputeRootDescriptorTable(2, pTarget->uav_[4].GetDesc()->GetGpuHandle());
	pCmdList->SetComputeRootDescriptorTable(3, pTarget->uav_[5].GetDesc()->GetGpuHandle());
	pCmdList->Dispatch(1, pTex->tex_.GetTextureDesc().width, 1);

	// バリアを張る
	cmdList.UAVBarrier(&pTarget->tex_[4]);
	cmdList.UAVBarrier(&pTarget->tex_[5]);
}

void RenderScene()
{
	static int sFFTCalcLoop = 1000;

	sl12::s32 frameIndex = g_Device_.GetSwapchain().GetFrameIndex();
	sl12::s32 nextFrameIndex = (frameIndex + 1) % sl12::Swapchain::kMaxBuffer;

	sl12::CommandList& mainCmdList = g_mainCmdLists_[frameIndex];

	g_Gui_.BeginNewFrame(&mainCmdList, kWindowWidth, kWindowHeight, g_InputData_);

	{
		const char* kViewNames[] = {
			"Source",
			"FFT Result",
			"IFFT Result",
		};
		auto currentItem = (int)g_fftViewType_;
		if (ImGui::Combo("View", &currentItem, kViewNames, _countof(kViewNames)))
		{
			g_fftViewType_ = (FFTViewType::Type)currentItem;
		}

		bool isSyncInterval = g_SyncInterval == 1;
		if (ImGui::Checkbox("Sync Interval", &isSyncInterval))
		{
			g_SyncInterval = isSyncInterval ? 1 : 0;
		}
	}

	// グラフィクスコマンドロードの開始
	mainCmdList.Reset();

	// ソーステクスチャとカーネルテクスチャのFFT計算
	MakeFFT(mainCmdList, &g_srcTarget_, &g_srcTexture_);
	MakeFFT(mainCmdList, &g_filterTarget_, &g_filterTexture_);

	// FFTの乗算
	MakeMulFFT(mainCmdList, &g_srcTarget_, &g_filterTarget_, &g_filterTexture_);

	// ソースのIFFT計算
	MakeIFFT(mainCmdList, &g_srcTarget_, &g_srcTexture_);

	for (auto& v : g_vbuffers_)
	{
		mainCmdList.TransitionBarrier(&v, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	}
	mainCmdList.TransitionBarrier(&g_ibuffer_, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	auto scTex = g_Device_.GetSwapchain().GetCurrentTexture(1);
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_Device_.GetSwapchain().GetDescHandle(nextFrameIndex);
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = g_DepthBufferView_.GetDesc()->GetCpuHandle();
	ID3D12GraphicsCommandList* pCmdList = mainCmdList.GetCommandList();

	mainCmdList.TransitionBarrier(scTex, D3D12_RESOURCE_STATE_RENDER_TARGET);

	// 画面クリア
	const float kClearColor[] = { 0.0f, 0.0f, 0.6f, 1.0f };
	pCmdList->ClearRenderTargetView(rtvHandle, kClearColor, 0, nullptr);
	pCmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, g_DepthBuffer_.GetTextureDesc().clearDepth, g_DepthBuffer_.GetTextureDesc().clearStencil, 0, nullptr);

	// Viewport + Scissor設定
	D3D12_VIEWPORT viewport{ 0.0f, 0.0f, (float)kWindowWidth, (float)kWindowHeight, 0.0f, 1.0f };
	D3D12_RECT scissor{ 0, 0, kWindowWidth, kWindowHeight };
	pCmdList->RSSetViewports(1, &viewport);
	pCmdList->RSSetScissorRects(1, &scissor);

	// Scene定数バッファを更新
	sl12::Descriptor& cbSceneDesc = *g_CBSceneViews_[frameIndex].GetDesc();
	{
		static float sAngle = 0.0f;
		void* p0 = g_pCBSceneBuffers_[frameIndex];
		DirectX::XMFLOAT4X4* pMtxs = reinterpret_cast<DirectX::XMFLOAT4X4*>(p0);
		DirectX::XMMATRIX mtxW = DirectX::XMMatrixRotationY(sAngle * DirectX::XM_PI / 180.0f) * DirectX::XMMatrixScaling(4.0f, 4.0f, 1.0f);
		DirectX::FXMVECTOR eye = DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(0.0f, 0.0f, 10.0f));
		DirectX::FXMVECTOR focus = DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
		DirectX::FXMVECTOR up = DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f));
		DirectX::XMMATRIX mtxV = DirectX::XMMatrixLookAtRH(eye, focus, up);
		DirectX::XMMATRIX mtxP = DirectX::XMMatrixPerspectiveFovRH(60.0f * DirectX::XM_PI / 180.0f, (float)kWindowWidth / (float)kWindowHeight, 0.1f, 100.0f);
		DirectX::XMStoreFloat4x4(pMtxs + 0, mtxW);
		DirectX::XMStoreFloat4x4(pMtxs + 1, mtxV);
		DirectX::XMStoreFloat4x4(pMtxs + 2, mtxP);

		//sAngle += 1.0f;
	}

	{
		// レンダーターゲット設定
		pCmdList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

		// PSOとルートシグネチャを設定
		if (g_fftViewType_ != FFTViewType::FFT_Result)
		{
			pCmdList->SetPipelineState(g_pPipelineStateTex_);
			pCmdList->SetGraphicsRootSignature(g_pRootSigTex_);
		}
		else
		{
			pCmdList->SetPipelineState(g_pPipelineStateFFT_);
			pCmdList->SetGraphicsRootSignature(g_pRootSigFFT_);
		}

		// DescriptorHeapを設定
		ID3D12DescriptorHeap* pDescHeaps[] = {
			g_Device_.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetHeap(),
			g_Device_.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER).GetHeap()
		};
		pCmdList->SetDescriptorHeaps(_countof(pDescHeaps), pDescHeaps);
		pCmdList->SetGraphicsRootDescriptorTable(0, cbSceneDesc.GetGpuHandle());
		if (g_fftViewType_ == FFTViewType::Source)
		{
			// ソーステクスチャの描画
			pCmdList->SetGraphicsRootDescriptorTable(1, g_srcTexture_.srv_.GetDesc()->GetGpuHandle());
			pCmdList->SetGraphicsRootDescriptorTable(2, g_sampler_.GetDesc()->GetGpuHandle());
		}
		else if (g_fftViewType_ == FFTViewType::IFFT_Result)
		{
			// IFFTの結果の描画
			pCmdList->SetGraphicsRootDescriptorTable(1, g_srcTarget_.srv_[4].GetDesc()->GetGpuHandle());
			pCmdList->SetGraphicsRootDescriptorTable(2, g_sampler_.GetDesc()->GetGpuHandle());
		}
		else
		{
			// FFTの結果の描画
			pCmdList->SetGraphicsRootDescriptorTable(1, g_srcTarget_.srv_[2].GetDesc()->GetGpuHandle());
			pCmdList->SetGraphicsRootDescriptorTable(2, g_srcTarget_.srv_[3].GetDesc()->GetGpuHandle());
			pCmdList->SetGraphicsRootDescriptorTable(3, g_sampler_.GetDesc()->GetGpuHandle());
		}

		// DrawCall
		D3D12_VERTEX_BUFFER_VIEW views[] = { g_vbufferViews_[0].GetView(), g_vbufferViews_[1].GetView(), g_vbufferViews_[2].GetView() };
		pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCmdList->IASetVertexBuffers(0, _countof(views), views);
		pCmdList->IASetIndexBuffer(&g_ibufferView_.GetView());
		pCmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
	}

	ImGui::Render();

	mainCmdList.TransitionBarrier(scTex, D3D12_RESOURCE_STATE_PRESENT);

	mainCmdList.Close();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	InitWindow(hInstance, nCmdShow);

	std::array<uint32_t, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> kDescNums
	{ 100, 100, 20, 10 };
	auto ret = g_Device_.Initialize(g_hWnd_, kWindowWidth, kWindowHeight, kDescNums);
	assert(ret);
	for (auto& v : g_mainCmdLists_)
	{
		ret = v.Initialize(&g_Device_, &g_Device_.GetGraphicsQueue());
		assert(ret);
	}
	for (auto& v : g_computeCmdLists_)
	{
		ret = v.Initialize(&g_Device_, &g_Device_.GetComputeQueue());
		assert(ret);
	}
	assert(ret);
	ret = g_copyCmdList_.Initialize(&g_Device_, &g_Device_.GetCopyQueue());
	assert(ret);
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

		// GPUによる描画待ち
		int frameIndex = g_Device_.GetSwapchain().GetFrameIndex();
		g_Device_.WaitDrawDone();
		g_Device_.Present(g_SyncInterval);

		// 前回フレームのコマンドを次回フレームの頭で実行
		// コマンドが実行中、次のフレーム用のコマンドがロードされる
		g_mainCmdLists_[frameIndex].Execute();
	}

	g_Device_.WaitDrawDone();
	DestroyAssets();
	g_copyCmdList_.Destroy();
	for (auto& v : g_mainCmdLists_)
		v.Destroy();
	for (auto& v : g_mainCmdLists_)
		v.Destroy();
	g_Device_.Destroy();

	return static_cast<char>(msg.wParam);
}
