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
#include <sl12/descriptor_set.h>
#include <sl12/timestamp.h>

#include "file.h"

#include <DirectXTex.h>
#include <windowsx.h>


namespace
{
	struct SceneCB
	{
		DirectX::XMFLOAT4X4		mtxWorldToView;
		DirectX::XMFLOAT4X4		mtxViewToClip;
		DirectX::XMFLOAT4X4		mtxViewToWorld;
		DirectX::XMFLOAT4X4		mtxPrevWorldToClip;
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

	struct WaterCB
	{
		float					waterHeight;
		float					temporalBlend;
		float					enableFresnel;
		float					fresnelCoeff;
		float					gapBleed;
	};	// struct WaterCB

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
			TiledLightC,
			ClearHashC,
			ProjectHashC,
			ResolveHashP,
			WaterV,
			WaterP,
			ReprojectReflectionV,
			ReprojectReflectionP,

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
	static const int kTileWidth = 16;
	static const int kLightMax = 128;

	DirectX::XMFLOAT4	g_LightPos_[kLightMax];

	HWND	g_hWnd_;

	sl12::Device		g_Device_;
	sl12::CommandList	g_mainCmdLists_[kMaxFrameCount];
	sl12::CommandList	g_copyCmdList_;

	ConstantSet				g_SceneCBs_[kMaxFrameCount];
	ConstantSet				g_MeshCB_;
	ConstantSet				g_BlurCB_;
	ConstantSet				g_LightCB_;
	ConstantSet				g_WaterCBs_[kMaxFrameCount];
	TextureSet				g_WaveNormalTex_;

	sl12::Sampler			g_sampler_;
	sl12::Sampler			g_samLinearClamp_;

	sl12::Shader			g_Shaders_[ShaderKind::Max];

	sl12::Buffer			g_LightPosB_[kMaxFrameCount];
	sl12::BufferView		g_LightPosBV_[kMaxFrameCount];
	sl12::Buffer			g_LightColorB_;
	sl12::BufferView		g_LightColorBV_;
	sl12::Buffer			g_WaterVB_;
	sl12::VertexBufferView	g_WaterVBV_;
	sl12::Buffer			g_WaterIB_;
	sl12::IndexBufferView	g_WaterIBV_;

	sl12::RootSignatureManager	g_rootSigMan_;
	sl12::RootSignatureHandle	g_basePassSig_;
	sl12::RootSignatureHandle	g_linearDepthSig_;
	sl12::RootSignatureHandle	g_lightingSig_;
	sl12::RootSignatureHandle	g_blurXPassSig_;
	sl12::RootSignatureHandle	g_blurYPassSig_;
	sl12::RootSignatureHandle	g_clearHashSig_;
	sl12::RootSignatureHandle	g_projectHashSig_;
	sl12::RootSignatureHandle	g_resolveHashSig_;
	sl12::RootSignatureHandle	g_tiledLightSig_;
	sl12::RootSignatureHandle	g_waterSig_;
	sl12::RootSignatureHandle	g_reprojectSig_;

	sl12::GraphicsPipelineState	g_basePassPso_;
	sl12::GraphicsPipelineState	g_linearDepthPso_;
	sl12::GraphicsPipelineState	g_lightingPso_;
	sl12::GraphicsPipelineState	g_blurXPassPso_;
	sl12::GraphicsPipelineState	g_blurYPassPso_;
	sl12::GraphicsPipelineState	g_resolveHashPso_;
	sl12::GraphicsPipelineState	g_waterPso_;
	sl12::GraphicsPipelineState	g_reprojectPso_;
	sl12::ComputePipelineState	g_tiledLightPso_;
	sl12::ComputePipelineState	g_clearHashPso_;
	sl12::ComputePipelineState	g_projectHashPso_;

	sl12::DescriptorSet			g_descSet_;

	sl12::File			g_meshFile_;
	sl12::MeshInstance	g_mesh_;

	sl12::Timestamp		g_gpuTimestamp_[sl12::Swapchain::kMaxBuffer];

	LARGE_INTEGER		g_frequency_;
	LARGE_INTEGER		g_perfCount_;
	LARGE_INTEGER		g_perfCountMax_;

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
			HashBuffer,
			WaterResult,
		};
	};	// struct RenderID
	sl12::RenderResourceManager					g_rrManager_;
	std::vector<sl12::ResourceProducerBase*>	g_rrProducers_;

	sl12::Gui	g_Gui_;
	sl12::InputData	g_InputData_{};

	static float	g_waterHeight = 60.0f;
	static float	g_temporalBlend = 0.5f;
	static float	g_fresnelCoeff = 2.0f;
	static bool		g_enableFresnel = true;
	static bool		g_gapBleed = true;
	static bool		g_scenePause = false;

	int					g_SyncInterval = 1;
}

