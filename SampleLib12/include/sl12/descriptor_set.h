#pragma once

#include <sl12/util.h>


namespace sl12
{
	class Device;

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

		// getter
		const Handles<16>& GetVsCbv() const { return vsCbv_; }
		const Handles<48>& GetVsSrv() const { return vsSrv_; }
		const Handles<16>& GetVsSampler() const { return vsSampler_; }
		const Handles<16>& GetPsCbv() const { return psCbv_; }
		const Handles<48>& GetPsSrv() const { return psSrv_; }
		const Handles<16>& GetPsSampler() const { return psSampler_; }
		const Handles<16>& GetPsUav() const { return psUav_; }
		const Handles<16>& GetGsCbv() const { return gsCbv_; }
		const Handles<48>& GetGsSrv() const { return gsSrv_; }
		const Handles<16>& GetGsSampler() const { return gsSampler_; }
		const Handles<16>& GetHsCbv() const { return hsCbv_; }
		const Handles<48>& GetHsSrv() const { return hsSrv_; }
		const Handles<16>& GetHsSampler() const { return hsSampler_; }
		const Handles<16>& GetDsCbv() const { return dsCbv_; }
		const Handles<48>& GetDsSrv() const { return dsSrv_; }
		const Handles<16>& GetDsSampler() const { return dsSampler_; }
		const Handles<16>& GetCsCbv() const { return csCbv_; }
		const Handles<48>& GetCsSrv() const { return csSrv_; }
		const Handles<16>& GetCsSampler() const { return csSampler_; }
		const Handles<16>& GetCsUav() const { return csUav_; }

	private:
		Handles<16>		vsCbv_;
		Handles<48>		vsSrv_;
		Handles<16>		vsSampler_;
		Handles<16>		psCbv_;
		Handles<48>		psSrv_;
		Handles<16>		psSampler_;
		Handles<16>		psUav_;
		Handles<16>		gsCbv_;
		Handles<48>		gsSrv_;
		Handles<16>		gsSampler_;
		Handles<16>		hsCbv_;
		Handles<48>		hsSrv_;
		Handles<16>		hsSampler_;
		Handles<16>		dsCbv_;
		Handles<48>		dsSrv_;
		Handles<16>		dsSampler_;
		Handles<16>		csCbv_;
		Handles<48>		csSrv_;
		Handles<16>		csSampler_;
		Handles<16>		csUav_;
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

}	// namespace sl12

//	EOF
