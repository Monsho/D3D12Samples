#include <sl12/swapchain.h>

#include <sl12/device.h>
#include <sl12/descriptor.h>
#include <sl12/descriptor_heap.h>
#include <sl12/command_queue.h>


namespace sl12
{
	//----
	bool Swapchain::Initialize(Device* pDev, CommandQueue* pQueue, HWND hWnd, uint32_t width, uint32_t height)
	{
		bool enableHDR = pDev->GetColorSpaceType() != ColorSpaceType::Rec709;

		{
			DXGI_SWAP_CHAIN_DESC1 desc = {};
			desc.BufferCount = kMaxBuffer;
			desc.Width = width;
			desc.Height = height;
			desc.Format = enableHDR ? DXGI_FORMAT_R10G10B10A2_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			desc.SampleDesc.Count = 1;
			desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

			IDXGISwapChain1* pSwap;
			auto hr = pDev->GetFactoryDep()->CreateSwapChainForHwnd(pQueue->GetQueueDep(), hWnd, &desc, nullptr, nullptr, &pSwap);
			if (FAILED(hr))
			{
				return false;
			}

			hr = pSwap->QueryInterface(IID_PPV_ARGS(&pSwapchain_));
			if (FAILED(hr))
			{
				return false;
			}

			const float kChromaticityLevel = 50000.0f;
			const float kLuminanceLevel = 10000.0f;
			DXGI_HDR_METADATA_HDR10 MetaData;
			switch (pDev->GetColorSpaceType())
			{
			case ColorSpaceType::Rec709:
				pSwapchain_->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
				MetaData.RedPrimary[0] = (UINT16)(0.640f * kChromaticityLevel);
				MetaData.RedPrimary[1] = (UINT16)(0.330f * kChromaticityLevel);
				MetaData.GreenPrimary[0] = (UINT16)(0.300f * kChromaticityLevel);
				MetaData.GreenPrimary[1] = (UINT16)(0.600f * kChromaticityLevel);
				MetaData.BluePrimary[0] = (UINT16)(0.150f * kChromaticityLevel);
				MetaData.BluePrimary[1] = (UINT16)(0.060f * kChromaticityLevel);
				MetaData.WhitePoint[0] = (UINT16)(0.3127f * kChromaticityLevel);
				MetaData.WhitePoint[1] = (UINT16)(0.3290f * kChromaticityLevel);
				MetaData.MaxMasteringLuminance = (UINT)(pDev->GetMaxLuminance() * kLuminanceLevel);
				MetaData.MinMasteringLuminance = (UINT)(pDev->GetMinLuminance() * kLuminanceLevel);
				MetaData.MaxContentLightLevel = (UINT16)(100.0f);
				pSwapchain_->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(MetaData), &MetaData);
				break;
			case ColorSpaceType::Rec2020:
				pSwapchain_->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
				MetaData.RedPrimary[0] = (UINT16)(0.708f * kChromaticityLevel);
				MetaData.RedPrimary[1] = (UINT16)(0.292f * kChromaticityLevel);
				MetaData.GreenPrimary[0] = (UINT16)(0.170f * kChromaticityLevel);
				MetaData.GreenPrimary[1] = (UINT16)(0.797f * kChromaticityLevel);
				MetaData.BluePrimary[0] = (UINT16)(0.131f * kChromaticityLevel);
				MetaData.BluePrimary[1] = (UINT16)(0.046f * kChromaticityLevel);
				MetaData.WhitePoint[0] = (UINT16)(0.3127f * kChromaticityLevel);
				MetaData.WhitePoint[1] = (UINT16)(0.3290f * kChromaticityLevel);
				MetaData.MaxMasteringLuminance = (UINT)(pDev->GetMaxLuminance() * kLuminanceLevel);
				MetaData.MinMasteringLuminance = (UINT)(pDev->GetMinLuminance() * kLuminanceLevel);
				MetaData.MaxContentLightLevel = (UINT16)(2000.0f);
				MetaData.MaxFrameAverageLightLevel = (UINT16)(pDev->GetMaxFullFrameLuminance());
				pSwapchain_->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(MetaData), &MetaData);
				break;
			}

			frameIndex_ = pSwapchain_->GetCurrentBackBufferIndex();
			swapchainEvent_ = pSwapchain_->GetFrameLatencyWaitableObject();

			pSwap->Release();
		}

		// テクスチャを初期化する
		for (u32 i = 0; i < kMaxBuffer; i++)
		{
			if (!textures_[i].InitializeFromSwapchain(pDev, this, i))
			{
				return false;
			}

			if (!views_[i].Initialize(pDev, &textures_[i]))
			{
				return false;
			}
		}

		return true;
	}

	//----
	void Swapchain::Destroy()
	{
		for (auto& v : views_) v.Destroy();
		for (auto& v : textures_) v.Destroy();
		SafeRelease(pSwapchain_);
	}

	//----
	void Swapchain::Present(int syncInterval)
	{
		pSwapchain_->Present(syncInterval, 0);
		frameIndex_ = pSwapchain_->GetCurrentBackBufferIndex();
	}

	//----
	void Swapchain::WaitPresent()
	{
		WaitForSingleObjectEx(swapchainEvent_, 100, FALSE);
	}

	//----
	D3D12_CPU_DESCRIPTOR_HANDLE Swapchain::GetDescHandle(int index)
	{
		return views_[index].GetDescInfo().cpuHandle;
	}

	//----
	D3D12_CPU_DESCRIPTOR_HANDLE Swapchain::GetCurrentDescHandle(int offset)
	{
		int index = (frameIndex_ + offset) % kMaxBuffer;
		return GetDescHandle(index);
	}

}	// namespace sl12

//	EOF
