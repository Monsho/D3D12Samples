#pragma once

#include <sl12/util.h>


namespace sl12
{
	class Device;
	class Descriptor;

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
		Descriptor* GetDesc() { return pDesc_; }

	private:
		D3D12_SAMPLER_DESC	samplerDesc_{};
		Descriptor*	pDesc_{ nullptr };
	};	// class Sampler

}	// namespace sl12

//	EOF
