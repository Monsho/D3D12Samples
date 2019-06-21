#pragma once

#include <sl12/util.h>
#include <sl12/descriptor_heap.h>


namespace sl12
{
	class Device;

	class Sampler
	{
	public:
		Sampler()
		{}
		~Sampler()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, const D3D12_SAMPLER_DESC& desc);
		void Destroy();

		// getter
		const D3D12_SAMPLER_DESC& GetSamplerDesc() const { return samplerDesc_; }
		DescriptorInfo& GetDescInfo() { return descInfo_; }

	private:
		D3D12_SAMPLER_DESC	samplerDesc_{};
		DescriptorInfo		descInfo_;
	};	// class Sampler

}	// namespace sl12

//	EOF
