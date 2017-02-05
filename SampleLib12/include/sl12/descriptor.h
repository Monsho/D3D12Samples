#pragma once

#include <sl12/util.h>


namespace sl12
{
	class Device;

	class Descriptor
	{
		friend class DescriptorHeap;

	public:
		Descriptor()
		{}
		~Descriptor()
		{
			Destroy();
		}

		void Destroy();

		void Release();

		// getter
		D3D12_CPU_DESCRIPTOR_HANDLE	GetCpuHandle() { return cpuHandle_; }
		D3D12_GPU_DESCRIPTOR_HANDLE	GetGpuHandle() { return gpuHandle_; }
		u32 GetIndex() const { return index_; }

	private:
		DescriptorHeap*				pParentHeap_{ nullptr };
		Descriptor*					pPrev_{ nullptr };
		Descriptor*					pNext_{ nullptr };
		D3D12_CPU_DESCRIPTOR_HANDLE	cpuHandle_{ 0 };
		D3D12_GPU_DESCRIPTOR_HANDLE	gpuHandle_{ 0 };
		u32							index_{ 0 };
	};	// class Descriptor

}	// namespace sl12

//	EOF
