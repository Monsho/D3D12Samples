#pragma once

#include <sl12/util.h>


namespace sl12
{
	class Device;

	static const u32 kCbvMax = 16;
	static const u32 kSrvMax = 48;
	static const u32 kUavMax = 16;
	static const u32 kSamplerMax = 16;

	class DescriptorSet
	{
	private:
		template<u32 Num>
		struct Handles
		{
			D3D12_CPU_DESCRIPTOR_HANDLE	cpuHandles[Num];
			u32							maxCount;

			void Reset()
			{
				memset(cpuHandles, 0, sizeof(cpuHandles));
				maxCount = 0;
			}

			void SetHandle(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
			{
				assert(index < Num);
				cpuHandles[index] = handle;
				maxCount = std::max<u32>(maxCount, index + 1);
			}
		};

	public:
		DescriptorSet()
		{}
		~DescriptorSet()
		{}

		void Reset();

		inline void SetVsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetVsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetVsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetPsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetPsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetPsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetPsUav(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetGsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetGsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetGsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetHsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetHsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetHsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetDsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetDsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetDsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetCsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetCsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetCsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetCsUav(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetMsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetMsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetMsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetAsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetAsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
		inline void SetAsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);

		// getter
		const Handles<kCbvMax>& GetVsCbv() const { return vsCbv_; }
		const Handles<kSrvMax>& GetVsSrv() const { return vsSrv_; }
		const Handles<kSamplerMax>& GetVsSampler() const { return vsSampler_; }
		const Handles<kCbvMax>& GetPsCbv() const { return psCbv_; }
		const Handles<kSrvMax>& GetPsSrv() const { return psSrv_; }
		const Handles<kSamplerMax>& GetPsSampler() const { return psSampler_; }
		const Handles<kUavMax>& GetPsUav() const { return psUav_; }
		const Handles<kCbvMax>& GetGsCbv() const { return gsCbv_; }
		const Handles<kSrvMax>& GetGsSrv() const { return gsSrv_; }
		const Handles<kSamplerMax>& GetGsSampler() const { return gsSampler_; }
		const Handles<kCbvMax>& GetHsCbv() const { return hsCbv_; }
		const Handles<kSrvMax>& GetHsSrv() const { return hsSrv_; }
		const Handles<kSamplerMax>& GetHsSampler() const { return hsSampler_; }
		const Handles<kCbvMax>& GetDsCbv() const { return dsCbv_; }
		const Handles<kSrvMax>& GetDsSrv() const { return dsSrv_; }
		const Handles<kSamplerMax>& GetDsSampler() const { return dsSampler_; }
		const Handles<kCbvMax>& GetCsCbv() const { return csCbv_; }
		const Handles<kSrvMax>& GetCsSrv() const { return csSrv_; }
		const Handles<kSamplerMax>& GetCsSampler() const { return csSampler_; }
		const Handles<kUavMax>& GetCsUav() const { return csUav_; }
		const Handles<kCbvMax>& GetMsCbv() const { return msCbv_; }
		const Handles<kSrvMax>& GetMsSrv() const { return msSrv_; }
		const Handles<kSamplerMax>& GetMsSampler() const { return msSampler_; }
		const Handles<kCbvMax>& GetAsCbv() const { return asCbv_; }
		const Handles<kSrvMax>& GetAsSrv() const { return asSrv_; }
		const Handles<kSamplerMax>& GetAsSampler() const { return asSampler_; }

	private:
		Handles<kCbvMax>		vsCbv_;
		Handles<kSrvMax>		vsSrv_;
		Handles<kSamplerMax>	vsSampler_;
		Handles<kCbvMax>		psCbv_;
		Handles<kSrvMax>		psSrv_;
		Handles<kSamplerMax>	psSampler_;
		Handles<kUavMax>		psUav_;
		Handles<kCbvMax>		gsCbv_;
		Handles<kSrvMax>		gsSrv_;
		Handles<kSamplerMax>	gsSampler_;
		Handles<kCbvMax>		hsCbv_;
		Handles<kSrvMax>		hsSrv_;
		Handles<kSamplerMax>	hsSampler_;
		Handles<kCbvMax>		dsCbv_;
		Handles<kSrvMax>		dsSrv_;
		Handles<kSamplerMax>	dsSampler_;
		Handles<kCbvMax>		csCbv_;
		Handles<kSrvMax>		csSrv_;
		Handles<kSamplerMax>	csSampler_;
		Handles<kUavMax>		csUav_;
		Handles<kCbvMax>		msCbv_;
		Handles<kSrvMax>		msSrv_;
		Handles<kSamplerMax>	msSampler_;
		Handles<kCbvMax>		asCbv_;
		Handles<kSrvMax>		asSrv_;
		Handles<kSamplerMax>	asSampler_;
	};	// class DescriptorSet

	inline void DescriptorSet::Reset()
	{
		vsCbv_.Reset();
		vsSrv_.Reset();
		vsSampler_.Reset();
		psCbv_.Reset();
		psSrv_.Reset();
		psSampler_.Reset();
		psUav_.Reset();
		gsCbv_.Reset();
		gsSrv_.Reset();
		gsSampler_.Reset();
		hsCbv_.Reset();
		hsSrv_.Reset();
		hsSampler_.Reset();
		dsCbv_.Reset();
		dsSrv_.Reset();
		dsSampler_.Reset();
		csCbv_.Reset();
		csSrv_.Reset();
		csSampler_.Reset();
		csUav_.Reset();
		msCbv_.Reset();
		msSrv_.Reset();
		msSampler_.Reset();
		asCbv_.Reset();
		asSrv_.Reset();
		asSampler_.Reset();
	}

	inline void DescriptorSet::SetVsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		vsCbv_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetVsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		vsSrv_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetVsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		vsSampler_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetPsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		psCbv_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetPsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		psSrv_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetPsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		psSampler_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetPsUav(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		psUav_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetGsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		gsCbv_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetGsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		gsSrv_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetGsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		gsSampler_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetHsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		hsCbv_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetHsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		hsSrv_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetHsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		hsSampler_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetDsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		dsCbv_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetDsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		dsSrv_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetDsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		dsSampler_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetCsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		csCbv_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetCsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		csSrv_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetCsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		csSampler_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetCsUav(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		csUav_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetMsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		msCbv_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetMsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		msSrv_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetMsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		msSampler_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetAsCbv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		asCbv_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetAsSrv(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		asSrv_.SetHandle(index, handle);
	}
	inline void DescriptorSet::SetAsSampler(u32 index, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
	{
		asSampler_.SetHandle(index, handle);
	}

}	// namespace sl12

//	EOF
