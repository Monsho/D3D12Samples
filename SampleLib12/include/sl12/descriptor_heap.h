#pragma once

#include <sl12/util.h>
#include <mutex>
#include <atomic>
#include <vector>
#include <map>
#include <list>


namespace sl12
{
	class Device;
	class Descriptor;
	class CommandList;

	class DescriptorHeap
	{
	public:
		DescriptorHeap()
		{}
		~DescriptorHeap()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, const D3D12_DESCRIPTOR_HEAP_DESC& desc);
		void Destroy();

		Descriptor* CreateDescriptor();
		void ReleaseDescriptor(Descriptor* p);

		// getter
		ID3D12DescriptorHeap* GetHeap() { return pHeap_; }

	private:
		ID3D12DescriptorHeap*		pHeap_{ nullptr };
		Descriptor*					pDescriptors_{ nullptr };
		//Descriptor*					pUsedList_{ nullptr };
		Descriptor*					pUnusedList_{ nullptr };
		D3D12_DESCRIPTOR_HEAP_DESC	heapDesc_{};
		uint32_t					descSize_{ 0 };
		uint32_t					take_num_{ 0 };
	};	// class DescriptorHeap


	class DescriptorAllocator;

	struct DescriptorInfo
	{
		DescriptorAllocator*		pAllocator = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE	cpuHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE	gpuHandle;
		u32							index = 0;

		bool IsValid() const
		{
			return pAllocator != nullptr;
		}

		void Free();
	};	// struct DescriptorInfo

	class DescriptorAllocator
	{
	public:
		DescriptorAllocator()
		{}
		~DescriptorAllocator()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, const D3D12_DESCRIPTOR_HEAP_DESC& desc);
		void Destroy();

		DescriptorInfo Allocate();
		void Free(DescriptorInfo info);

	private:
		std::mutex					mutex_;
		ID3D12DescriptorHeap*		pHeap_ = nullptr;
		D3D12_DESCRIPTOR_HEAP_DESC	heapDesc_{};
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandleStart_;
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandleStart_;
		u8*							pUseFlags_ = nullptr;
		u32							descSize_ = 0;
		u32							allocCount_ = 0;
		u32							currentPosition_ = 0;
	};	// class DescriptorAllocator


	class DescriptorStack
	{
		friend class GlobalDescriptorHeap;

	public:
		DescriptorStack()
		{}
		~DescriptorStack()
		{}

		bool Allocate(u32 count, D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle);

		void Reset()
		{
			stackPosition_ = 0;
		}

	private:
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandleStart_;
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandleStart_;
		u32							descSize_ = 0;
		u32							stackMax_ = 0;
		u32							stackPosition_ = 0;
	};	// class DescriptorStack

	class DescriptorStackList
	{
	public:
		DescriptorStackList()
		{}
		~DescriptorStackList()
		{}

		bool Initialilze(class GlobalDescriptorHeap* parent);

		void Allocate(u32 count, D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle);

		void Reset();

	private:
		bool AddStack();

	private:
		class GlobalDescriptorHeap*		pParentHeap_ = nullptr;
		std::vector<DescriptorStack>	stacks_;
		u32								stackIndex_ = 0;
	};	// class DescriptorStackList

	class GlobalDescriptorHeap
	{
	public:
		GlobalDescriptorHeap()
		{}
		~GlobalDescriptorHeap()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, const D3D12_DESCRIPTOR_HEAP_DESC& desc);
		void Destroy();

		bool AllocateStack(DescriptorStack& stack, u32 count);

		// getter
		ID3D12DescriptorHeap* GetHeap() { return pHeap_; }

	private:
		std::mutex					mutex_;
		ID3D12DescriptorHeap*		pHeap_ = nullptr;
		D3D12_DESCRIPTOR_HEAP_DESC	heapDesc_{};
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandleStart_;
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandleStart_;
		u32							descSize_ = 0;
		u32							allocCount_ = 0;
	};	// class GlobalDescriptorHeap

	class SamplerDescriptorHeap
	{
	public:
		SamplerDescriptorHeap()
		{}
		~SamplerDescriptorHeap()
		{
			Destroy();
		}

		bool Initialize(Device* pDev);
		void Destroy();

		bool Allocate(u32 count, D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle);

		// getter
		ID3D12DescriptorHeap* GetHeap() { return pHeap_; }

	private:
		ID3D12DescriptorHeap*		pHeap_ = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandleStart_ = {};
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandleStart_ = {};
		u32							descSize_ = 0;
		u32							allocCount_ = 0;
	};	// class SamplerDescriptorHeap

	class SamplerDescriptorCache
	{
	private:
		struct MapItem
		{
			SamplerDescriptorHeap*		pHeap = nullptr;
			D3D12_CPU_DESCRIPTOR_HANDLE	cpuHandle;
			D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
		};	// struct MapItem

	public:
		SamplerDescriptorCache()
		{}
		~SamplerDescriptorCache()
		{}

		bool Initialize(Device* pDev);
		void Destroy();

		bool AllocateAndCopy(u32 count, D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandles, D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle);

		// getter
		ID3D12DescriptorHeap* GetHeap()
		{
			assert(pLastAllocateHeap_ != nullptr);
			return pLastAllocateHeap_->GetHeap();
		}

	private:
		bool AddHeap();

	private:
		Device*													pParentDevice_ = nullptr;
		std::vector<std::unique_ptr<SamplerDescriptorHeap>>		heapList_;
		SamplerDescriptorHeap*									pLastAllocateHeap_ = nullptr;
		SamplerDescriptorHeap*									pCurrentHeap_ = nullptr;
		std::map<u32, MapItem>									descCache_;
	};	// class SamplerDescriptorCache


	struct RaytracingDescriptorCount
	{
		u32		cbv = 0;
		u32		srv = 0;
		u32		uav = 0;
		u32		sampler = 0;

		u32 GetViewTotal() const
		{
			return cbv + srv + uav;
		}
	};	// struct RaytracingDescriptorCount

	class RaytracingDescriptorHeap
	{
	public:
		RaytracingDescriptorHeap()
		{}
		~RaytracingDescriptorHeap()
		{
			Destroy();
		}

		bool Initialize(
			Device* pDev,
			u32 bufferCount,
			u32 asCount,
			u32 globalCbvCount,
			u32 globalSrvCount,
			u32 globalUavCount,
			u32 globalSamplerCount,
			u32 materialCount);
		bool Initialize(
			Device* pDev,
			u32 bufferCount,
			u32 asCount,
			const RaytracingDescriptorCount& globalCount,
			const RaytracingDescriptorCount& localCount,
			u32 materialCount);
		void Destroy();

		void GetGlobalViewHandleStart(u32 frameIndex, D3D12_CPU_DESCRIPTOR_HANDLE& cpu, D3D12_GPU_DESCRIPTOR_HANDLE& gpu);
		void GetGlobalSamplerHandleStart(u32 frameIndex, D3D12_CPU_DESCRIPTOR_HANDLE& cpu, D3D12_GPU_DESCRIPTOR_HANDLE& gpu);
		void GetLocalViewHandleStart(u32 frameIndex, D3D12_CPU_DESCRIPTOR_HANDLE& cpu, D3D12_GPU_DESCRIPTOR_HANDLE& gpu);
		void GetLocalSamplerHandleStart(u32 frameIndex, D3D12_CPU_DESCRIPTOR_HANDLE& cpu, D3D12_GPU_DESCRIPTOR_HANDLE& gpu);

		bool CanResizeMaterialCount(u32 materialCount);

		// getter
		ID3D12DescriptorHeap* GetViewHeap() { return pViewHeap_; }
		ID3D12DescriptorHeap* GetSamplerHeap() { return pSamplerHeap_; }
		u32 GetViewDescSize() const { return viewDescSize_; }
		u32 GetSamplerDescSize() const { return samplerDescSize_; }
		u32 GetBufferCount() const { return bufferCount_; }
		u32 GetASCount() const { return asCount_; }
		u32 GetGlobalCbvCount() const { return globalCount_.cbv; }
		u32 GetGlobalSrvCount() const { return globalCount_.srv; }
		u32 GetGlobalUavCount() const { return globalCount_.uav; }
		u32 GetGlobalSamplerCount() const { return globalCount_.sampler; }
		u32 GetGlobalViewCount() const
		{
			return globalCount_.GetViewTotal();
		}
		u32 GetLocalCbvCount() const { return localCount_.cbv; }
		u32 GetLocalSrvCount() const { return localCount_.srv; }
		u32 GetLocalUavCount() const { return localCount_.uav; }
		u32 GetLocalSamplerCount() const { return localCount_.sampler; }
		u32 GetLocalViewCount() const
		{
			return localCount_.GetViewTotal();
		}

	private:
		ID3D12DescriptorHeap*		pViewHeap_ = nullptr;
		ID3D12DescriptorHeap*		pSamplerHeap_ = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE	viewCpuHandleStart_ = {};
		D3D12_GPU_DESCRIPTOR_HANDLE	viewGpuHandleStart_ = {};
		D3D12_CPU_DESCRIPTOR_HANDLE	samplerCpuHandleStart_ = {};
		D3D12_GPU_DESCRIPTOR_HANDLE	samplerGpuHandleStart_ = {};
		u32							viewDescMax_ = 0;
		u32							samplerDescMax_ = 0;
		u32							viewDescSize_ = 0;
		u32							samplerDescSize_ = 0;

		u32							bufferCount_ = 0;
		u32							asCount_ = 0;
		RaytracingDescriptorCount	globalCount_;
		RaytracingDescriptorCount	localCount_;
		u32							materialCount_ = 0;
	};	// class RaytracingDescriptorHeap

	class RaytracingDescriptorManager
	{
		struct KillPendingHeap
		{
			RaytracingDescriptorHeap*	pHeap = nullptr;
			int							killCount = 0;

			KillPendingHeap(RaytracingDescriptorHeap* h);

			bool Kill()
			{
				killCount--;
				if (killCount <= 0)
				{
					SafeDelete(pHeap);
					return true;
				}
				return false;
			}

			void ForceKill()
			{
				SafeDelete(pHeap);
			}
		};

	public:
		struct HandleStart
		{
			D3D12_CPU_DESCRIPTOR_HANDLE		viewCpuHandle;
			D3D12_GPU_DESCRIPTOR_HANDLE		viewGpuHandle;
			D3D12_CPU_DESCRIPTOR_HANDLE		samplerCpuHandle;
			D3D12_GPU_DESCRIPTOR_HANDLE		samplerGpuHandle;
		};	// struct HandleStart

	public:
		RaytracingDescriptorManager()
		{}
		~RaytracingDescriptorManager()
		{
			Destroy();
		}

		bool Initialize(
			Device* pDev,
			u32 renderCount,
			u32 asCount,
			u32 globalCbvCount,
			u32 globalSrvCount,
			u32 globalUavCount,
			u32 globalSamplerCount,
			u32 materialCount);
		bool Initialize(
			Device* pDev,
			u32 renderCount,
			u32 asCount,
			const RaytracingDescriptorCount& globalCount,
			const RaytracingDescriptorCount& localCount,
			u32 materialCount);
		void Destroy();

		void BeginNewFrame();

		bool ResizeMaterialCount(u32 materialCount);

		void SetHeapToCommandList(sl12::CommandList& cmdList);

		HandleStart IncrementGlobalHandleStart();
		HandleStart IncrementLocalHandleStart();

		u32 GetViewDescSize() const { return pCurrentHeap_->GetViewDescSize(); }
		u32 GetSamplerDescSize() const { return pCurrentHeap_->GetSamplerDescSize(); }

		u32 GetASCount() const { return pCurrentHeap_->GetASCount(); }
		u32 GetGlobalCbvCount() const { return pCurrentHeap_->GetGlobalCbvCount(); }
		u32 GetGlobalSrvCount() const { return pCurrentHeap_->GetGlobalSrvCount(); }
		u32 GetGlobalUavCount() const { return pCurrentHeap_->GetGlobalUavCount(); }
		u32 GetGlobalSamplerCount() const { return pCurrentHeap_->GetGlobalSamplerCount(); }

	private:
		Device*							pParentDevice_ = nullptr;
		RaytracingDescriptorHeap*		pCurrentHeap_ = nullptr;
		std::list<KillPendingHeap>		heapsBeforeKill_;
		u32								globalIndex_ = 0;
		u32								localIndex_ = 0;
	};	// class RaytracingDescriptorManager

}	// namespace sl12

//	EOF
