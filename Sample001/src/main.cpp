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
#include <sl12/root_signature.h>
#include <sl12/pipeline_state.h>
#include <sl12/descriptor_set.h>
#include <sl12/shader.h>
#include <DirectXTex.h>

#include "file.h"


namespace
{
	static const wchar_t* kWindowTitle = L"D3D12Sample";
	static const int kWindowWidth = 1920;
	static const int kWindowHeight = 1080;
	//static const DXGI_FORMAT	kDepthBufferFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
	//static const DXGI_FORMAT	kDepthViewFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	static const DXGI_FORMAT	kDepthBufferFormat = DXGI_FORMAT_R32_TYPELESS;
	static const DXGI_FORMAT	kDepthViewFormat = DXGI_FORMAT_D32_FLOAT;

	HWND	g_hWnd_;

	sl12::Device		g_Device_;
	sl12::CommandList	g_mainCmdList_;
	sl12::CommandList	g_mainCmdLists_[sl12::Swapchain::kMaxBuffer];
	sl12::CommandList*	g_pNextCmdList_ = nullptr;
	sl12::CommandList	g_copyCmdList_;

	sl12::Texture			g_RenderTarget_;
	sl12::RenderTargetView	g_RenderTargetView_;
	sl12::TextureView		g_RenderTargetTexView_;

	sl12::Texture			g_DepthBuffer_;
	sl12::DepthStencilView	g_DepthBufferView_;
	sl12::TextureView		g_DepthBufferTexView_;

	static const uint32_t		kMaxCBs = sl12::Swapchain::kMaxBuffer * 2;
	sl12::Buffer				g_CBScenes_[kMaxCBs];
	void*						g_pCBSceneBuffers_[kMaxCBs] = { nullptr };
	sl12::ConstantBufferView	g_CBSceneViews_[kMaxCBs];

	sl12::Buffer			g_vbuffers_[3];
	sl12::VertexBufferView	g_vbufferViews_[3];

	sl12::Buffer			g_ibuffer_;
	sl12::IndexBufferView	g_ibufferView_;

	sl12::Texture			g_texture_;
	sl12::TextureView		g_textureView_;
	sl12::Sampler			g_sampler_;

	sl12::Shader			g_VShader_, g_PShader_;
	sl12::Shader			g_VSScreenShader_, g_PSDispDepthShader_;

	sl12::RootSignature		g_rootSig_;

	sl12::GraphicsPipelineState	g_psoWriteS_;
	sl12::GraphicsPipelineState	g_psoUseS_;

