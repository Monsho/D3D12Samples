#include <sl12/sampler.h>

#include <sl12/device.h>
#include <sl12/descriptor.h>
#include <sl12/descriptor_heap.h>

namespace sl12
{
	//----
	bool Sampler::Initialize(Device* pDev, const D3D12_SAMPLER_DESC& desc)
	{
		descInfo_ = pDev->GetSamplerDescriptorHeap().Allocate();
		if (!descInfo_.IsValid())
		{
			return false;
		}

		pDev->GetDeviceDep()->CreateSampler(&desc, descInfo_.cpuHandle);

		samplerDesc_ = desc;

		return true;
	}

	//----
	void Sampler::Destroy()
	{
		descInfo_.Free();
	}

}	// namespace sl12

//	EOF
