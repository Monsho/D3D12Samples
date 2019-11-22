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
#include <sl12/default_states.h>
#include <sl12/root_signature.h>
#include <sl12/pipeline_state.h>
#include <sl12/descriptor_set.h>
#include <sl12/shader.h>
#include <sl12/gui.h>
#include <DirectXTex.h>
#include <windowsx.h>

#include "file.h"


namespace
{
	static const wchar_t* kWindowTitle = L"D3D12Sample";
	static const int kWindowWidth = 1920;
	static const int kWindowHeight = 1080;
	//static const DXGI_FORMAT	kDepthFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
	static const DXGI_FORMAT	kDepthFormat = DXGI_FORMAT_D32_FLOAT;
	static const uint32_t kMaxTriangle = 10000000;

	struct CompressVertex
	{
		float		position[3];
		uint32_t	color;
	};
	struct NoCompressVertex
	{
		float		position[3];
		float		color[4];
	};

	HWND	g_hWnd_;

	sl12::Device		g_Device_;
	sl12::CommandList	g_mainCmdList_;
	sl12::CommandList	g_mainCmdLists_[sl12::Swapchain::kMaxBuffer];
	sl12::CommandList*	g_pNextCmdList_ = nullptr;
	sl12::CommandList	g_copyCmdList_;

	sl12::Texture			g_DepthBuffer_;
	sl12::DepthStencilView	g_DepthBufferView_;

	static const uint32_t		kMaxCBs = sl12::Swapchain::kMaxBuffer;
	sl12::Buffer				g_CBScenes_[kMaxCBs];
	void*						g_pCBSceneBuffers_[kMaxCBs] = { nullptr };
	sl12::ConstantBufferView	g_CBSceneViews_[kMaxCBs];

	sl12::Buffer			g_vbuffers_[2];
	sl12::VertexBufferView	g_vbufferViews_[2];

	sl12::Shader			g_VShader_, g_PShader_;

	sl12::RootSignature		g_rootSig_;
	sl12::GraphicsPipelineState		g_compressPipeline_;
	sl12::GraphicsPipelineState		g_noCompressPipeline_;
	sl12::DescriptorSet		g_descSet_;

	ID3D12QueryHeap*		g_pTimestampQuery_[sl12::Swapchain::kMaxBuffer] = { nullptr };
	ID3D12Resource*			g_pTimestampBuffer_[sl12::Swapchain::kMaxBuffer] = { nullptr };

	sl12::Gui		g_Gui_;
	sl12::InputData	g_InputData_{};
	bool			g_IsNoCompressVertex = false;
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

	g_copyCmdList_.Reset();