	sl12::DescriptorSet		g_descSet_;

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

bool InitializeAssets()
{
	ID3D12Device* pDev = g_Device_.GetDeviceDep();

	// レンダーターゲットを作成
	{
		sl12::TextureDesc texDesc;
		texDesc.dimension = sl12::TextureDimension::Texture2D;
		texDesc.width = kWindowWidth;
		texDesc.height = kWindowHeight;
		texDesc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		texDesc.clearColor[0] = 0.0f;
		texDesc.clearColor[1] = 0.6f;
		texDesc.clearColor[2] = 0.0f;
		texDesc.clearColor[3] = 1.0f;
		texDesc.isRenderTarget = true;

		if (!g_RenderTarget_.Initialize(&g_Device_, texDesc))
		{
			return false;
		}

		if (!g_RenderTargetView_.Initialize(&g_Device_, &g_RenderTarget_))
		{
			return false;
		}

		if (!g_RenderTargetTexView_.Initialize(&g_Device_, &g_RenderTarget_))
		{
			return false;
		}
	}

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

		if (!g_DepthBufferTexView_.Initialize(&g_Device_, &g_DepthBuffer_))
		{
			return false;
		}
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

			g_pCBSceneBuffers_[i] = g_CBScenes_[i].Map(&g_mainCmdList_);
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
	{
		File texFile("data/icon.tga");

		if (!g_texture_.InitializeFromTGA(&g_Device_, &g_copyCmdList_, texFile.GetData(), texFile.GetSize(), 1, false))
		{
			return false;
		}
		if (!g_textureView_.Initialize(&g_Device_, &g_texture_))
		{
			return false;
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
	if (!g_VSScreenShader_.Initialize(&g_Device_, sl12::ShaderType::Vertex, "data/VSScreen.cso"))
	{
		return false;
	}
	if (!g_PSDispDepthShader_.Initialize(&g_Device_, sl12::ShaderType::Pixel, "data/PSDispDepth.cso"))
	{
		return false;
	}

	// ルートシグネチャを作成
	{
		if (!g_rootSig_.Initialize(&g_Device_, &g_VShader_, &g_PShader_, nullptr, nullptr, nullptr))
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

		sl12::GraphicsPipelineStateDesc desc{};

		desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
		desc.rasterizer.cullMode = D3D12_CULL_MODE_NONE;
		desc.rasterizer.isFrontCCW = false;
		desc.rasterizer.isDepthClipEnable = true;
		desc.multisampleCount = 1;

		desc.blend.sampleMask = UINT_MAX;
		desc.blend.rtDesc[0].isBlendEnable = false;
		desc.blend.rtDesc[0].srcBlendColor = D3D12_BLEND_ONE;
		desc.blend.rtDesc[0].dstBlendColor = D3D12_BLEND_ZERO;
		desc.blend.rtDesc[0].blendOpColor = D3D12_BLEND_OP_ADD;
		desc.blend.rtDesc[0].srcBlendAlpha = D3D12_BLEND_ONE;
		desc.blend.rtDesc[0].dstBlendAlpha = D3D12_BLEND_ZERO;
		desc.blend.rtDesc[0].blendOpAlpha = D3D12_BLEND_OP_ADD;
		desc.blend.rtDesc[0].writeMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		desc.depthStencil.isDepthEnable = true;
		desc.depthStencil.isDepthWriteEnable = true;
		desc.depthStencil.depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		desc.pRootSignature = &g_rootSig_;
		desc.pVS = &g_VShader_;
		desc.pPS = &g_PShader_;
		desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		desc.inputLayout.numElements = _countof(elementDescs);
		desc.inputLayout.pElements = elementDescs;
		desc.numRTVs = 0;
		desc.rtvFormats[desc.numRTVs++] = g_RenderTarget_.GetTextureDesc().format;
		desc.dsvFormat = g_DepthBuffer_.GetTextureDesc().format;

		if (!g_psoWriteS_.Initialize(&g_Device_, desc))
		{
			return false;
		}

		desc.inputLayout.numElements = 0;
		desc.inputLayout.pElements = nullptr;
		desc.pVS = &g_VSScreenShader_;
		desc.pPS = &g_PSDispDepthShader_;
		desc.depthStencil.isDepthEnable = false;
		desc.numRTVs = 0;
		desc.rtvFormats[desc.numRTVs++] = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.dsvFormat = DXGI_FORMAT_UNKNOWN;

		if (!g_psoUseS_.Initialize(&g_Device_, desc))
		{
			return false;
		}
	}

	return true;
}

void DestroyAssets()
{
	g_psoWriteS_.Destroy();
	g_psoUseS_.Destroy();

	g_rootSig_.Destroy();

	g_VSScreenShader_.Destroy();
	g_PSDispDepthShader_.Destroy();
	g_VShader_.Destroy();
	g_PShader_.Destroy();

	g_sampler_.Destroy();
	g_textureView_.Destroy();
	g_texture_.Destroy();

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

	g_DepthBufferTexView_.Destroy();
	g_DepthBufferView_.Destroy();
	g_DepthBuffer_.Destroy();

	g_RenderTargetTexView_.Destroy();
	g_RenderTargetView_.Destroy();
	g_RenderTarget_.Destroy();
}

void RenderScene()
{
	if (g_pNextCmdList_)
		g_pNextCmdList_->Execute();

	int32_t frameIndex = (g_Device_.GetSwapchain().GetFrameIndex() + 1) % sl12::Swapchain::kMaxBuffer;
	g_pNextCmdList_ = &g_mainCmdLists_[frameIndex];

	g_pNextCmdList_->Reset();

	static bool s_InitFrame = true;
	if (s_InitFrame)
	{
		g_pNextCmdList_->TransitionBarrier(&g_texture_, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		for (auto& v : g_vbuffers_)
		{
			g_pNextCmdList_->TransitionBarrier(&v, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		}
		g_pNextCmdList_->TransitionBarrier(&g_ibuffer_, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
		s_InitFrame = false;
	}

	auto scTex = g_Device_.GetSwapchain().GetCurrentTexture(1);
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_Device_.GetSwapchain().GetCurrentDescHandle(1);
	D3D12_CPU_DESCRIPTOR_HANDLE rtvOffHandle = g_RenderTargetView_.GetDescInfo().cpuHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = g_DepthBufferView_.GetDescInfo().cpuHandle;
	ID3D12GraphicsCommandList* pCmdList = g_pNextCmdList_->GetCommandList();

	g_pNextCmdList_->TransitionBarrier(scTex, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	g_pNextCmdList_->TransitionBarrier(&g_RenderTarget_, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
	g_pNextCmdList_->TransitionBarrier(&g_DepthBuffer_, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	// 画面クリア
	const float kClearColor[] = { 0.0f, 0.0f, 0.6f, 1.0f };
	pCmdList->ClearRenderTargetView(rtvHandle, kClearColor, 0, nullptr);
	pCmdList->ClearRenderTargetView(rtvOffHandle, g_RenderTarget_.GetTextureDesc().clearColor, 0, nullptr);
	pCmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, g_DepthBuffer_.GetTextureDesc().clearDepth, g_DepthBuffer_.GetTextureDesc().clearStencil, 0, nullptr);

	// Viewport + Scissor設定
	D3D12_VIEWPORT viewport{ 0.0f, 0.0f, (float)kWindowWidth, (float)kWindowHeight, 0.0f, 1.0f };
	D3D12_RECT scissor{ 0, 0, kWindowWidth, kWindowHeight };
	pCmdList->RSSetViewports(1, &viewport);
	pCmdList->RSSetScissorRects(1, &scissor);

	// Scene定数バッファを更新
	auto&& cbScene = g_CBSceneViews_[frameIndex];
	auto&& cbPrevScene = g_CBSceneViews_[frameIndex + sl12::Swapchain::kMaxBuffer];
	{
		static float sAngle = 0.0f;
		void* p0 = g_pCBSceneBuffers_[frameIndex];
		DirectX::XMFLOAT4X4* pMtxs = reinterpret_cast<DirectX::XMFLOAT4X4*>(p0);
		DirectX::XMMATRIX mtxW = DirectX::XMMatrixRotationY(sAngle * DirectX::XM_PI / 180.0f);
		DirectX::FXMVECTOR eye = DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(0.0f, 5.0f, 10.0f));
		DirectX::FXMVECTOR focus = DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
		DirectX::FXMVECTOR up = DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f));
		DirectX::XMMATRIX mtxV = DirectX::XMMatrixLookAtRH(eye, focus, up);
		DirectX::XMMATRIX mtxP = DirectX::XMMatrixPerspectiveFovRH(60.0f * DirectX::XM_PI / 180.0f, (float)kWindowWidth / (float)kWindowHeight, 1.0f, 100000.0f);
		DirectX::XMStoreFloat4x4(pMtxs + 0, mtxW);
		DirectX::XMStoreFloat4x4(pMtxs + 1, mtxV);
		DirectX::XMStoreFloat4x4(pMtxs + 2, mtxP);

		void* p1 = g_pCBSceneBuffers_[frameIndex + sl12::Swapchain::kMaxBuffer];
		memcpy(p1, p0, sizeof(DirectX::XMFLOAT4X4) * 3);
		pMtxs = reinterpret_cast<DirectX::XMFLOAT4X4*>(p1);
		mtxW = DirectX::XMMatrixTranslation(0.5f, 0.0f, 0.5f) * DirectX::XMMatrixScaling(4.0f, 2.0f, 1.0f);
		DirectX::XMStoreFloat4x4(pMtxs + 0, mtxW);

		sAngle += 1.0f;
	}

	{
		// レンダーターゲット設定
		pCmdList->OMSetRenderTargets(1, &rtvOffHandle, false, &dsvHandle);

		pCmdList->SetPipelineState(g_psoWriteS_.GetPSO());

		g_descSet_.Reset();
		g_descSet_.SetVsCbv(0, cbScene.GetDescInfo().cpuHandle);
		g_descSet_.SetPsSrv(0, g_textureView_.GetDescInfo().cpuHandle);
		g_descSet_.SetPsSampler(0, g_sampler_.GetDescInfo().cpuHandle);

		g_pNextCmdList_->SetGraphicsRootSignatureAndDescriptorSet(&g_rootSig_, &g_descSet_);

		// DrawCall
		D3D12_VERTEX_BUFFER_VIEW views[] = { g_vbufferViews_[0].GetView(), g_vbufferViews_[1].GetView(), g_vbufferViews_[2].GetView() };
		pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCmdList->IASetVertexBuffers(0, _countof(views), views);
		pCmdList->IASetIndexBuffer(&g_ibufferView_.GetView());
		pCmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
	}

	g_pNextCmdList_->TransitionBarrier(&g_RenderTarget_, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
	g_pNextCmdList_->TransitionBarrier(&g_DepthBuffer_, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);

	{
		// レンダーターゲット設定
		pCmdList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
		//pCmdList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

		pCmdList->SetPipelineState(g_psoUseS_.GetPSO());
		pCmdList->OMSetStencilRef(0xf);

		g_descSet_.Reset();
		g_descSet_.SetVsCbv(0, cbPrevScene.GetDescInfo().cpuHandle);
		g_descSet_.SetPsSrv(0, g_DepthBufferTexView_.GetDescInfo().cpuHandle);
		g_descSet_.SetPsSampler(0, g_sampler_.GetDescInfo().cpuHandle);

		g_pNextCmdList_->SetGraphicsRootSignatureAndDescriptorSet(&g_rootSig_, &g_descSet_);

		// DrawCall
		D3D12_VERTEX_BUFFER_VIEW views[] = { g_vbufferViews_[0].GetView(), g_vbufferViews_[1].GetView(), g_vbufferViews_[2].GetView() };
		pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCmdList->IASetVertexBuffers(0, _countof(views), views);
		pCmdList->IASetIndexBuffer(&g_ibufferView_.GetView());
		pCmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
	}

	g_pNextCmdList_->TransitionBarrier(scTex, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	g_pNextCmdList_->Close();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	InitWindow(hInstance, nCmdShow);

	std::array<uint32_t, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> kDescNums
	{ 100, 100, 20, 10 };
	auto ret = g_Device_.Initialize(g_hWnd_, kWindowWidth, kWindowHeight, kDescNums);
	assert(ret);
	ret = g_mainCmdList_.Initialize(&g_Device_, &g_Device_.GetGraphicsQueue());
	assert(ret);
	for (int i = 0; i < _countof(g_mainCmdLists_); ++i)
	{
		ret = g_mainCmdLists_[i].Initialize(&g_Device_, &g_Device_.GetGraphicsQueue());
		assert(ret);
	}
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

		g_Device_.WaitPresent();
		RenderScene();
		g_Device_.Present();
		g_Device_.WaitDrawDone();
	}

	g_Device_.WaitDrawDone();
	DestroyAssets();
	g_copyCmdList_.Destroy();
	for (int i = 0; i < _countof(g_mainCmdLists_); ++i)
	{
		g_mainCmdLists_[i].Destroy();
	}
	g_mainCmdList_.Destroy();
	g_Device_.Destroy();

	return static_cast<char>(msg.wParam);
}
