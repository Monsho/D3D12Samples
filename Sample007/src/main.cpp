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
#include <sl12/mesh.h>
#include <sl12/root_signature.h>
#include <sl12/pipeline_state.h>
#include <sl12/file.h>
#include <sl12/root_signature_manager.h>
#include <sl12/render_resource_manager.h>

#include <DirectXTex.h>
#include <windowsx.h>


namespace
{
	struct SceneCB
	{
		DirectX::XMFLOAT4X4		mtxWorldToView;
		DirectX::XMFLOAT4X4		mtxViewToClip;
		DirectX::XMFLOAT4X4		mtxViewToWorld;
		DirectX::XMFLOAT4		screenInfo;
		DirectX::XMFLOAT4		frustumCorner;
	};	// struct SceneCB

	struct MeshCB
	{
		DirectX::XMFLOAT4X4		mtxLocalToWorld;
	};	// struct MeshCB

	struct BlurCB
	{
		DirectX::XMFLOAT4		gaussWeights;
		DirectX::XMFLOAT2		deltaUV;
	};	// struct BlurCB

	struct ConstantSet
	{
		sl12::Buffer				cb_;
		sl12::ConstantBufferView	cbv_;
		void*						ptr_;

		void Destroy()
		{
			cbv_.Destroy();
			cb_.Destroy();
		}
	};	// struct ConstantSet

	struct ShaderKind
	{
		enum
		{
			BasePassV,
			BasePassP,
			PostProcessV,
			LinearDepthP,
			LightingP,
			BlurXP,
			BlurYP,

			Max
		};
	};	// struct ShaderKind

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

	ConstantSet				g_SceneCBs_[kMaxFrameCount];
	ConstantSet				g_MeshCB_;
	ConstantSet				g_BlurCB_;

	sl12::Sampler			g_sampler_;
	sl12::Sampler			g_samLinearClamp_;

	sl12::Shader			g_VShader_, g_PShader_;

	sl12::Shader			g_Shaders_[ShaderKind::Max];

	sl12::RootSignatureManager	g_rootSigMan_;
	sl12::RootSignatureHandle	g_basePassSig_;
	sl12::RootSignatureHandle	g_linearDepthSig_;
	sl12::RootSignatureHandle	g_lightingSig_;
	sl12::RootSignatureHandle	g_blurXPassSig_;
	sl12::RootSignatureHandle	g_blurYPassSig_;

	sl12::GraphicsPipelineState	g_basePassPso_;
	sl12::GraphicsPipelineState	g_linearDepthPso_;
	sl12::GraphicsPipelineState	g_lightingPso_;
	sl12::GraphicsPipelineState	g_blurXPassPso_;
	sl12::GraphicsPipelineState	g_blurYPassPso_;

	sl12::RootSignature			g_rootSigMesh_;
	sl12::GraphicsPipelineState	g_psoMesh_;

	sl12::File			g_meshFile_;
	sl12::MeshInstance	g_mesh_;

	struct RenderID
	{
		enum
		{
			GBuffer0,
			GBuffer1,
			GBuffer2,
			Depth,
			LightResult,
			LinearDepth,
		};
	};	// struct RenderID
	sl12::RenderResourceManager					g_rrManager_;
	std::vector<sl12::ResourceProducerBase*>	g_rrProducers_;

	sl12::Gui	g_Gui_;
	sl12::InputData	g_InputData_{};

	int					g_SyncInterval = 1;

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
		sl12::TextureDesc texDesc;
		texDesc.dimension = sl12::TextureDimension::Texture2D;
		texDesc.width = kWindowWidth;
		texDesc.height = kWindowHeight;
		texDesc.format = kDepthViewFormat;
		texDesc.isDepthBuffer = true;

		if (!g_DepthBuffer_.Initialize(&g_Device_, texDesc))
		{
			return false;
		}