// テクスチャを読み込む
bool LoadTexture(TextureSet* pTexSet, const char* filename)
{
	File texFile(filename);

	if (!pTexSet->tex_.InitializeFromTGA(&g_Device_, &g_copyCmdList_, texFile.GetData(), texFile.GetSize(), 1, false))
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

	// バッファを作成
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
	{
		for (int i = 0; i < 3; ++i)
		{
			if (!g_LightPosB_[i].Initialize(&g_Device_, sizeof(DirectX::XMFLOAT4) * kLightMax, sizeof(DirectX::XMFLOAT4), sl12::BufferUsage::ShaderResource, true, false))
			{
				return false;
			}
			if (!g_LightPosBV_[i].Initialize(&g_Device_, &g_LightPosB_[i], 0, sizeof(DirectX::XMFLOAT4)))
			{
				return false;
			}
		}
		if (!g_LightColorB_.Initialize(&g_Device_, sizeof(DirectX::XMFLOAT4) * kLightMax, sizeof(DirectX::XMFLOAT4), sl12::BufferUsage::ShaderResource, true, false))
		{
			return false;
		}
		if (!g_LightColorBV_.Initialize(&g_Device_, &g_LightColorB_, 0, sizeof(DirectX::XMFLOAT4)))
		{
			return false;
		}

		auto randFloat = [](float minV, float maxV)
		{
			float r = (float)std::rand() / (float)RAND_MAX;
			return (maxV - minV) * r + minV;
		};

		auto color = reinterpret_cast<DirectX::XMFLOAT4*>(g_LightColorB_.Map(nullptr));
		for (sl12::u32 i = 0; i < kLightMax; i++, color++)
		{
			g_LightPos_[i].x = randFloat(-1000.0f, 1000.0f);
			g_LightPos_[i].y = randFloat(100.0f, 400.0f);
			g_LightPos_[i].z = randFloat(-500.0f, 500.0f);
			g_LightPos_[i].w = randFloat(100.0f, 500.0f);

			float intensity = randFloat(3000.0f, 10000.0f);
			color->x = randFloat(0.0f, 1.0f) * intensity;
			color->y = randFloat(0.0f, 1.0f) * intensity;
			color->z = randFloat(0.0f, 1.0f) * intensity;
			color->w = 1.0f;
		}
		g_LightColorB_.Unmap();

		if (!g_LightCB_.cb_.Initialize(&g_Device_, sizeof(sl12::u32), 1, sl12::BufferUsage::ConstantBuffer, true, false))
		{
			return false;
		}

		if (!g_LightCB_.cbv_.Initialize(&g_Device_, &g_LightCB_.cb_))
		{
			return false;
		}

		auto p = reinterpret_cast<sl12::u32*>(g_LightCB_.cb_.Map(nullptr));
		*p = kLightMax;
		g_LightCB_.cb_.Unmap();
	}
	{
		if (!g_WaterVB_.Initialize(&g_Device_, sizeof(DirectX::XMFLOAT3) * 4, sizeof(DirectX::XMFLOAT3), sl12::BufferUsage::VertexBuffer, true, false))
		{
			return false;
		}
		if (!g_WaterVBV_.Initialize(&g_Device_, &g_WaterVB_))
		{
			return false;
		}
		auto vb = reinterpret_cast<DirectX::XMFLOAT3*>(g_WaterVB_.Map(nullptr));
		vb[0] = DirectX::XMFLOAT3(-1000.0f, 0.0f, -500.0f);
		vb[1] = DirectX::XMFLOAT3( 1000.0f, 0.0f, -500.0f);
		vb[2] = DirectX::XMFLOAT3(-1000.0f, 0.0f,  500.0f);
		vb[3] = DirectX::XMFLOAT3( 1000.0f, 0.0f,  500.0f);
		g_WaterVB_.Unmap();

		if (!g_WaterIB_.Initialize(&g_Device_, sizeof(sl12::u16) * 6, sizeof(sl12::u16), sl12::BufferUsage::IndexBuffer, true, false))
		{
			return false;
		}
		if (!g_WaterIBV_.Initialize(&g_Device_, &g_WaterIB_))
		{
			return false;
		}
		auto ib = reinterpret_cast<sl12::u16*>(g_WaterIB_.Map(nullptr));
		ib[0] = 0; ib[1] = 1; ib[2] = 2;
		ib[3] = 1; ib[4] = 3; ib[5] = 2;
		g_WaterIB_.Unmap();

		for (int i = 0; i < kMaxFrameCount; ++i)
		{
			if (!g_WaterCBs_[i].cb_.Initialize(&g_Device_, sizeof(WaterCB), 1, sl12::BufferUsage::ConstantBuffer, true, false))
			{
				return false;
			}

			if (!g_WaterCBs_[i].cbv_.Initialize(&g_Device_, &g_WaterCBs_[i].cb_))
			{
				return false;
			}
		}
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

	// テクスチャロード
	if (!LoadTexture(&g_WaveNormalTex_, "data/wave_normal.tga"))
	{
		return false;
	}

	// シェーダロード
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
	if (!g_Shaders_[ShaderKind::TiledLightC].Initialize(&g_Device_, sl12::ShaderType::Compute, "data/tile_lighting.c.cso"))
	{
		return false;
	}
	if (!g_Shaders_[ShaderKind::ClearHashC].Initialize(&g_Device_, sl12::ShaderType::Compute, "data/clear_hash.c.cso"))
	{
		return false;
	}
	if (!g_Shaders_[ShaderKind::ProjectHashC].Initialize(&g_Device_, sl12::ShaderType::Compute, "data/project_hash.c.cso"))
	{
		return false;
	}
	if (!g_Shaders_[ShaderKind::ResolveHashP].Initialize(&g_Device_, sl12::ShaderType::Pixel, "data/resolve_hash.p.cso"))
	{
		return false;
	}
	if (!g_Shaders_[ShaderKind::WaterV].Initialize(&g_Device_, sl12::ShaderType::Vertex, "data/water.vv.cso"))
	{
		return false;
	}
	if (!g_Shaders_[ShaderKind::WaterP].Initialize(&g_Device_, sl12::ShaderType::Pixel, "data/water.p.cso"))
	{
		return false;
	}
	if (!g_Shaders_[ShaderKind::ReprojectReflectionV].Initialize(&g_Device_, sl12::ShaderType::Vertex, "data/reproject_reflection.vv.cso"))
	{
		return false;
	}
	if (!g_Shaders_[ShaderKind::ReprojectReflectionP].Initialize(&g_Device_, sl12::ShaderType::Pixel, "data/reproject_reflection.p.cso"))
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

		desc.pPS = &g_Shaders_[ShaderKind::ResolveHashP];
		g_resolveHashSig_ = g_rootSigMan_.CreateRootSignature(desc);

		desc.pVS = &g_Shaders_[ShaderKind::WaterV];
		desc.pPS = &g_Shaders_[ShaderKind::WaterP];
		g_waterSig_ = g_rootSigMan_.CreateRootSignature(desc);

		desc.pVS = &g_Shaders_[ShaderKind::ReprojectReflectionV];
		desc.pPS = &g_Shaders_[ShaderKind::ReprojectReflectionP];
		g_reprojectSig_ = g_rootSigMan_.CreateRootSignature(desc);
	}
	{
		sl12::RootSignatureCreateDesc desc;

		desc.pCS = &g_Shaders_[ShaderKind::TiledLightC];
		g_tiledLightSig_ = g_rootSigMan_.CreateRootSignature(desc);

		desc.pCS = &g_Shaders_[ShaderKind::ClearHashC];
		g_clearHashSig_ = g_rootSigMan_.CreateRootSignature(desc);

		desc.pCS = &g_Shaders_[ShaderKind::ProjectHashC];
		g_projectHashSig_ = g_rootSigMan_.CreateRootSignature(desc);
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
		desc.dsvFormat = kDepthViewFormat;
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
	{
		sl12::GraphicsPipelineStateDesc desc;
		desc.pRootSignature = g_resolveHashSig_.GetRootSignature();
		desc.pVS = &g_Shaders_[ShaderKind::PostProcessV];
		desc.pPS = &g_Shaders_[ShaderKind::ResolveHashP];

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

		if (!g_resolveHashPso_.Initialize(&g_Device_, desc))
		{
			return false;
		}
	}
	{
		sl12::GraphicsPipelineStateDesc desc;
		desc.pRootSignature = g_waterSig_.GetRootSignature();
		desc.pVS = &g_Shaders_[ShaderKind::WaterV];
		desc.pPS = &g_Shaders_[ShaderKind::WaterP];

		desc.blend.sampleMask = UINT_MAX;
		desc.blend.rtDesc[0].isBlendEnable = true;
		desc.blend.rtDesc[0].srcBlendColor = D3D12_BLEND_SRC_ALPHA;
		desc.blend.rtDesc[0].dstBlendColor = D3D12_BLEND_INV_SRC_ALPHA;
		desc.blend.rtDesc[0].blendOpColor = D3D12_BLEND_OP_ADD;
		desc.blend.rtDesc[0].srcBlendAlpha = D3D12_BLEND_ONE;
		desc.blend.rtDesc[0].dstBlendAlpha = D3D12_BLEND_ZERO;
		desc.blend.rtDesc[0].blendOpAlpha = D3D12_BLEND_OP_ADD;
		desc.blend.rtDesc[0].writeMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		desc.rasterizer.cullMode = D3D12_CULL_MODE_NONE;
		desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
		desc.rasterizer.isDepthClipEnable = true;
		desc.rasterizer.isFrontCCW = true;

		desc.depthStencil.isDepthEnable = true;
		desc.depthStencil.isDepthWriteEnable = true;
		desc.depthStencil.depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		D3D12_INPUT_ELEMENT_DESC inputElem[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		desc.inputLayout.numElements = _countof(inputElem);
		desc.inputLayout.pElements = inputElem;

		desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		desc.numRTVs = 0;
		desc.rtvFormats[desc.numRTVs++] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		desc.dsvFormat = kDepthViewFormat;
		desc.multisampleCount = 1;

		if (!g_waterPso_.Initialize(&g_Device_, desc))
		{
			return false;
		}
	}
	{
		sl12::GraphicsPipelineStateDesc desc;
		desc.pRootSignature = g_reprojectSig_.GetRootSignature();
		desc.pVS = &g_Shaders_[ShaderKind::ReprojectReflectionV];
		desc.pPS = &g_Shaders_[ShaderKind::ReprojectReflectionP];

		desc.blend.sampleMask = UINT_MAX;
		desc.blend.rtDesc[0].isBlendEnable = true;
		desc.blend.rtDesc[0].srcBlendColor = D3D12_BLEND_SRC_ALPHA;
		desc.blend.rtDesc[0].dstBlendColor = D3D12_BLEND_INV_SRC_ALPHA;
		desc.blend.rtDesc[0].blendOpColor = D3D12_BLEND_OP_ADD;
		desc.blend.rtDesc[0].srcBlendAlpha = D3D12_BLEND_ONE;
		desc.blend.rtDesc[0].dstBlendAlpha = D3D12_BLEND_ONE;
		desc.blend.rtDesc[0].blendOpAlpha = D3D12_BLEND_OP_MAX;
		desc.blend.rtDesc[0].writeMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		desc.rasterizer.cullMode = D3D12_CULL_MODE_NONE;
		desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
		desc.rasterizer.isDepthClipEnable = true;
		desc.rasterizer.isFrontCCW = true;

		desc.depthStencil.isDepthEnable = false;
		desc.depthStencil.isDepthWriteEnable = false;

		D3D12_INPUT_ELEMENT_DESC inputElem[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		desc.inputLayout.numElements = _countof(inputElem);
		desc.inputLayout.pElements = inputElem;

		desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		desc.numRTVs = 0;
		desc.rtvFormats[desc.numRTVs++] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		desc.dsvFormat = DXGI_FORMAT_UNKNOWN;
		desc.multisampleCount = 1;

		if (!g_reprojectPso_.Initialize(&g_Device_, desc))
		{
			return false;
		}
	}
	{
		sl12::ComputePipelineStateDesc desc;
		desc.pRootSignature = g_tiledLightSig_.GetRootSignature();
		desc.pCS = &g_Shaders_[ShaderKind::TiledLightC];

		if (!g_tiledLightPso_.Initialize(&g_Device_, desc))
		{
			return false;
		}
	}
	{
		sl12::ComputePipelineStateDesc desc;
		desc.pRootSignature = g_clearHashSig_.GetRootSignature();
		desc.pCS = &g_Shaders_[ShaderKind::ClearHashC];

		if (!g_clearHashPso_.Initialize(&g_Device_, desc))
		{
			return false;
		}
	}
	{
		sl12::ComputePipelineStateDesc desc;
		desc.pRootSignature = g_projectHashSig_.GetRootSignature();
		desc.pCS = &g_Shaders_[ShaderKind::ProjectHashC];

		if (!g_projectHashPso_.Initialize(&g_Device_, desc))
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
	if (!g_Gui_.Initialize(&g_Device_, DXGI_FORMAT_R8G8B8A8_UNORM))
	{
		return false;
	}
	if (!g_Gui_.CreateFontImage(&g_Device_, g_copyCmdList_))
	{
		return false;
	}

	for (int i = 0; i < ARRAYSIZE(g_gpuTimestamp_); ++i)
	{
		if (!g_gpuTimestamp_[i].Initialize(&g_Device_, 2))
		{
			return false;
		}
	}

	return true;
}

void DestroyAssets()
{
	for (auto&& v : g_gpuTimestamp_) v.Destroy();

	g_Gui_.Destroy();

	g_mesh_.Destroy();
	g_meshFile_.Destroy();

	g_basePassPso_.Destroy();
	g_linearDepthPso_.Destroy();
	g_lightingPso_.Destroy();
	g_blurXPassPso_.Destroy();
	g_blurYPassPso_.Destroy();
	g_clearHashPso_.Destroy();
	g_projectHashPso_.Destroy();
	g_resolveHashPso_.Destroy();
	g_tiledLightPso_.Destroy();
	g_waterPso_.Destroy();
	g_reprojectPso_.Destroy();

	g_basePassSig_.Invalid();
	g_linearDepthSig_.Invalid();
	g_lightingSig_.Invalid();
	g_blurXPassSig_.Invalid();
	g_blurYPassSig_.Invalid();
	g_clearHashSig_.Invalid();
	g_projectHashSig_.Invalid();
	g_resolveHashSig_.Invalid();
	g_tiledLightSig_.Invalid();
	g_waterSig_.Invalid();
	g_reprojectSig_.Invalid();
	g_rootSigMan_.Destroy();

	for (auto&& v : g_LightPosBV_) v.Destroy();
	for (auto&& v : g_LightPosB_) v.Destroy();
	g_LightColorBV_.Destroy();
	g_LightColorB_.Destroy();

	g_samLinearClamp_.Destroy();
	g_sampler_.Destroy();

	for (auto&& v : g_Shaders_) v.Destroy();
	g_WaveNormalTex_.Destroy();

	for (auto&&v : g_WaterCBs_) v.Destroy();
	g_LightCB_.Destroy();
	g_BlurCB_.Destroy();
	g_MeshCB_.Destroy();
	for (auto&&v : g_SceneCBs_) v.Destroy();
}

bool InitializeRenderResource()
{
	// プロデューサーを生成する
	g_rrProducers_.push_back(new sl12::ResourceProducer<0, 4, 0>());		// Deferred Base Pass
	g_rrProducers_.push_back(new sl12::ResourceProducer<1, 1, 0>());		// Linear Depth Pass
	g_rrProducers_.push_back(new sl12::ResourceProducer<4, 1, 0>());		// Deferred Lighting Pass
	g_rrProducers_.push_back(new sl12::ResourceProducer<1, 1, 0>());		// Projection Hash Pass
	g_rrProducers_.push_back(new sl12::ResourceProducer<2, 1, 0>());		// Resolve Hash Pass
	g_rrProducers_.push_back(new sl12::ResourceProducer<2, 1, 0>());		// Temporal Reprojection Pass
	g_rrProducers_.push_back(new sl12::ResourceProducer<1, 2, 0>());		// Water Pass
	g_rrProducers_.push_back(new sl12::ResourceProducer<2, 1, 1>());		// Blur Pass

																			// 記述子の準備
	sl12::RenderResourceDesc descGB0, descGB1, descGB2, descD, descLR, descLD, descBX, descHash, descWater;
	descGB0.SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
	descGB1.SetFormat(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	descGB2.SetFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
	descD.SetFormat(kDepthViewFormat);
	descLR.SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT).SetUavCount(1);
	descLD.SetFormat(DXGI_FORMAT_R32_FLOAT);
	descBX.SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
	descHash.SetFormat(DXGI_FORMAT_R32_UINT).SetUavCount(1);
	descWater.SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT).SetHistoryMax(1);

	// プロデューサー設定
	{
		int pass = 0;

		// Deferred Base Pass
		g_rrProducers_[pass]->SetOutputUnique(0, RenderID::GBuffer0, descGB0);
		g_rrProducers_[pass]->SetOutputUnique(1, RenderID::GBuffer1, descGB1);
		g_rrProducers_[pass]->SetOutputUnique(2, RenderID::GBuffer2, descGB2);
		g_rrProducers_[pass]->SetOutputUnique(3, RenderID::Depth, descD);
		pass++;

		// Linear Depth Pass
		g_rrProducers_[pass]->SetInputUnique(0, RenderID::Depth);
		g_rrProducers_[pass]->SetOutputUnique(0, RenderID::LinearDepth, descLD);
		pass++;

		// Deferred Lighting Pass
		g_rrProducers_[pass]->SetInputUnique(0, RenderID::GBuffer0);
		g_rrProducers_[pass]->SetInputUnique(1, RenderID::GBuffer1);
		g_rrProducers_[pass]->SetInputUnique(2, RenderID::GBuffer2);
		g_rrProducers_[pass]->SetInputUnique(3, RenderID::LinearDepth);
		g_rrProducers_[pass]->SetOutputUnique(0, RenderID::LightResult, descLR);
		pass++;

		// Projection Hash Pass
		g_rrProducers_[pass]->SetInputUnique(0, RenderID::LinearDepth);
		g_rrProducers_[pass]->SetOutputUnique(0, RenderID::HashBuffer, descHash);
		pass++;

		// Resolve Hash Pass
		g_rrProducers_[pass]->SetInputUnique(0, RenderID::LightResult);
		g_rrProducers_[pass]->SetInputUnique(1, RenderID::HashBuffer);
		g_rrProducers_[pass]->SetOutputUnique(0, RenderID::WaterResult, descWater);
		pass++;

		// Temporal Reprojection Pass
		g_rrProducers_[pass]->SetInputUnique(0, RenderID::WaterResult, 1);
		g_rrProducers_[pass]->SetInputUnique(1, RenderID::HashBuffer);
		g_rrProducers_[pass]->SetOutputUnique(0, RenderID::WaterResult, descWater);
		pass++;

		// Water Pass
		g_rrProducers_[pass]->SetInputUnique(0, RenderID::WaterResult);
		g_rrProducers_[pass]->SetOutputUnique(0, RenderID::LightResult, descLR);
		g_rrProducers_[pass]->SetOutputUnique(1, RenderID::Depth, descD);
		pass++;

		// Blur Pass
		g_rrProducers_[pass]->SetInputUnique(0, RenderID::LightResult);
		g_rrProducers_[pass]->SetInputUnique(1, RenderID::LinearDepth);
		g_rrProducers_[pass]->SetOutputSwapchain(0);
		g_rrProducers_[pass]->SetTemp(0, descBX);
		pass++;
	}

	// マネージャ初期化
	if (!g_rrManager_.Initialize(g_Device_, kWindowWidth, kWindowHeight))
	{
		return false;
	}

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
	sl12::s32 prevFrameIndex = (frameIndex + sl12::Swapchain::kMaxBuffer - 1) % sl12::Swapchain::kMaxBuffer;

	sl12::CommandList& mainCmdList = g_mainCmdLists_[frameIndex];

	g_Gui_.BeginNewFrame(&mainCmdList, kWindowWidth, kWindowHeight, g_InputData_);

	// GUI
	{
		ImGui::DragFloat("Water Height", &g_waterHeight, 0.1f, 0.0f, 100.0f);
		ImGui::DragFloat("Temporal Blend", &g_temporalBlend, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("Fresnel Coeff", &g_fresnelCoeff, 0.01f, 1.0f, 5.0f);
		ImGui::Checkbox("Fresnel Enable", &g_enableFresnel);
		ImGui::Checkbox("Gap Bleed", &g_gapBleed);
		ImGui::Checkbox("Scene Pause", &g_scenePause);

		g_perfCountMax_.QuadPart = std::max(g_perfCount_.QuadPart, g_perfCountMax_.QuadPart);
		auto us = g_perfCount_.QuadPart * 1000000 / g_frequency_.QuadPart;
		auto max_us = g_perfCountMax_.QuadPart * 1000000 / g_frequency_.QuadPart;
		ImGui::Text("CPU Time : %lld (us)", us);
		ImGui::Text("CPU Time Max : %lld (us)", max_us);

		uint64_t timestamp[2];
		g_gpuTimestamp_[prevFrameIndex].GetTimestamp(0, 2, timestamp);
		uint64_t gpu_time = timestamp[1] - timestamp[0];
		uint64_t freq = g_Device_.GetGraphicsQueue().GetTimestampFrequency();
		float gpu_us = (float)gpu_time / ((float)freq / 1000000.0f);
		ImGui::Text("GPU Time: %f (us)", gpu_us);
	}

	// グラフィクスコマンドロードの開始
	mainCmdList.Reset();

	// リソース生成
	g_rrManager_.MakeResources(g_rrProducers_);

	auto scTex = g_Device_.GetSwapchain().GetCurrentTexture(1);
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_Device_.GetSwapchain().GetDescHandle(nextFrameIndex);
	ID3D12GraphicsCommandList* pCmdList = mainCmdList.GetCommandList();

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
		static DirectX::XMFLOAT4X4 sPrevWorldToClip = DirectX::XMFLOAT4X4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
		static float sCamAngle = 0.0f;
		const float kAspect = (float)kWindowWidth / (float)kWindowHeight;

		SceneCB* ptr = reinterpret_cast<SceneCB*>(curCB.ptr_);
		auto eye = DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(-1000.0f, 200.0f, 0.0f));
		auto mtxRotY = DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(sinf(DirectX::XMConvertToRadians(sCamAngle)) * 10.0f));
		eye = DirectX::XMVector3TransformCoord(eye, mtxRotY);
		auto focus = DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
		auto up = DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f));
		auto mtxView = DirectX::XMMatrixLookAtRH(eye, focus, up);
		auto mtxClip = DirectX::XMMatrixPerspectiveFovRH(kFovY, kAspect, kNearZ, kFarZ);
		DirectX::XMStoreFloat4x4(&ptr->mtxWorldToView, mtxView);
		DirectX::XMStoreFloat4x4(&ptr->mtxViewToWorld, DirectX::XMMatrixInverse(nullptr, mtxView));
		DirectX::XMStoreFloat4x4(&ptr->mtxViewToClip, mtxClip);
		ptr->mtxPrevWorldToClip = sPrevWorldToClip;
		auto mtxVC = DirectX::XMMatrixMultiply(mtxView, mtxClip);
		DirectX::XMStoreFloat4x4(&sPrevWorldToClip, mtxVC);
		ptr->screenInfo = DirectX::XMFLOAT4((float)kWindowWidth, (float)kWindowHeight, kNearZ, kFarZ);
		ptr->frustumCorner.z = kFarZ;
		ptr->frustumCorner.y = tanf(kFovY * 0.5f) * kFarZ;
		ptr->frustumCorner.x = ptr->frustumCorner.y * kAspect;

		if (!g_scenePause)
			sCamAngle += 1.0f;
	}
	auto&& curWaterCB = g_WaterCBs_[frameIndex];
	{
		static const float kNearZ = 1.0f;
		static const float kFarZ = 10000.0f;
		static const float kFovY = DirectX::XMConvertToRadians(60.0f);
		static DirectX::XMFLOAT4X4 sPrevWorldToClip = DirectX::XMFLOAT4X4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
		static float sCamAngle = 0.0f;
		const float kAspect = (float)kWindowWidth / (float)kWindowHeight;

		WaterCB* ptr = reinterpret_cast<WaterCB*>(curWaterCB.cb_.Map(nullptr));
		ptr->waterHeight = g_waterHeight;
		ptr->temporalBlend = g_temporalBlend;
		ptr->enableFresnel = g_enableFresnel ? 1.0f : 0.0f;
		ptr->fresnelCoeff = g_fresnelCoeff;
		ptr->gapBleed = g_gapBleed ? 1.0f : 0.0f;
		curWaterCB.cb_.Unmap();
	}

	// ライト更新
	auto&& curLightPosB = g_LightPosB_[frameIndex];
	auto&& curLightPosBV = g_LightPosBV_[frameIndex];
	{
		auto mtxRot = DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(g_scenePause ? 0.0f : 1.0f));
		for (int i = 0; i < kLightMax; i++)
		{
			DirectX::XMVECTOR p = DirectX::XMLoadFloat4(&DirectX::XMFLOAT4(g_LightPos_[i].x, g_LightPos_[i].y, g_LightPos_[i].z, 1.0f));
			p = DirectX::XMVector3TransformCoord(p, mtxRot);
			DirectX::XMFLOAT4 pp;
			DirectX::XMStoreFloat4(&pp, p);
			g_LightPos_[i].x = pp.x;
			g_LightPos_[i].y = pp.y;
			g_LightPos_[i].z = pp.z;
		}

		auto p = curLightPosB.Map(nullptr);
		memcpy(p, g_LightPos_, sizeof(g_LightPos_));
		curLightPosB.Unmap();
	}

	int passNo = 0;

	LARGE_INTEGER start_time, end_time;

	QueryPerformanceCounter(&start_time);

	g_gpuTimestamp_[frameIndex].Reset();
	g_gpuTimestamp_[frameIndex].Query(&mainCmdList);

	// BasePass
	{
		auto thisProd = g_rrProducers_[passNo++];
		sl12::RenderResource* pOutputs[] = {
			g_rrManager_.GetRenderResourceFromID(thisProd->GetOutputIds()[0]),		// GBuffer0
			g_rrManager_.GetRenderResourceFromID(thisProd->GetOutputIds()[1]),		// GBuffer1
			g_rrManager_.GetRenderResourceFromID(thisProd->GetOutputIds()[2]),		// GBuffer2
			g_rrManager_.GetRenderResourceFromID(thisProd->GetOutputIds()[3]),		// Depth
		};

		D3D12_CPU_DESCRIPTOR_HANDLE rtvs[3]
		{
			pOutputs[0]->GetRtv()->GetDescInfo().cpuHandle,
			pOutputs[1]->GetRtv()->GetDescInfo().cpuHandle,
			pOutputs[2]->GetRtv()->GetDescInfo().cpuHandle,
		};
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = pOutputs[3]->GetDsv()->GetDescInfo().cpuHandle;

		// バリア
		g_rrManager_.BarrierAllResources(mainCmdList, thisProd);

		// 画面クリア
		pCmdList->ClearRenderTargetView(rtvs[0], pOutputs[0]->GetTexture()->GetTextureDesc().clearColor, 0, nullptr);
		pCmdList->ClearRenderTargetView(rtvs[1], pOutputs[1]->GetTexture()->GetTextureDesc().clearColor, 0, nullptr);
		pCmdList->ClearRenderTargetView(rtvs[2], pOutputs[2]->GetTexture()->GetTextureDesc().clearColor, 0, nullptr);
		pCmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		// レンダーターゲット設定
		pCmdList->OMSetRenderTargets(_countof(rtvs), rtvs, false, &dsv);

		// PSOとルートシグネチャを設定
		pCmdList->SetPipelineState(g_basePassPso_.GetPSO());

		// デスクリプタテーブル設定
		g_descSet_.Reset();
		g_basePassSig_.SetDescriptor(&g_descSet_, "CbScene", curCB.cbv_);
		g_basePassSig_.SetDescriptor(&g_descSet_, "CbMesh", g_MeshCB_.cbv_);

		mainCmdList.SetGraphicsRootSignatureAndDescriptorSet(g_basePassSig_.GetRootSignature(), &g_descSet_);

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

	g_gpuTimestamp_[frameIndex].Query(&mainCmdList);
	g_gpuTimestamp_[frameIndex].Resolve(&mainCmdList);

	// LinearDepthPass
	{
		auto thisProd = g_rrProducers_[passNo++];
		sl12::RenderResource* pInput = g_rrManager_.GetRenderResourceFromID(thisProd->GetInputIds()[0]);
		sl12::RenderResource* pOutput = g_rrManager_.GetRenderResourceFromID(thisProd->GetOutputIds()[0]);

		D3D12_CPU_DESCRIPTOR_HANDLE rtv = pOutput->GetRtv()->GetDescInfo().cpuHandle;

		// バリア
		g_rrManager_.BarrierAllResources(mainCmdList, thisProd);

		// レンダーターゲット設定
		pCmdList->OMSetRenderTargets(1, &rtv, false, nullptr);

		// PSOとルートシグネチャを設定
		pCmdList->SetPipelineState(g_linearDepthPso_.GetPSO());

		// デスクリプタテーブル設定
		g_descSet_.Reset();
		g_linearDepthSig_.SetDescriptor(&g_descSet_, "CbScene", curCB.cbv_);
		g_linearDepthSig_.SetDescriptor(&g_descSet_, "texDepth", *pInput->GetSrv());

		mainCmdList.SetGraphicsRootSignatureAndDescriptorSet(g_linearDepthSig_.GetRootSignature(), &g_descSet_);

		// DrawCall
		pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCmdList->IASetVertexBuffers(0, 0, nullptr);
		pCmdList->IASetIndexBuffer(nullptr);
		pCmdList->DrawInstanced(3, 1, 0, 0);
	}

	// LightingPass
	{
		auto thisProd = g_rrProducers_[passNo++];
		sl12::RenderResource* pInputs[] = {
			g_rrManager_.GetRenderResourceFromID(thisProd->GetInputIds()[0]),		// GBuffer0
			g_rrManager_.GetRenderResourceFromID(thisProd->GetInputIds()[1]),		// GBuffer1
			g_rrManager_.GetRenderResourceFromID(thisProd->GetInputIds()[2]),		// GBuffer2
			g_rrManager_.GetRenderResourceFromID(thisProd->GetInputIds()[3]),		// LinearDepth
		};
		sl12::RenderResource* pOutput = g_rrManager_.GetRenderResourceFromID(thisProd->GetOutputIds()[0]);

		// バリア
		g_rrManager_.BarrierAllResources(mainCmdList, thisProd);

		// PSOとルートシグネチャを設定
		pCmdList->SetPipelineState(g_tiledLightPso_.GetPSO());

		// デスクリプタテーブル設定
		g_descSet_.Reset();
		g_tiledLightSig_.SetDescriptor(&g_descSet_, "CbScene", curCB.cbv_);
		g_tiledLightSig_.SetDescriptor(&g_descSet_, "CbLightInfo", g_LightCB_.cbv_);
		g_tiledLightSig_.SetDescriptor(&g_descSet_, "texGBuffer0", *pInputs[0]->GetSrv());
		g_tiledLightSig_.SetDescriptor(&g_descSet_, "texGBuffer1", *pInputs[1]->GetSrv());
		g_tiledLightSig_.SetDescriptor(&g_descSet_, "texGBuffer2", *pInputs[2]->GetSrv());
		g_tiledLightSig_.SetDescriptor(&g_descSet_, "texLinearDepth", *pInputs[3]->GetSrv());
		g_tiledLightSig_.SetDescriptor(&g_descSet_, "rLightPosBuffer", curLightPosBV);
		g_tiledLightSig_.SetDescriptor(&g_descSet_, "rLightColorBuffer", g_LightColorBV_);
		g_tiledLightSig_.SetDescriptor(&g_descSet_, "rwFinal", *pOutput->GetUav(0));

		mainCmdList.SetComputeRootSignatureAndDescriptorSet(g_tiledLightSig_.GetRootSignature(), &g_descSet_);

		// DrawCall
		pCmdList->Dispatch(kWindowWidth / kTileWidth, kWindowHeight / kTileWidth, 1);
	}

	// Clear & Projection Hash Pass
	{
		auto thisProd = g_rrProducers_[passNo++];
		sl12::RenderResource* pInput = g_rrManager_.GetRenderResourceFromID(thisProd->GetInputIds()[0]);
		sl12::RenderResource* pOutput = g_rrManager_.GetRenderResourceFromID(thisProd->GetOutputIds()[0]);

		// バリア
		g_rrManager_.BarrierAllResources(mainCmdList, thisProd);

		// Clear
		{
			// PSOとルートシグネチャを設定
			pCmdList->SetPipelineState(g_clearHashPso_.GetPSO());

			// デスクリプタテーブル設定
			g_descSet_.Reset();
			g_clearHashSig_.SetDescriptor(&g_descSet_, "CbScene", curCB.cbv_);
			g_clearHashSig_.SetDescriptor(&g_descSet_, "rwProjectHash", *pOutput->GetUav());

			mainCmdList.SetComputeRootSignatureAndDescriptorSet(g_clearHashSig_.GetRootSignature(), &g_descSet_);

			// DrawCall
			pCmdList->Dispatch(kWindowWidth / kTileWidth, kWindowHeight / kTileWidth, 1);
		}

		// Projection
		{
			// PSOとルートシグネチャを設定
			pCmdList->SetPipelineState(g_projectHashPso_.GetPSO());

			// デスクリプタテーブル設定
			g_descSet_.Reset();
			g_projectHashSig_.SetDescriptor(&g_descSet_, "CbScene", curCB.cbv_);
			g_projectHashSig_.SetDescriptor(&g_descSet_, "CbWaterInfo", curWaterCB.cbv_);
			g_projectHashSig_.SetDescriptor(&g_descSet_, "texLinearDepth", *pInput->GetSrv());
			g_projectHashSig_.SetDescriptor(&g_descSet_, "rwProjectHash", *pOutput->GetUav());

			mainCmdList.SetComputeRootSignatureAndDescriptorSet(g_projectHashSig_.GetRootSignature(), &g_descSet_);

			// DrawCall
			pCmdList->Dispatch(kWindowWidth / kTileWidth, kWindowHeight / kTileWidth, 1);
		}
	}

	// Resolve Hash Pass
	{
		auto thisProd = g_rrProducers_[passNo++];
		sl12::RenderResource* pInputs[] = {
			g_rrManager_.GetRenderResourceFromID(thisProd->GetInputIds()[0]),
			g_rrManager_.GetRenderResourceFromID(thisProd->GetInputIds()[1]),
		};
		sl12::RenderResource* pOutput = g_rrManager_.GetRenderResourceFromID(thisProd->GetOutputIds()[0]);

		D3D12_CPU_DESCRIPTOR_HANDLE rtv = pOutput->GetRtv()->GetDescInfo().cpuHandle;

		// バリア
		g_rrManager_.BarrierAllResources(mainCmdList, thisProd);

		// 画面クリア
		pCmdList->ClearRenderTargetView(rtv, pOutput->GetTexture()->GetTextureDesc().clearColor, 0, nullptr);

		// レンダーターゲット設定
		pCmdList->OMSetRenderTargets(1, &rtv, false, nullptr);

		// PSOとルートシグネチャを設定
		pCmdList->SetPipelineState(g_resolveHashPso_.GetPSO());

		// デスクリプタテーブル設定
		g_descSet_.Reset();
		g_resolveHashSig_.SetDescriptor(&g_descSet_, "CbScene", curCB.cbv_);
		g_resolveHashSig_.SetDescriptor(&g_descSet_, "CbWaterInfo", curWaterCB.cbv_);
		g_resolveHashSig_.SetDescriptor(&g_descSet_, "texSceneColor", *pInputs[0]->GetSrv());
		g_resolveHashSig_.SetDescriptor(&g_descSet_, "texProjectHash", *pInputs[1]->GetSrv());

		mainCmdList.SetGraphicsRootSignatureAndDescriptorSet(g_resolveHashSig_.GetRootSignature(), &g_descSet_);

		// DrawCall
		pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCmdList->IASetVertexBuffers(0, 0, nullptr);
		pCmdList->IASetIndexBuffer(nullptr);
		pCmdList->DrawInstanced(3, 1, 0, 0);
	}

	// Temporal Reprojection Pass
	{
		auto thisProd = g_rrProducers_[passNo++];
		sl12::RenderResource* pInputs[] = {
			g_rrManager_.GetRenderResourceFromID(thisProd->GetInputIds()[0]),
			g_rrManager_.GetRenderResourceFromID(thisProd->GetInputIds()[1]),
		};
		sl12::RenderResource* pOutput = g_rrManager_.GetRenderResourceFromID(thisProd->GetOutputIds()[0]);

		auto rtv = pOutput->GetRtv()->GetDescInfo().cpuHandle;

		// バリア
		g_rrManager_.BarrierAllResources(mainCmdList, thisProd);

		// レンダーターゲット設定
		pCmdList->OMSetRenderTargets(1, &rtv, false, nullptr);

		// PSOとルートシグネチャを設定
		pCmdList->SetPipelineState(g_reprojectPso_.GetPSO());

		// デスクリプタテーブル設定
		g_descSet_.Reset();
		g_reprojectSig_.SetDescriptor(&g_descSet_, "CbScene", curCB.cbv_);
		g_reprojectSig_.SetDescriptor(&g_descSet_, "CbWaterInfo", curWaterCB.cbv_);
		g_reprojectSig_.SetDescriptor(&g_descSet_, "texPrevReflection", *pInputs[0]->GetSrv());
		g_reprojectSig_.SetDescriptor(&g_descSet_, "texProjectHash", *pInputs[1]->GetSrv());

		mainCmdList.SetGraphicsRootSignatureAndDescriptorSet(g_reprojectSig_.GetRootSignature(), &g_descSet_);

		// DrawCall
		auto vb = g_WaterVBV_.GetView();
		auto ib = g_WaterIBV_.GetView();
		pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCmdList->IASetVertexBuffers(0, 1, &vb);
		pCmdList->IASetIndexBuffer(&ib);
		pCmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
	}

	// Water Pass
	{
		auto thisProd = g_rrProducers_[passNo++];
		sl12::RenderResource* pInput = g_rrManager_.GetRenderResourceFromID(thisProd->GetInputIds()[0]);
		sl12::RenderResource* pOutputs[] = {
			g_rrManager_.GetRenderResourceFromID(thisProd->GetOutputIds()[0]),
			g_rrManager_.GetRenderResourceFromID(thisProd->GetOutputIds()[1]),
		};

		auto rtv = pOutputs[0]->GetRtv()->GetDescInfo().cpuHandle;
		auto dsv = pOutputs[1]->GetDsv()->GetDescInfo().cpuHandle;

		// バリア
		g_rrManager_.BarrierAllResources(mainCmdList, thisProd);

		// レンダーターゲット設定
		pCmdList->OMSetRenderTargets(1, &rtv, false, &dsv);

		// PSOとルートシグネチャを設定
		pCmdList->SetPipelineState(g_waterPso_.GetPSO());

		// デスクリプタテーブル設定
		g_descSet_.Reset();
		g_waterSig_.SetDescriptor(&g_descSet_, "CbScene", curCB.cbv_);
		g_waterSig_.SetDescriptor(&g_descSet_, "CbWaterInfo", curWaterCB.cbv_);
		g_waterSig_.SetDescriptor(&g_descSet_, "texSSPR", *pInput->GetSrv());
		g_waterSig_.SetDescriptor(&g_descSet_, "texNormal", g_WaveNormalTex_.srv_);
		g_waterSig_.SetDescriptor(&g_descSet_, "samLinear", g_sampler_);

		mainCmdList.SetGraphicsRootSignatureAndDescriptorSet(g_waterSig_.GetRootSignature(), &g_descSet_);

		// DrawCall
		auto vb = g_WaterVBV_.GetView();
		auto ib = g_WaterIBV_.GetView();
		pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCmdList->IASetVertexBuffers(0, 1, &vb);
		pCmdList->IASetIndexBuffer(&ib);
		pCmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
	}

	// BlurPass
	{
		auto thisProd = g_rrProducers_[passNo++];
		sl12::RenderResource* pInputs[] = {
			g_rrManager_.GetRenderResourceFromID(thisProd->GetInputIds()[0]),		// LightResult
			g_rrManager_.GetRenderResourceFromID(thisProd->GetInputIds()[1]),		// LinearDepth
		};
		sl12::RenderResource* pTemp = g_rrManager_.GetRenderResourceFromID(thisProd->GetTempIds()[0]);

		D3D12_CPU_DESCRIPTOR_HANDLE tempRtv = pTemp->GetRtv()->GetDescInfo().cpuHandle;

		// バリア
		g_rrManager_.BarrierAllResources(mainCmdList, thisProd);
		auto tempPrevStates = thisProd->GetTempPrevStates();
		mainCmdList.TransitionBarrier(pTemp->GetTexture(), tempPrevStates[0], D3D12_RESOURCE_STATE_RENDER_TARGET);

		//// X軸方向
		// レンダーターゲット設定
		pCmdList->OMSetRenderTargets(1, &tempRtv, false, nullptr);

		// PSOとルートシグネチャを設定
		pCmdList->SetPipelineState(g_blurXPassPso_.GetPSO());

		// デスクリプタテーブル設定
		g_descSet_.Reset();
		g_blurXPassSig_.SetDescriptor(&g_descSet_, "CbGaussBlur", g_BlurCB_.cbv_);
		g_blurXPassSig_.SetDescriptor(&g_descSet_, "texSource", *pInputs[0]->GetSrv());
		g_blurXPassSig_.SetDescriptor(&g_descSet_, "texLinearDepth", *pInputs[1]->GetSrv());
		g_blurXPassSig_.SetDescriptor(&g_descSet_, "samLinearClamp", g_samLinearClamp_);

		mainCmdList.SetGraphicsRootSignatureAndDescriptorSet(g_blurXPassSig_.GetRootSignature(), &g_descSet_);

		// DrawCall
		pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCmdList->IASetVertexBuffers(0, 0, nullptr);
		pCmdList->IASetIndexBuffer(nullptr);
		pCmdList->DrawInstanced(3, 1, 0, 0);


		//// Y軸方向
		// バリア
		mainCmdList.TransitionBarrier(pTemp->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		// 一応、画面クリア
		const float kClearColor[] = { 0.0f, 0.0f, 0.6f, 1.0f };
		pCmdList->ClearRenderTargetView(rtvHandle, kClearColor, 0, nullptr);

		// レンダーターゲット設定
		pCmdList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

		// PSOとルートシグネチャを設定
		pCmdList->SetPipelineState(g_blurYPassPso_.GetPSO());

		// デスクリプタテーブル設定
		g_descSet_.Reset();
		g_blurYPassSig_.SetDescriptor(&g_descSet_, "CbGaussBlur", g_BlurCB_.cbv_);
		g_blurYPassSig_.SetDescriptor(&g_descSet_, "texSource", *pTemp->GetSrv());
		g_blurYPassSig_.SetDescriptor(&g_descSet_, "texLinearDepth", *pInputs[1]->GetSrv());
		g_blurYPassSig_.SetDescriptor(&g_descSet_, "samLinearClamp", g_samLinearClamp_);

		mainCmdList.SetGraphicsRootSignatureAndDescriptorSet(g_blurYPassSig_.GetRootSignature(), &g_descSet_);

		// DrawCall
		pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCmdList->IASetVertexBuffers(0, 0, nullptr);
		pCmdList->IASetIndexBuffer(nullptr);
		pCmdList->DrawInstanced(3, 1, 0, 0);
	}

	QueryPerformanceCounter(&end_time);
	g_perfCount_.QuadPart = end_time.QuadPart - start_time.QuadPart;

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
	ret = g_copyCmdList_.Initialize(&g_Device_, &g_Device_.GetCopyQueue());
	assert(ret);
	ret = InitializeAssets();
	assert(ret);
	ret = InitializeRenderResource();
	assert(ret);

	QueryPerformanceFrequency(&g_frequency_);

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

		g_Device_.WaitPresent();

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
