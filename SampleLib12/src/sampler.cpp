#include <sl12/sampler.h>

#include <sl12/device.h>
#include <sl12/descriptor.h>
#include <sl12/descriptor_heap.h>

namespace sl12
{
	//----
	bool Sampler::Initialize(Device* pDev, const D3D12_SAMPLER_DESC& desc)
	{
		pDesc_ = pDev->GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER).CreateDescriptor();
		if (!pDesc_)
		{
			return false;
		}
		descInfo_ = pDev->GetSamplerDescriptorHeap().Allocate();
		if (!descInfo_.IsValid())
		{
			return false;
		}

		pDev->GetDeviceDep()->CreateSampler(&desc, pDesc_->GetCpuHandle());
		pDev->GetDeviceDep()->CreateSampler(&desc, descInfo_.cpuHandle);

		samplerDesc_ = desc;

		return true;
	}

	//----
	void Sampler::Destroy()
	{
		descInfo_.Free();
		SafeRelease(pDesc_);
	}

}	// namespace sl12

//	EOF
