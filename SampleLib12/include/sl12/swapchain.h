#pragma once

#include <sl12/util.h>

#include <sl12/texture.h>
#include <sl12/texture_view.h>


namespace sl12
{
	class Device;
	class Descriptor;
	class CommandQueue;

	class Swapchain
	{
	public:
		static const u32	kMaxBuffer = 3;

	public:
		Swapchain()
		{}
		~Swapchain()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, CommandQueue* pQueue, HWND hWnd, uint32_t width, uint32_t height, DXGI_FORMAT format);
		void Destroy();

		void Present(int syncInterval);

		void WaitPresent();

		// getter
		IDXGISwapChain3* GetSwapchain() { return pSwapchain_; }
		Texture* GetTexture(int index) { return &textures_[index]; }
		Texture* GetCurrentTexture(int offset = 0) { return &textures_[(frameIndex_ + offset) % kMaxBuffer]; }
		RenderTargetView* GetRenderTargetView(int index) { return &views_[index]; }
		RenderTargetView* GetCurrentRenderTargetView(int offset = 0) { return &views_[(frameIndex_ + offset) % kMaxBuffer]; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetDescHandle(int index);
		D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentDescHandle(int offset = 0);
		int32_t GetFrameIndex() const { return frameIndex_; }

	private:
		IDXGISwapChain3*		pSwapchain_{ nullptr };
		Texture					textures_[kMaxBuffer];
		RenderTargetView		views_[kMaxBuffer];
		int32_t					frameIndex_{ 0 };
		HANDLE					swapchainEvent_;
	};	// class Swapchain

}	// namespace sl12

//	EOF
