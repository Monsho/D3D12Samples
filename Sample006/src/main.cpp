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
#include <DirectXTex.h>
#include <windowsx.h>


namespace
{
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

	sl12::Buffer				g_CBScenes_[kMaxFrameCount];
	void*						g_pCBSceneBuffers_[kMaxFrameCount] = { nullptr };
	sl12::ConstantBufferView	g_CBSceneViews_[kMaxFrameCount];

	sl12::Sampler			g_sampler_;

	sl12::Shader			g_VShader_, g_PShader_;

	sl12::RootSignature			g_rootSigMesh_;
	sl12::GraphicsPipelineState	g_psoMesh_;

	sl12::File			g_meshFile_;
	sl12::MeshInstance	g_mesh_;

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

	// 定数バッファを作成
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
	if (!g_VShader_.Initialize(&g_Device_, sl12::ShaderType::Vertex, "data/VSMesh.cso"))
	{
		return false;
	}
	if (!g_PShader_.Initialize(&g_Device_, sl12::ShaderType::Pixel, "data/PSMesh.cso"))
	{
		return false;
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

	g_VShader_.Destroy();
	g_PShader_.Destroy();

	g_sampler_.Destroy();

	for (auto& v : g_CBSceneViews_)
	{
		v.Destroy();
	}
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
		static float sAngle = 90.0f;
		void* p0 = g_pCBSceneBuffers_[frameIndex];
		DirectX::XMFLOAT4X4* pMtxs = reinterpret_cast<DirectX::XMFLOAT4X4*>(p0);
		DirectX::XMMATRIX mtxW = DirectX::XMMatrixRotationY(sAngle * DirectX::XM_PI / 180.0f);
		DirectX::FXMVECTOR eye = DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(0.0f, 200.0f, 600.0f));
		DirectX::FXMVECTOR focus = DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
		DirectX::FXMVECTOR up = DirectX::XMLoadFloat3(&DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f));
		DirectX::XMMATRIX mtxV = DirectX::XMMatrixLookAtRH(eye, focus, up);
		DirectX::XMMATRIX mtxP = DirectX::XMMatrixPerspectiveFovRH(60.0f * DirectX::XM_PI / 180.0f, (float)kWindowWidth / (float)kWindowHeight, 1.0f, 10000.0f);
		DirectX::XMStoreFloat4x4(pMtxs + 0, mtxW);
		DirectX::XMStoreFloat4x4(pMtxs + 1, mtxV);
		DirectX::XMStoreFloat4x4(pMtxs + 2, mtxP);

		//sAngle += 1.0f;
	}

	{
		// レンダーターゲット設定
		pCmdList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

		// PSOとルートシグネチャを設定
		pCmdList->SetPipelineState(g_psoMesh_.GetPSO());
		pCmdList->SetGraphicsRootSignature(g_rootSigMesh_.GetRootSignature());

		// DescriptorHeapを設定
		ID3D12DescriptorHeap* pDescHeaps[] = {
			g_Device_.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetHeap(),
			g_Device_.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER).GetHeap()
		};
		pCmdList->SetDescriptorHeaps(_countof(pDescHeaps), pDescHeaps);
		pCmdList->SetGraphicsRootDescriptorTable(0, cbSceneDesc.GetGpuHandle());

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
		/*
		D3D12_VERTEX_BUFFER_VIEW views[] = { g_vbufferViews_[0].GetView(), g_vbufferViews_[1].GetView(), g_vbufferViews_[2].GetView() };
		pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCmdList->IASetVertexBuffers(0, _countof(views), views);
		pCmdList->IASetIndexBuffer(&g_ibufferView_.GetView());
		pCmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
		*/
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