	// 深度バッファを作成
	{
		sl12::TextureDesc texDesc;
		texDesc.dimension = sl12::TextureDimension::Texture2D;
		texDesc.width = kWindowWidth;
		texDesc.height = kWindowHeight;
		texDesc.format = kDepthFormat;
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
	{
		for (int i = 0; i < _countof(g_CBScenes_); i++)
		{
			if (!g_CBScenes_[i].Initialize(&g_Device_, sizeof(DirectX::XMFLOAT4X4), 1, sl12::BufferUsage::ConstantBuffer, true, false))
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
		auto frandom = [](float minv, float maxv) {
			float t = (float)rand() / (float)(RAND_MAX - 1);
			return minv * (1.0f - t) + maxv * t;
		};
		auto posRandom = [&](float* p) {
			p[0] = frandom(-20.0f, 20.0f);
			p[1] = frandom(-20.0f, 20.0f);
			p[2] = frandom(-20.0f, 20.0f);
		};
		auto colRandom = [&](float* c) {
			c[0] = frandom(0.0f, 1.0f);
			c[1] = frandom(0.0f, 1.0f);
			c[2] = frandom(0.0f, 1.0f);
			c[3] = 1.0f;
		};
		auto toU32Col = [](float* c) {
			return ((uint32_t)(c[3] * 255.0f) << 24) | ((uint32_t)(c[2] * 255.0f) << 16) | ((uint32_t)(c[1] * 255.0f) << 8) | ((uint32_t)(c[0] * 255.0f) << 0);
		};

		CompressVertex* pCV = new CompressVertex[kMaxTriangle * 3];
		NoCompressVertex* pNV = new NoCompressVertex[kMaxTriangle * 3];
		for (uint32_t i = 0; i < kMaxTriangle; ++i)
		{
			uint32_t index = i * 3;
			// カラー
			colRandom(pNV[index].color);
			memcpy(pNV[index + 1].color, pNV[index].color, sizeof(pNV[index].color));
			memcpy(pNV[index + 2].color, pNV[index].color, sizeof(pNV[index].color));
			pCV[index].color = pCV[index + 1].color = pCV[index + 2].color = toU32Col(pNV[index].color);
			// 座標
			float p[3];
			posRandom(p);
			pNV[index].position[0] = pCV[index].position[0] = p[0];
			pNV[index].position[1] = pCV[index].position[1] = p[1] + 0.2f;
			pNV[index].position[2] = pCV[index].position[2] = p[2];
			pNV[index + 1].position[0] = pCV[index + 1].position[0] = p[0] - 0.2f;
			pNV[index + 1].position[1] = pCV[index + 1].position[1] = p[1] - 0.2f;
			pNV[index + 1].position[2] = pCV[index + 1].position[2] = p[2];
			pNV[index + 2].position[0] = pCV[index + 2].position[0] = p[0] + 0.2f;
			pNV[index + 2].position[1] = pCV[index + 2].position[1] = p[1] - 0.2f;
			pNV[index + 2].position[2] = pCV[index + 2].position[2] = p[2];
		}

		if (!g_vbuffers_[0].Initialize(&g_Device_, sizeof(CompressVertex) * kMaxTriangle * 3, sizeof(CompressVertex), sl12::BufferUsage::VertexBuffer, false, false))
		{
			return false;
		}
		if (!g_vbufferViews_[0].Initialize(&g_Device_, &g_vbuffers_[0]))
		{
			return false;
		}
		g_vbuffers_[0].UpdateBuffer(&g_Device_, &g_copyCmdList_, pCV, sizeof(CompressVertex) * kMaxTriangle * 3);

		if (!g_vbuffers_[1].Initialize(&g_Device_, sizeof(NoCompressVertex) * kMaxTriangle * 3, sizeof(NoCompressVertex), sl12::BufferUsage::VertexBuffer, false, false))
		{
			return false;
		}
		if (!g_vbufferViews_[1].Initialize(&g_Device_, &g_vbuffers_[1]))
		{
			return false;
		}
		g_vbuffers_[1].UpdateBuffer(&g_Device_, &g_copyCmdList_, pNV, sizeof(NoCompressVertex) * kMaxTriangle * 3);

		delete[] pCV;
		delete[] pNV;
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

	// ルートシグネチャを作成
	{
		if (!g_rootSig_.Initialize(&g_Device_, &g_VShader_, &g_PShader_, nullptr, nullptr, nullptr))
		{
			return false;
		}
	}

	// PSOを作成
	{
		D3D12_INPUT_ELEMENT_DESC elementDescs0[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, sizeof(float) * 3, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		D3D12_INPUT_ELEMENT_DESC elementDescs1[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, sizeof(float) * 3, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		sl12::GraphicsPipelineStateDesc psoDesc{};

		psoDesc.rasterizer = sl12::DefaultRasterizerStateStandard();
		psoDesc.rasterizer.cullMode = D3D12_CULL_MODE_NONE;

		psoDesc.blend.isAlphaToCoverageEnable = false;
		psoDesc.blend.isIndependentBlend = false;
		psoDesc.blend.sampleMask = UINT_MAX;
		psoDesc.blend.rtDesc[0] = sl12::DefaultRenderTargetBlendNone();

		psoDesc.depthStencil = sl12::DefaultDepthStateEnableEnable();

		psoDesc.pRootSignature = &g_rootSig_;

		psoDesc.pVS = &g_VShader_;
		psoDesc.pPS = &g_PShader_;

		psoDesc.inputLayout.numElements = _countof(elementDescs0);
		psoDesc.inputLayout.pElements = elementDescs0;

		psoDesc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		psoDesc.numRTVs = 1;
		psoDesc.rtvFormats[0] = g_Device_.GetSwapchain().GetRenderTargetView(0)->GetFormat();
		psoDesc.dsvFormat = g_DepthBufferView_.GetFormat();
		psoDesc.multisampleCount = 1;

		if (!g_compressPipeline_.Initialize(&g_Device_, psoDesc))
		{
			return false;
		}

		psoDesc.inputLayout.numElements = _countof(elementDescs1);
		psoDesc.inputLayout.pElements = elementDescs1;

		if (!g_noCompressPipeline_.Initialize(&g_Device_, psoDesc))
		{
			return false;
		}
	}

	// タイムスタンプクエリとバッファ
	for (int i = 0; i < sl12::Swapchain::kMaxBuffer; ++i)
	{
		D3D12_QUERY_HEAP_DESC qd{};
		qd.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		qd.Count = 2;
		qd.NodeMask = 1;

		auto hr = pDev->CreateQueryHeap(&qd, IID_PPV_ARGS(&g_pTimestampQuery_[i]));
		if (FAILED(hr))
		{
			return false;
		}

		D3D12_HEAP_PROPERTIES prop{};
		prop.Type = D3D12_HEAP_TYPE_READBACK;
		prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		prop.CreationNodeMask = 1;
		prop.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC rd{};
		rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		rd.Alignment = 0;
		rd.Width = sizeof(uint64_t) * 2;
		rd.Height = 1;
		rd.DepthOrArraySize = 1;
		rd.MipLevels = 1;
		rd.Format = DXGI_FORMAT_UNKNOWN;
		rd.SampleDesc.Count = 1;
		rd.SampleDesc.Quality = 0;
		rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		rd.Flags = D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

		hr = pDev->CreateCommittedResource(&prop, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&g_pTimestampBuffer_[i]));
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

	g_copyCmdList_.Close();
	g_copyCmdList_.Execute();

	sl12::Fence fence;
	fence.Initialize(&g_Device_);
	fence.Signal(g_copyCmdList_.GetParentQueue());
	fence.WaitSignal();

	fence.Destroy();

	return true;
}

void DestroyAssets()
{
	g_Gui_.Destroy();

	for (auto& v : g_pTimestampBuffer_) sl12::SafeRelease(v);
	for (auto& v : g_pTimestampQuery_) sl12::SafeRelease(v);

	g_noCompressPipeline_.Destroy();
	g_compressPipeline_.Destroy();
	g_rootSig_.Destroy();

	g_VShader_.Destroy();
	g_PShader_.Destroy();

	for (auto& v : g_vbufferViews_) v.Destroy();
	for (auto& v : g_vbuffers_) v.Destroy();

	for (auto& v : g_CBSceneViews_) v.Destroy();
	for (auto& v : g_CBScenes_)
	{
		v.Unmap();
		v.Destroy();
	}

	g_DepthBufferView_.Destroy();
	g_DepthBuffer_.Destroy();
}

void RenderScene()
{
	if (g_pNextCmdList_)
		g_pNextCmdList_->Execute();

	int32_t prevFrameIndex = g_Device_.GetSwapchain().GetFrameIndex();
	int32_t frameIndex = (prevFrameIndex + 1) % sl12::Swapchain::kMaxBuffer;
	g_pNextCmdList_ = &g_mainCmdLists_[frameIndex];

	g_Gui_.BeginNewFrame(g_pNextCmdList_, kWindowWidth, kWindowHeight, g_InputData_);

	// GUI
	{
		if (ImGui::Button("Change"))
		{
			g_IsNoCompressVertex = !g_IsNoCompressVertex;
		}

		ImGui::Text(g_IsNoCompressVertex ? "Float32 Color" : "U32 Color");

		auto buffer = g_pTimestampBuffer_[prevFrameIndex];
		void* p = nullptr;
		D3D12_RANGE range{0, sizeof(uint64_t) * 2};
		buffer->Map(0, &range, &p);
		uint64_t* t = (uint64_t*)p;
		uint64_t time = t[1] - t[0];
		buffer->Unmap(0, nullptr);
		uint64_t freq = g_Device_.GetGraphicsQueue().GetTimestampFrequency();
		float ms = (float)time / ((float)freq / 1000.0f);

		ImGui::Text("Time : %f (ms)", ms);
	}

	g_pNextCmdList_->Reset();

	for (auto& v : g_vbuffers_)
	{
		g_pNextCmdList_->TransitionBarrier(&v, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	}

	auto scTex = g_Device_.GetSwapchain().GetCurrentTexture(1);
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_Device_.GetSwapchain().GetCurrentDescHandle(1);
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = g_DepthBufferView_.GetDescInfo().cpuHandle;
	ID3D12GraphicsCommandList* pCmdList = g_pNextCmdList_->GetCommandList();

	g_pNextCmdList_->TransitionBarrier(scTex, D3D12_RESOURCE_STATE_RENDER_TARGET);
	g_pNextCmdList_->TransitionBarrier(&g_DepthBuffer_, D3D12_RESOURCE_STATE_DEPTH_WRITE);

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
	{
		static float sAngle = 0.0f;
		static const float kXZLength = 80.0f;
		void* p0 = g_pCBSceneBuffers_[frameIndex];
		DirectX::XMFLOAT4X4* pMtxs = reinterpret_cast<DirectX::XMFLOAT4X4*>(p0);
		DirectX::FXMVECTOR eye = DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(sinf(sAngle * DirectX::XM_PI / 180.0f) * kXZLength, 5.0f, cosf(sAngle * DirectX::XM_PI / 180.0f) * kXZLength));
		DirectX::FXMVECTOR focus = DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
		DirectX::FXMVECTOR up = DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f));
		DirectX::XMMATRIX mtxV = DirectX::XMMatrixLookAtRH(eye, focus, up);
		DirectX::XMMATRIX mtxP = DirectX::XMMatrixPerspectiveFovRH(60.0f * DirectX::XM_PI / 180.0f, (float)kWindowWidth / (float)kWindowHeight, 1.0f, 100000.0f);
		DirectX::XMMATRIX mtxVP = mtxV * mtxP;
		DirectX::XMStoreFloat4x4(pMtxs, mtxVP);

		sAngle += 1.0f;
	}

	pCmdList->EndQuery(g_pTimestampQuery_[frameIndex], D3D12_QUERY_TYPE_TIMESTAMP, 0);
	{
		// レンダーターゲット設定
		pCmdList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

		// PSO設定
		if (!g_IsNoCompressVertex)
		{
			pCmdList->SetPipelineState(g_compressPipeline_.GetPSO());
		}
		else
		{
			pCmdList->SetPipelineState(g_noCompressPipeline_.GetPSO());
		}

		g_descSet_.Reset();
		g_descSet_.SetVsCbv(0, g_CBSceneViews_[frameIndex].GetDescInfo().cpuHandle);

		g_pNextCmdList_->SetGraphicsRootSignatureAndDescriptorSet(&g_rootSig_, &g_descSet_);

		// DrawCall
		D3D12_VERTEX_BUFFER_VIEW views0[] = { g_vbufferViews_[0].GetView() };
		D3D12_VERTEX_BUFFER_VIEW views1[] = { g_vbufferViews_[1].GetView() };
		pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		if (!g_IsNoCompressVertex)
		{
			pCmdList->IASetVertexBuffers(0, _countof(views0), views0);
		}
		else
		{
			pCmdList->IASetVertexBuffers(0, _countof(views1), views1);
		}
		pCmdList->DrawInstanced(kMaxTriangle * 3, 1, 0, 0);
	}
	pCmdList->EndQuery(g_pTimestampQuery_[frameIndex], D3D12_QUERY_TYPE_TIMESTAMP, 1);

	ImGui::Render();

	g_pNextCmdList_->TransitionBarrier(scTex, D3D12_RESOURCE_STATE_PRESENT);

	pCmdList->ResolveQueryData(g_pTimestampQuery_[frameIndex], D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, g_pTimestampBuffer_[frameIndex], 0);

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
