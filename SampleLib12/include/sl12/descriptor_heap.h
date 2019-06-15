#pragma once

#include <sl12/util.h>
#include <mutex>
#include <atomic>
#include <vector>
#include <map>


namespace sl12
{
	class Device;
	class Descriptor;

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
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandleStart_;
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandleStart_;
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

}	// namespace sl12

//	EOF