		if (!g_DepthBufferView_.Initialize(&g_Device_, &g_DepthBuffer_))
		{
			return false;
		}
	}

	// 定数バッファを作成
	for (int i = 0; i < _countof(g_SceneCBs_); i++)
	{
		if (!g_SceneCBs_[i].cb_.Initialize(&g_Device_, sizeof(SceneCB), 1, sl12::BufferUsage::ConstantBuffer, true, false))
		{
			return false;
		}

		if (!g_SceneCBs_[i].cbv_.Initialize(&g_Device_, &g_SceneCBs_[i].cb_))
		{
			return false;
		}

		g_SceneCBs_[i].ptr_ = g_SceneCBs_[i].cb_.Map(&g_mainCmdLists_[i]);
	}
	{
		if (!g_MeshCB_.cb_.Initialize(&g_Device_, sizeof(MeshCB), 1, sl12::BufferUsage::ConstantBuffer, true, false))
		{
			return false;
		}

		if (!g_MeshCB_.cbv_.Initialize(&g_Device_, &g_MeshCB_.cb_))
		{
			return false;
		}

		auto p = reinterpret_cast<MeshCB*>(g_MeshCB_.cb_.Map(nullptr));
		DirectX::XMMATRIX mtx = DirectX::XMMatrixIdentity();
		DirectX::XMStoreFloat4x4(&p->mtxLocalToWorld, mtx);
		g_MeshCB_.cb_.Unmap();
	}
	{
		if (!g_BlurCB_.cb_.Initialize(&g_Device_, sizeof(BlurCB), 1, sl12::BufferUsage::ConstantBuffer, true, false))
		{
			return false;
		}

		if (!g_BlurCB_.cbv_.Initialize(&g_Device_, &g_BlurCB_.cb_))
		{
			return false;
		}

		auto p = reinterpret_cast<BlurCB*>(g_BlurCB_.cb_.Map(nullptr));
		float variance = 5.0f * 5.0f;
		p->deltaUV.x = 1.0f / (float)kWindowWidth;
		p->deltaUV.y = 1.0f / (float)kWindowHeight;

		float sum = 0.0f;
		p->gaussWeights.x = expf(-0.5f * 0.0f * 0.0f / variance);
		p->gaussWeights.y = expf(-0.5f * 1.0f * 1.0f / variance);
		p->gaussWeights.z = expf(-0.5f * 2.0f * 2.0f / variance);
		p->gaussWeights.w = expf(-0.5f * 3.0f * 3.0f / variance);
		sum = p->gaussWeights.x;
		sum += p->gaussWeights.y * 2.0f;
		sum += p->gaussWeights.z * 2.0f;
		sum += p->gaussWeights.w * 2.0f;
		p->gaussWeights.x /= sum;
		p->gaussWeights.y /= sum;
		p->gaussWeights.z /= sum;
		p->gaussWeights.w /= sum;
		g_BlurCB_.cb_.Unmap();
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

		desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		if (!g_samLinearClamp_.Initialize(&g_Device_, desc))
		{
			return false;
		}
	}

	// シェーダロード
	if (!g_VShader_.Initialize(&g_Device_, sl12::ShaderType::Vertex, "data/VSMesh.cso"))
	{
		return false;
	}
	if (!g_PShader_.Initialize(&g_Device_, sl12::ShaderType::Pixel, "data/PSMesh.cso"))
	{
		return false;
	}
	if (!g_Shaders_[ShaderKind::BasePassV].Initialize(&g_Device_, sl12::ShaderType::Vertex, "data/base_pass.vv.cso"))
	{
		return false;
	}
	if (!g_Shaders_[ShaderKind::BasePassP].Initialize(&g_Device_, sl12::ShaderType::Pixel, "data/base_pass.p.cso"))
	{
		return false;
	}
	if (!g_Shaders_[ShaderKind::PostProcessV].Initialize(&g_Device_, sl12::ShaderType::Vertex, "data/post_process.vv.cso"))
	{
		return false;
	}
	if (!g_Shaders_[ShaderKind::LinearDepthP].Initialize(&g_Device_, sl12::ShaderType::Pixel, "data/linear_depth.p.cso"))
	{
		return false;
	}
	if (!g_Shaders_[ShaderKind::LightingP].Initialize(&g_Device_, sl12::ShaderType::Pixel, "data/lighting.p.cso"))
	{
		return false;
	}
	if (!g_Shaders_[ShaderKind::BlurXP].Initialize(&g_Device_, sl12::ShaderType::Pixel, "data/blur_x.p.cso"))
	{
		return false;
	}
	if (!g_Shaders_[ShaderKind::BlurYP].Initialize(&g_Device_, sl12::ShaderType::Pixel, "data/blur_y.p.cso"))
	{
		return false;
	}

	// ルートシグネチャマネージャの初期化
	if (!g_rootSigMan_.Initialize(&g_Device_))
	{
		return false;
	}

	// ルートシグネチャを生成
	{
		sl12::RootSignatureCreateDesc desc;

		desc.pVS = &g_Shaders_[ShaderKind::BasePassV];
		desc.pPS = &g_Shaders_[ShaderKind::BasePassP];
		g_basePassSig_ = g_rootSigMan_.CreateRootSignature(desc);

		desc.pVS = &g_Shaders_[ShaderKind::PostProcessV];
		desc.pPS = &g_Shaders_[ShaderKind::LinearDepthP];
		g_linearDepthSig_ = g_rootSigMan_.CreateRootSignature(desc);

		desc.pPS = &g_Shaders_[ShaderKind::LightingP];
		g_lightingSig_ = g_rootSigMan_.CreateRootSignature(desc);

		desc.pPS = &g_Shaders_[ShaderKind::BlurXP];
		g_blurXPassSig_ = g_rootSigMan_.CreateRootSignature(desc);

		desc.pPS = &g_Shaders_[ShaderKind::BlurYP];
		g_blurYPassSig_ = g_rootSigMan_.CreateRootSignature(desc);
	}

	// PSOを生成
	{
		sl12::GraphicsPipelineStateDesc desc;
		desc.pRootSignature = g_basePassSig_.GetRootSignature();
		desc.pVS = &g_Shaders_[ShaderKind::BasePassV];
		desc.pPS = &g_Shaders_[ShaderKind::BasePassP];

		desc.blend.sampleMask = UINT_MAX;
		desc.blend.rtDesc[0].isBlendEnable = false;
		desc.blend.rtDesc[0].writeMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		desc.rasterizer.cullMode = D3D12_CULL_MODE_BACK;
		desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
		desc.rasterizer.isDepthClipEnable = true;
		desc.rasterizer.isFrontCCW = true;

		desc.depthStencil.isDepthEnable = true;
		desc.depthStencil.isDepthWriteEnable = true;
		desc.depthStencil.depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		D3D12_INPUT_ELEMENT_DESC inputElem[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		desc.inputLayout.numElements = _countof(inputElem);
		desc.inputLayout.pElements = inputElem;

		desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		desc.numRTVs = 0;
		desc.rtvFormats[desc.numRTVs++] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		desc.rtvFormats[desc.numRTVs++] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.rtvFormats[desc.numRTVs++] = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.dsvFormat = DXGI_FORMAT_D32_FLOAT;
		desc.multisampleCount = 1;

		if (!g_basePassPso_.Initialize(&g_Device_, desc))
		{
			return false;
		}
	}
	{
		sl12::GraphicsPipelineStateDesc desc;
		desc.pRootSignature = g_linearDepthSig_.GetRootSignature();
		desc.pVS = &g_Shaders_[ShaderKind::PostProcessV];
		desc.pPS = &g_Shaders_[ShaderKind::LinearDepthP];

		desc.blend.sampleMask = UINT_MAX;
		desc.blend.rtDesc[0].isBlendEnable = false;
		desc.blend.rtDesc[0].writeMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		desc.rasterizer.cullMode = D3D12_CULL_MODE_NONE;
		desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
		desc.rasterizer.isDepthClipEnable = false;
		desc.rasterizer.isFrontCCW = true;

		desc.depthStencil.isDepthEnable = false;
		desc.depthStencil.isDepthWriteEnable = false;

		desc.inputLayout.numElements = 0;
		desc.inputLayout.pElements = nullptr;

		desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		desc.numRTVs = 0;
		desc.rtvFormats[desc.numRTVs++] = DXGI_FORMAT_R32_FLOAT;
		desc.dsvFormat = DXGI_FORMAT_UNKNOWN;
		desc.multisampleCount = 1;

		if (!g_linearDepthPso_.Initialize(&g_Device_, desc))
		{
			return false;
		}
	}
	{
		sl12::GraphicsPipelineStateDesc desc;
		desc.pRootSignature = g_lightingSig_.GetRootSignature();
		desc.pVS = &g_Shaders_[ShaderKind::PostProcessV];
		desc.pPS = &g_Shaders_[ShaderKind::LightingP];

		desc.blend.sampleMask = UINT_MAX;
		desc.blend.rtDesc[0].isBlendEnable = false;
		desc.blend.rtDesc[0].writeMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		desc.rasterizer.cullMode = D3D12_CULL_MODE_NONE;
		desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
		desc.rasterizer.isDepthClipEnable = false;
		desc.rasterizer.isFrontCCW = true;

		desc.depthStencil.isDepthEnable = false;
		desc.depthStencil.isDepthWriteEnable = false;

		desc.inputLayout.numElements = 0;
		desc.inputLayout.pElements = nullptr;

		desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		desc.numRTVs = 0;
		desc.rtvFormats[desc.numRTVs++] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		desc.dsvFormat = DXGI_FORMAT_UNKNOWN;
		desc.multisampleCount = 1;

		if (!g_lightingPso_.Initialize(&g_Device_, desc))
		{
			return false;
		}
	}
	{
		sl12::GraphicsPipelineStateDesc desc;
		desc.pRootSignature = g_blurXPassSig_.GetRootSignature();
		desc.pVS = &g_Shaders_[ShaderKind::PostProcessV];
		desc.pPS = &g_Shaders_[ShaderKind::BlurXP];

		desc.blend.sampleMask = UINT_MAX;
		desc.blend.rtDesc[0].isBlendEnable = false;
		desc.blend.rtDesc[0].writeMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		desc.rasterizer.cullMode = D3D12_CULL_MODE_NONE;
		desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
		desc.rasterizer.isDepthClipEnable = false;
		desc.rasterizer.isFrontCCW = true;

		desc.depthStencil.isDepthEnable = false;
		desc.depthStencil.isDepthWriteEnable = false;

		desc.inputLayout.numElements = 0;
		desc.inputLayout.pElements = nullptr;

		desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		desc.numRTVs = 0;
		desc.rtvFormats[desc.numRTVs++] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		desc.dsvFormat = DXGI_FORMAT_UNKNOWN;
		desc.multisampleCount = 1;

		if (!g_blurXPassPso_.Initialize(&g_Device_, desc))
		{
			return false;
		}

		desc.pRootSignature = g_blurYPassSig_.GetRootSignature();
		desc.pPS = &g_Shaders_[ShaderKind::BlurYP];
		desc.rtvFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		if (!g_blurYPassPso_.Initialize(&g_Device_, desc))
		{
			return false;
		}
	}

	// ルートシグネチャを作成
	{
		sl12::RootParameter params[] = {
			sl12::RootParameter(sl12::RootParameterType::ConstantBuffer, sl12::ShaderVisibility::Vertex, 0),
		};

		sl12::RootSignatureDesc desc;
		desc.numParameters = _countof(params);
		desc.pParameters = params;

		if (!g_rootSigMesh_.Initialize(&g_Device_, desc))
		{
			return false;
		}
	}

	// PSOを作成
	{
		sl12::GraphicsPipelineStateDesc desc;
		desc.pRootSignature = &g_rootSigMesh_;
		desc.pVS = &g_VShader_;
		desc.pPS = &g_PShader_;

		desc.blend.sampleMask = UINT_MAX;
		desc.blend.rtDesc[0].isBlendEnable = false;
		desc.blend.rtDesc[0].writeMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		desc.rasterizer.cullMode = D3D12_CULL_MODE_BACK;
		desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
		desc.rasterizer.isDepthClipEnable = true;
		desc.rasterizer.isFrontCCW = true;

		desc.depthStencil.isDepthEnable = true;
		desc.depthStencil.isDepthWriteEnable = true;
		desc.depthStencil.depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		D3D12_INPUT_ELEMENT_DESC inputElem[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		desc.inputLayout.numElements = _countof(inputElem);
		desc.inputLayout.pElements = inputElem;
		
		desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		desc.numRTVs = 1;
		desc.rtvFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.dsvFormat = DXGI_FORMAT_D32_FLOAT;
		desc.multisampleCount = 1;

		if (!g_psoMesh_.Initialize(&g_Device_, desc))
		{
			return false;
		}
	}

	// メッシュロード
	if (!g_meshFile_.ReadFile("data/sponza.mesh"))
	{
		return false;
	}
	if (!g_mesh_.Initialize(&g_Device_, &g_copyCmdList_, g_meshFile_.GetData()))
	{
		return false;
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

	g_mesh_.Destroy();
	g_meshFile_.Destroy();

	g_psoMesh_.Destroy();
	g_rootSigMesh_.Destroy();

	g_basePassPso_.Destroy();
	g_linearDepthPso_.Destroy();
	g_lightingPso_.Destroy();
	g_blurXPassPso_.Destroy();
	g_blurYPassPso_.Destroy();

	g_basePassSig_.Invalid();
	g_linearDepthSig_.Invalid();
	g_lightingSig_.Invalid();
	g_blurXPassSig_.Invalid();
	g_blurYPassSig_.Invalid();
	g_rootSigMan_.Destroy();

	g_VShader_.Destroy();
	g_PShader_.Destroy();

	g_samLinearClamp_.Destroy();
	g_sampler_.Destroy();

	g_BlurCB_.Destroy();
	g_MeshCB_.Destroy();
	for (auto&&v : g_SceneCBs_) v.Destroy();

	g_DepthBufferView_.Destroy();
	g_DepthBuffer_.Destroy();
}

bool InitializeRenderResource()
{
	// プロデューサーを生成する
	g_rrProducers_.push_back(new sl12::ResourceProducer<0, 4, 0>());		// Deferred Base Pass
	g_rrProducers_.push_back(new sl12::ResourceProducer<1, 1, 0>());		// Linear Depth Pass
	g_rrProducers_.push_back(new sl12::ResourceProducer<4, 1, 0>());		// Deferred Lighting Pass
	g_rrProducers_.push_back(new sl12::ResourceProducer<2, 1, 1>());		// Blur Pass

	// 記述子の準備
	sl12::RenderResourceDesc descGB0, descGB1, descGB2, descD, descLR, descLD, descBX;
	descGB0.SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
	descGB1.SetFormat(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	descGB2.SetFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
	descD.SetFormat(DXGI_FORMAT_D32_FLOAT);
	descLR.SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
	descLD.SetFormat(DXGI_FORMAT_R32_FLOAT);
	descBX.SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);

	// プロデューサー設定
	g_rrProducers_[0]->SetOutput(0, RenderID::GBuffer0, descGB0);
	g_rrProducers_[0]->SetOutput(1, RenderID::GBuffer1, descGB1);
	g_rrProducers_[0]->SetOutput(2, RenderID::GBuffer2, descGB2);
	g_rrProducers_[0]->SetOutput(3, RenderID::Depth, descD);

	g_rrProducers_[1]->SetInput(0, RenderID::Depth);
	g_rrProducers_[1]->SetOutput(0, RenderID::LinearDepth, descLD);

	g_rrProducers_[2]->SetInput(0, RenderID::GBuffer0);
	g_rrProducers_[2]->SetInput(1, RenderID::GBuffer1);
	g_rrProducers_[2]->SetInput(2, RenderID::GBuffer2);
	g_rrProducers_[2]->SetInput(3, RenderID::LinearDepth);
	g_rrProducers_[2]->SetOutput(0, RenderID::LightResult, descLR);

	g_rrProducers_[3]->SetInput(0, RenderID::LightResult);
	g_rrProducers_[3]->SetInput(1, RenderID::LinearDepth);
	g_rrProducers_[3]->SetOutput(0, sl12::kPrevOutputID, descLR);
	g_rrProducers_[3]->SetTemp(0, descBX);

	// マネージャ初期化
	if (!g_rrManager_.Initialize(g_Device_, kWindowWidth, kWindowHeight))
	{
		return false;
	}

	// リソース生成
	g_rrManager_.MakeResources(g_rrProducers_);

	return true;
}

void DestroyRenderResource()
{
	for (auto&& v : g_rrProducers_) delete v;
	g_rrManager_.Destroy();
}

void RenderScene()
{
	sl12::s32 frameIndex = g_Device_.GetSwapchain().GetFrameIndex();
	sl12::s32 nextFrameIndex = (frameIndex + 1) % sl12::Swapchain::kMaxBuffer;

	sl12::CommandList& mainCmdList = g_mainCmdLists_[frameIndex];

	g_Gui_.BeginNewFrame(&mainCmdList, kWindowWidth, kWindowHeight, g_InputData_);

	// グラフィクスコマンドロードの開始
	mainCmdList.Reset();

	auto scTex = g_Device_.GetSwapchain().GetCurrentTexture(1);
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_Device_.GetSwapchain().GetDescHandle(nextFrameIndex);
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = g_DepthBufferView_.GetDesc()->GetCpuHandle();
	ID3D12GraphicsCommandList* pCmdList = mainCmdList.GetCommandList();

	mainCmdList.TransitionBarrier(scTex, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

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
	auto&& curCB = g_SceneCBs_[frameIndex];
	{
		static const float kNearZ = 1.0f;
		static const float kFarZ = 10000.0f;
		static const float kFovY = DirectX::XMConvertToRadians(60.0f);
		const float kAspect = (float)kWindowWidth / (float)kWindowHeight;

		SceneCB* ptr = reinterpret_cast<SceneCB*>(curCB.ptr_);
		DirectX::FXMVECTOR eye = DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(-600.0f, 200.0f, 0.0f));
		DirectX::FXMVECTOR focus = DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
		DirectX::FXMVECTOR up = DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f));
		auto mtxView = DirectX::XMMatrixLookAtRH(eye, focus, up);
		auto mtxClip = DirectX::XMMatrixPerspectiveFovRH(kFovY, kAspect, kNearZ, kFarZ);
		DirectX::XMStoreFloat4x4(&ptr->mtxWorldToView, mtxView);
		DirectX::XMStoreFloat4x4(&ptr->mtxViewToWorld, DirectX::XMMatrixInverse(nullptr, mtxView));
		DirectX::XMStoreFloat4x4(&ptr->mtxViewToClip, mtxClip);
		ptr->screenInfo = DirectX::XMFLOAT4((float)kWindowWidth, (float)kWindowHeight, kNearZ, kFarZ);
		ptr->frustumCorner.z = kFarZ;
		ptr->frustumCorner.y = tanf(kFovY * 0.5f) * kFarZ;
		ptr->frustumCorner.x = ptr->frustumCorner.y * kAspect;

		//sAngle += 1.0f;
	}

	// BasePass
	{
		auto thisProd = g_rrProducers_[0];
		sl12::RenderResource* pOutputs[] = {
			g_rrManager_.GetRenderResourceFromID(thisProd->GetOutputIds()[0]),		// GBuffer0
			g_rrManager_.GetRenderResourceFromID(thisProd->GetOutputIds()[1]),		// GBuffer1
			g_rrManager_.GetRenderResourceFromID(thisProd->GetOutputIds()[2]),		// GBuffer2
			g_rrManager_.GetRenderResourceFromID(thisProd->GetOutputIds()[3]),		// Depth
		};

		D3D12_CPU_DESCRIPTOR_HANDLE rtvs[3]
		{
			pOutputs[0]->GetRtv()->GetDesc()->GetCpuHandle(),
			pOutputs[1]->GetRtv()->GetDesc()->GetCpuHandle(),
			pOutputs[2]->GetRtv()->GetDesc()->GetCpuHandle(),
		};
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = pOutputs[3]->GetDsv()->GetDesc()->GetCpuHandle();

		// バリア
		auto prevState = thisProd->GetOutputPrevStates();
		for (auto&& p : pOutputs)
		{
			if (p->IsRtv())
			{
				mainCmdList.TransitionBarrier(p->GetTexture(), *prevState, D3D12_RESOURCE_STATE_RENDER_TARGET);
			}
			else if (p->IsDsv())
			{
				mainCmdList.TransitionBarrier(p->GetTexture(), *prevState, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			}
			prevState++;
		}

		// 画面クリア
		pCmdList->ClearRenderTargetView(rtvs[0], pOutputs[0]->GetTexture()->GetTextureDesc().clearColor, 0, nullptr);
		pCmdList->ClearRenderTargetView(rtvs[1], pOutputs[1]->GetTexture()->GetTextureDesc().clearColor, 0, nullptr);
		pCmdList->ClearRenderTargetView(rtvs[2], pOutputs[2]->GetTexture()->GetTextureDesc().clearColor, 0, nullptr);
		pCmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		// レンダーターゲット設定
		pCmdList->OMSetRenderTargets(_countof(rtvs), rtvs, false, &dsv);

		// DescriptorHeapを設定
		ID3D12DescriptorHeap* pDescHeaps[] = {
			g_Device_.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetHeap(),
			g_Device_.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER).GetHeap()
		};
		pCmdList->SetDescriptorHeaps(_countof(pDescHeaps), pDescHeaps);

		// PSOとルートシグネチャを設定
		pCmdList->SetPipelineState(g_basePassPso_.GetPSO());
		pCmdList->SetGraphicsRootSignature(g_basePassSig_.GetRootSignature()->GetRootSignature());

		// デスクリプタテーブル設定
		g_basePassSig_.SetDescriptor(mainCmdList, "CbScene", curCB.cbv_);
		g_basePassSig_.SetDescriptor(mainCmdList, "CbMesh", g_MeshCB_.cbv_);

		// DrawCall
		auto submeshCount = g_mesh_.GetSubmeshCount();
		for (sl12::s32 i = 0; i < submeshCount; ++i)
		{
			sl12::DrawSubmeshInfo info = g_mesh_.GetDrawSubmeshInfo(i);

			D3D12_VERTEX_BUFFER_VIEW views[] = {
				info.pShape->GetPositionView()->GetView(),
				info.pShape->GetNormalView()->GetView(),
				info.pShape->GetTexcoordView()->GetView(),
			};
			pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCmdList->IASetVertexBuffers(0, _countof(views), views);
			pCmdList->IASetIndexBuffer(&info.pSubmesh->GetIndexBufferView()->GetView());
			pCmdList->DrawIndexedInstanced(info.numIndices, 1, 0, 0, 0);
		}
	}

	// LinearDepthPass
	{
		auto thisProd = g_rrProducers_[1];
		sl12::RenderResource* pInput = g_rrManager_.GetRenderResourceFromID(thisProd->GetInputIds()[0]);
		sl12::RenderResource* pOutput = g_rrManager_.GetRenderResourceFromID(thisProd->GetOutputIds()[0]);

		D3D12_CPU_DESCRIPTOR_HANDLE rtv = pOutput->GetRtv()->GetDesc()->GetCpuHandle();

		// バリア
		mainCmdList.TransitionBarrier(pInput->GetTexture(), *thisProd->GetInputPrevStates(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		mainCmdList.TransitionBarrier(pOutput->GetTexture(), *thisProd->GetOutputPrevStates(), D3D12_RESOURCE_STATE_RENDER_TARGET);

		// レンダーターゲット設定
		pCmdList->OMSetRenderTargets(1, &rtv, false, nullptr);

		// PSOとルートシグネチャを設定
		pCmdList->SetPipelineState(g_linearDepthPso_.GetPSO());
		pCmdList->SetGraphicsRootSignature(g_linearDepthSig_.GetRootSignature()->GetRootSignature());

		// デスクリプタテーブル設定
		g_linearDepthSig_.SetDescriptor(mainCmdList, "CbScene", curCB.cbv_);
		g_linearDepthSig_.SetDescriptor(mainCmdList, "texDepth", *pInput->GetSrv());

		// DrawCall
		pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCmdList->IASetVertexBuffers(0, 0, nullptr);
		pCmdList->IASetIndexBuffer(nullptr);
		pCmdList->DrawInstanced(3, 1, 0, 0);
	}

	// LightingPass
	{
		auto thisProd = g_rrProducers_[2];
		sl12::RenderResource* pInputs[] = {
			g_rrManager_.GetRenderResourceFromID(thisProd->GetInputIds()[0]),		// GBuffer0
			g_rrManager_.GetRenderResourceFromID(thisProd->GetInputIds()[1]),		// GBuffer1
			g_rrManager_.GetRenderResourceFromID(thisProd->GetInputIds()[2]),		// GBuffer2
			g_rrManager_.GetRenderResourceFromID(thisProd->GetInputIds()[3]),		// LinearDepth
		};
		sl12::RenderResource* pOutput = g_rrManager_.GetRenderResourceFromID(thisProd->GetOutputIds()[0]);

		D3D12_CPU_DESCRIPTOR_HANDLE rtv = pOutput->GetRtv()->GetDesc()->GetCpuHandle();

		// バリア
		auto inputPrevStates = thisProd->GetInputPrevStates();
		auto outputPrevStates = thisProd->GetOutputPrevStates();
		mainCmdList.TransitionBarrier(pInputs[0]->GetTexture(), inputPrevStates[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		mainCmdList.TransitionBarrier(pInputs[1]->GetTexture(), inputPrevStates[1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		mainCmdList.TransitionBarrier(pInputs[2]->GetTexture(), inputPrevStates[2], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		mainCmdList.TransitionBarrier(pInputs[3]->GetTexture(), inputPrevStates[3], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		mainCmdList.TransitionBarrier(pOutput->GetTexture(), outputPrevStates[0], D3D12_RESOURCE_STATE_RENDER_TARGET);

		// レンダーターゲット設定
		pCmdList->OMSetRenderTargets(1, &rtv, false, nullptr);

		// PSOとルートシグネチャを設定
		pCmdList->SetPipelineState(g_lightingPso_.GetPSO());
		pCmdList->SetGraphicsRootSignature(g_lightingSig_.GetRootSignature()->GetRootSignature());

		// デスクリプタテーブル設定
		g_lightingSig_.SetDescriptor(mainCmdList, "CbScene", curCB.cbv_);
		g_lightingSig_.SetDescriptor(mainCmdList, "texGBuffer0", *pInputs[0]->GetSrv());
		g_lightingSig_.SetDescriptor(mainCmdList, "texGBuffer1", *pInputs[1]->GetSrv());
		g_lightingSig_.SetDescriptor(mainCmdList, "texGBuffer2", *pInputs[2]->GetSrv());
		g_lightingSig_.SetDescriptor(mainCmdList, "texLinearDepth", *pInputs[3]->GetSrv());

		// DrawCall
		pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCmdList->IASetVertexBuffers(0, 0, nullptr);
		pCmdList->IASetIndexBuffer(nullptr);
		pCmdList->DrawInstanced(3, 1, 0, 0);
	}

	// BlurPass
	{
		auto thisProd = g_rrProducers_[3];
		sl12::RenderResource* pInputs[] = {
			g_rrManager_.GetRenderResourceFromID(thisProd->GetInputIds()[0]),		// LightResult
			g_rrManager_.GetRenderResourceFromID(thisProd->GetInputIds()[1]),		// LinearDepth
		};
		sl12::RenderResource* pTemp = g_rrManager_.GetRenderResourceFromID(thisProd->GetTempIds()[0]);

		D3D12_CPU_DESCRIPTOR_HANDLE tempRtv = pTemp->GetRtv()->GetDesc()->GetCpuHandle();

		// バリア
		auto inputPrevStates = thisProd->GetInputPrevStates();
		auto tempPrevStates = thisProd->GetTempPrevStates();
		mainCmdList.TransitionBarrier(pInputs[0]->GetTexture(), inputPrevStates[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		mainCmdList.TransitionBarrier(pInputs[1]->GetTexture(), inputPrevStates[1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		mainCmdList.TransitionBarrier(pTemp->GetTexture(), tempPrevStates[0], D3D12_RESOURCE_STATE_RENDER_TARGET);

		//// X軸方向
		// レンダーターゲット設定
		pCmdList->OMSetRenderTargets(1, &tempRtv, false, nullptr);

		// PSOとルートシグネチャを設定
		pCmdList->SetPipelineState(g_blurXPassPso_.GetPSO());
		pCmdList->SetGraphicsRootSignature(g_blurXPassSig_.GetRootSignature()->GetRootSignature());

		// デスクリプタテーブル設定
		g_blurXPassSig_.SetDescriptor(mainCmdList, "CbGaussBlur", g_BlurCB_.cbv_);
		g_blurXPassSig_.SetDescriptor(mainCmdList, "texSource", *pInputs[0]->GetSrv());
		g_blurXPassSig_.SetDescriptor(mainCmdList, "texLinearDepth", *pInputs[1]->GetSrv());
		g_blurXPassSig_.SetDescriptor(mainCmdList, "samLinearClamp", g_samLinearClamp_);

		// DrawCall
		pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCmdList->IASetVertexBuffers(0, 0, nullptr);
		pCmdList->IASetIndexBuffer(nullptr);
		pCmdList->DrawInstanced(3, 1, 0, 0);


		//// Y軸方向
		// バリア
		mainCmdList.TransitionBarrier(pTemp->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		// レンダーターゲット設定
		pCmdList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

		// PSOとルートシグネチャを設定
		pCmdList->SetPipelineState(g_blurYPassPso_.GetPSO());
		pCmdList->SetGraphicsRootSignature(g_blurYPassSig_.GetRootSignature()->GetRootSignature());

		// デスクリプタテーブル設定
		g_blurYPassSig_.SetDescriptor(mainCmdList, "CbGaussBlur", g_BlurCB_.cbv_);
		g_blurYPassSig_.SetDescriptor(mainCmdList, "texSource", *pTemp->GetSrv());
		g_blurYPassSig_.SetDescriptor(mainCmdList, "texLinearDepth", *pInputs[1]->GetSrv());
		g_blurYPassSig_.SetDescriptor(mainCmdList, "samLinearClamp", g_samLinearClamp_);

		// DrawCall
		pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCmdList->IASetVertexBuffers(0, 0, nullptr);
		pCmdList->IASetIndexBuffer(nullptr);
		pCmdList->DrawInstanced(3, 1, 0, 0);
	}

	ImGui::Render();

	mainCmdList.TransitionBarrier(scTex, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

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
	ret = InitializeRenderResource();
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
	DestroyRenderResource();
	DestroyAssets();
	g_copyCmdList_.Destroy();
	for (auto& v : g_mainCmdLists_)
		v.Destroy();
	for (auto& v : g_mainCmdLists_)
		v.Destroy();
	g_Device_.Destroy();

	return static_cast<char>(msg.wParam);
}
