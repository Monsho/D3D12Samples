#pragma once

#define NOMINMAX
#include "device.h"
#include "sampler.h"


namespace sl12
{

	/*********************************************************//**
	 * @brief アプリケーションクラス
	*************************************************************/
	class Application
	{
	public:
		Application(HINSTANCE hInstance, int nCmdShow, int screenWidth, int screenHeight, ColorSpaceType csType = ColorSpaceType::Rec709);
		virtual ~Application();

		int Run();

		// virtual
		virtual bool Initialize() = 0;
		virtual bool Execute() = 0;
		virtual void Finalize() = 0;
		virtual int Input(UINT message, WPARAM wParam, LPARAM lParam) { return 0; }

	private:

	protected:
		HINSTANCE		hInstance_ = 0;
		HWND			hWnd_ = 0;
		int				screenWidth_ = 0, screenHeight_ = 0;

		sl12::CpuTimer	deltaTime_;

		sl12::Device	device_;

		sl12::Sampler	pointWrapSampler_;
		sl12::Sampler	pointClampSampler_;
		sl12::Sampler	linearWrapSampler_;
		sl12::Sampler	linearClampSampler_;
	};	// class Application

}	// namespace sl12

//	EOF
