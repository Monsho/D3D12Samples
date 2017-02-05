#pragma once

#include <sl12/util.h>


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
		Descriptor*					pUsedList_{ nullptr };
		Descriptor*					pUnusedList_{ nullptr };
		D3D12_DESCRIPTOR_HEAP_DESC	heapDesc_{};
		uint32_t					descSize_{ 0 };
	};	// class DescriptorHeap

}	// namespace sl12

//	EOF
