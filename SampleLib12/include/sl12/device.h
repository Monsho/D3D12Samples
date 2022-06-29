#pragma once

#include <array>
#include <list>
#include <memory>
#include <mutex>
#include <sl12/util.h>
#include <sl12/descriptor_heap.h>
#include <sl12/death_list.h>

#define LatestDevice	ID3D12Device6

namespace sl12
{
	class CommandQueue;
	class Swapchain;
	class DescriptorHeap;
	class DescriptorAllocator;
	class GlobalDescriptorHeap;
	class CommandList;
	class Texture;
	class TextureView;
	class CopyRingBuffer;

	struct IRenderCommand
	{
		virtual ~IRenderCommand()
		{}

		virtual void LoadCommand(CommandList* pCmdlist) = 0;
	};	// struct IRenderCommand

	struct DummyTex
	{
		enum Type
		{
			Black,
			White,
			FlatNormal,

			Max
		};
	};	// struct DummyTex

	class Device
	{
	public:
		Device();
		~Device();

		bool Initialize(HWND hWnd, u32 screenWidth, u32 screenHeight, const std::array<u32, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES>& numDescs, ColorSpaceType csType = ColorSpaceType::Rec709);
		void Destroy();

		void Present(int syncInterval = 1);

		void WaitDrawDone();
		void WaitPresent();

		bool CreateDummyTextures(CommandList* pCmdList);

		void SyncKillObjects(bool bForce = false)
		{
			if (!bForce)
			{
				deathList_.SyncKill();
			}
			else
			{
				deathList_.Destroy();
			}
		}
		void PendingKill(PendingKillItem* p)
		{
			deathList_.PendingKill(p);
		}
		template <typename T>
		void KillObject(T* p)
		{
			deathList_.KillObject<T>(p);
		}

		void AddRenderCommand(std::unique_ptr<IRenderCommand>& rc)
		{
			std::lock_guard<std::mutex> lock(renderCommandMutex_);
			renderCommands_.push_back(std::move(rc));
		}
		void LoadRenderCommands(CommandList* pCmdlist)
		{
			std::lock_guard<std::mutex> lock(renderCommandMutex_);
			for (auto&& rc : renderCommands_)
			{
				rc->LoadCommand(pCmdlist);
			}
			renderCommands_.clear();
		}

		// getter
		IDXGIFactory4*	GetFactoryDep()
		{
			return pFactory_;
		}
		LatestDevice* GetLatestDeviceDep()
		{
			return pLatestDevice_;
		}
		ID3D12Device*	GetDeviceDep()
		{
			return pDevice_;
		}
		ID3D12Device5*	GetDxrDeviceDep()
		{
			return pDxrDevice_;
		}
		bool			IsDxrSupported() const
		{
			return isDxrSupported_;
		}
		ColorSpaceType	GetColorSpaceType() const
		{
			return colorSpaceType_;
		}
		RECT			GetDesktopCoordinates() const
		{
			return desktopCoordinates_;
		}
		float			GetMinLuminance() const
		{
			return minLuminance_;
		}
		float			GetMaxLuminance() const
		{
			return maxLuminance_;
		}
		float			GetMaxFullFrameLuminance() const
		{
			return maxFullFrameLuminance_;
		}
		CommandQueue&	GetGraphicsQueue()
		{
			return *pGraphicsQueue_;
		}
		CommandQueue&	GetComputeQueue()
		{
			return *pComputeQueue_;
		}
		CommandQueue&	GetCopyQueue()
		{
			return *pCopyQueue_;
		}
		GlobalDescriptorHeap& GetGlobalViewDescriptorHeap()
		{
			return *pGlobalViewDescHeap_;
		}
		DescriptorAllocator& GetViewDescriptorHeap()
		{
			return *pViewDescHeap_;
		}
		DescriptorAllocator& GetSamplerDescriptorHeap()
		{
			return *pSamplerDescHeap_;
		}
		DescriptorAllocator& GetRtvDescriptorHeap()
		{
			return *pRtvDescHeap_;
		}
		DescriptorAllocator& GetDsvDescriptorHeap()
		{
			return *pDsvDescHeap_;
		}
		DescriptorInfo& GetDefaultViewDescInfo()
		{
			return defaultViewDescInfo_;
		}
		DescriptorInfo& GetDefaultSamplerDescInfo()
		{
			return defaultSamplerDescInfo_;
		}
		Swapchain&		GetSwapchain()
		{
			return *pSwapchain_;
		}

		Texture* GetDummyTexture(DummyTex::Type type)
		{
			return dummyTextures_[type].get();
		}
		TextureView* GetDummyTextureView(DummyTex::Type type)
		{
			return dummyTextureViews_[type].get();
		}

		void CopyToBuffer(CommandList* pCmdList, Buffer* pDstBuffer, u32 dstOffset, const void* pSrcData, u32 srcSize);

	private:
		IDXGIFactory7*	pFactory_{ nullptr };
		IDXGIAdapter4*	pAdapter_{ nullptr };
		IDXGIOutput6*	pOutput_{ nullptr };

		LatestDevice*	pLatestDevice_{ nullptr };
		ID3D12Device*	pDevice_{ nullptr };

		ID3D12Device5*	pDxrDevice_{ nullptr };
		bool			isDxrSupported_ = false;

		ColorSpaceType	colorSpaceType_ = ColorSpaceType::Rec709;
		RECT			desktopCoordinates_;
		float			minLuminance_ = 0.0f;
		float			maxLuminance_ = 0.0f;
		float			maxFullFrameLuminance_ = 0.0f;

		CommandQueue*	pGraphicsQueue_{ nullptr };
		CommandQueue*	pComputeQueue_{ nullptr };
		CommandQueue*	pCopyQueue_{ nullptr };

		GlobalDescriptorHeap*	pGlobalViewDescHeap_ = nullptr;
		DescriptorAllocator*	pViewDescHeap_ = nullptr;
		DescriptorAllocator*	pSamplerDescHeap_ = nullptr;
		DescriptorAllocator*	pRtvDescHeap_ = nullptr;
		DescriptorAllocator*	pDsvDescHeap_ = nullptr;

		DescriptorInfo	defaultViewDescInfo_;
		DescriptorInfo	defaultSamplerDescInfo_;

		Swapchain*		pSwapchain_{ nullptr };

		ID3D12Fence*	pFence_{ nullptr };
		u32				fenceValue_{ 0 };
		HANDLE			fenceEvent_{ nullptr };

		std::vector<std::unique_ptr<Texture>>		dummyTextures_;
		std::vector<std::unique_ptr<TextureView>>	dummyTextureViews_;

		DeathList		deathList_;

		std::mutex									renderCommandMutex_;
		std::list<std::unique_ptr<IRenderCommand>>	renderCommands_;

		std::unique_ptr<CopyRingBuffer>				pRingBuffer_;
	};	// class Device

}	// namespace sl12

//	EOF
