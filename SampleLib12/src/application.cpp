#include "sl12/application.h"



namespace sl12
{
	Random	GlobalRandom;

	namespace
	{
		Application* g_AppInst = nullptr;

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

			if (g_AppInst)
			{
				g_AppInst->Input(message, wParam, lParam);
			}

			// Handle any messages the switch statement didn't.
			return DefWindowProc(hWnd, message, wParam, lParam);
		}

		// Window�̏�����
		HWND InitializeWindow(HINSTANCE hInstance, int nCmdShow, int screenWidth, int screenHeight)
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

			RECT windowRect = { 0, 0, screenWidth, screenHeight };
			AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

			// Create the window and store a handle to it.
			HWND hWnd = CreateWindowEx(NULL,
				L"WindowClass1",
				L"D3D12Sample",
				WS_OVERLAPPEDWINDOW,
				300,
				300,
				windowRect.right - windowRect.left,
				windowRect.bottom - windowRect.top,
				NULL,		// We have no parent window, NULL.
				NULL,		// We aren't using menus, NULL.
				hInstance,
				NULL);		// We aren't using multiple windows, NULL.

			ShowWindow(hWnd, nCmdShow);

			return hWnd;
		}

	}

	//--------------------------------------------------
	// �R���X�g���N�^
	//--------------------------------------------------
	Application::Application(HINSTANCE hInstance, int nCmdShow, int screenWidth, int screenHeight)
	{
		hInstance_ = hInstance;
		screenWidth_ = screenWidth;
		screenHeight_ = screenHeight;

		// Window�̏�����
		hWnd_ = InitializeWindow(hInstance, nCmdShow, screenWidth, screenHeight);

		CpuTimer::Initialize();

		// D3D12�f�o�C�X�̏�����
		std::array<uint32_t, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> kDescNums
		{ 65535, 128, 256, 64 };
		auto isInitDevice = device_.Initialize(hWnd_, screenWidth, screenHeight, kDescNums);
		assert(isInitDevice);

		// �悭�g���T���v���[��������Ă���
		{
			D3D12_SAMPLER_DESC desc{};
			desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			desc.MaxLOD = FLT_MAX;
			auto isInitSampler = pointWrapSampler_.Initialize(&device_, desc);
			assert(isInitSampler);

			desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			isInitSampler = linearWrapSampler_.Initialize(&device_, desc);
			assert(isInitSampler);

			desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			isInitSampler = pointClampSampler_.Initialize(&device_, desc);
			assert(isInitSampler);

			desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			isInitSampler = linearClampSampler_.Initialize(&device_, desc);
			assert(isInitSampler);
		}
	}

	//--------------------------------------------------
	// �f�X�g���N�^
	//--------------------------------------------------
	Application::~Application()
	{
		pointWrapSampler_.Destroy();
		pointClampSampler_.Destroy();
		linearWrapSampler_.Destroy();
		linearClampSampler_.Destroy();

		device_.Destroy();
		if (hWnd_) CloseWindow(hWnd_);
	}

	//--------------------------------------------------
	// �A�v���P�[�V���������s����
	//--------------------------------------------------
	int Application::Run()
	{
		g_AppInst = this;
		if (!Initialize())
		{
			return -1;
		}

		deltaTime_ = sl12::CpuTimer();

		// ���C�����[�v
		MSG msg = { 0 };
		while (true)
		{
			sl12::CpuTimer time = sl12::CpuTimer::CurrentTime();

			// Process any messages in the queue.
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);

				if (msg.message == WM_QUIT)
					break;
			}

			Execute();

			deltaTime_ = sl12::CpuTimer::CurrentTime() - time;
		}

		Finalize();
		g_AppInst = nullptr;

		return static_cast<char>(msg.wParam);
	}

}	// namespace sl12

//	EOF
