#pragma once

#include <sl12/util.h>


namespace sl12
{
	class Device;
	class Descriptor;
	class CommandQueue;

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

		bool Initialize(Device* pDev, CommandQueue* pQueue, HWND hWnd, uint32_t width, uint32_t height, DXGI_FORMAT format);
		void Destroy();

		void Present();

		// getter
		ID3D12Resource* GetCurrentRenderTarget() { return pRenderTargets_[frameIndex_]; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentDescHandle();
		int32_t GetFrameIndex() const { return frameIndex_; }

	private:
		IDXGISwapChain3*		pSwapchain_{ nullptr };
		Descriptor*				pRtvDescs_[kMaxBuffer]{ nullptr };
		ID3D12Resource*			pRenderTargets_[kMaxBuffer]{ nullptr };
		int32_t					frameIndex_{ 0 };
	};	// class Swapchain

}	// namespace sl12

//	EOF
