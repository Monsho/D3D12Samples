#include <sl12/descriptor_heap.h>

#include <sl12/device.h>
#include <sl12/descriptor.h>


namespace sl12
{
	//----
	bool DescriptorHeap::Initialize(Device* pDev, const D3D12_DESCRIPTOR_HEAP_DESC& desc)
	{
		auto hr = pDev->GetDeviceDep()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pHeap_));
		if (FAILED(hr))
		{
			return false;
		}

		pDescriptors_ = new Descriptor[desc.NumDescriptors + 2];
		pUsedList_ = pDescriptors_;
		pUsedList_->pPrev_ = pUsedList_->pNext_ = pUsedList_;
		pUnusedList_ = pDescriptors_ + 1;
		pUnusedList_->pPrev_ = pUnusedList_->pNext_ = pUnusedList_;

		heapDesc_ = desc;
		descSize_ = pDev->GetDeviceDep()->GetDescriptorHandleIncrementSize(desc.Type);

		// すべてUnusedListに接続
		Descriptor* p = pUnusedList_ + 1;
		D3D12_CPU_DESCRIPTOR_HANDLE hCpu = pHeap_->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE hGpu = pHeap_->GetGPUDescriptorHandleForHeapStart();
		for (u32 i = 0; i < desc.NumDescriptors; i++, p++, hCpu.ptr += descSize_, hGpu.ptr += descSize_)
		{
			p->pParentHeap_ = this;
			p->cpuHandle_ = hCpu;
			p->gpuHandle_ = hGpu;
			p->index_ = i;

			Descriptor* next = pUnusedList_->pNext_;
			pUnusedList_->pNext_ = next->pPrev_ = p;
			p->pPrev_ = pUnusedList_;
			p->pNext_ = next;
		}

		return true;
	}

	//----
	void DescriptorHeap::Destroy()
	{
		assert(pUsedList_->pNext_ == pUsedList_);
		delete[] pDescriptors_;
		SafeRelease(pHeap_);
	}

	//----
	Descriptor* DescriptorHeap::CreateDescriptor()
	{
		if (pUnusedList_->pNext_ == pUnusedList_)
		{
			return nullptr;
		}

		Descriptor* ret = pUnusedList_->pNext_;
		ret->pPrev_->pNext_ = ret->pNext_;
		ret->pNext_->pPrev_ = ret->pPrev_;
		ret->pNext_ = ret->pPrev_ = ret;

		return ret;
	}

	//----
	void DescriptorHeap::ReleaseDescriptor(Descriptor* p)
	{
		assert((pDescriptors_ <= p) && (p <= pDescriptors + 2));

		Descriptor* next = pUnusedList_->pNext_;
		pUnusedList_->pNext_ = next->pPrev_ = p;
		p->pPrev_ = pUnusedList_;
		p->pNext_ = next;
	}

}	// namespace sl12

//	EOF
