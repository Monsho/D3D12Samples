#include <sl12/root_signature.h>

#include <sl12/device.h>
#include <sl12/shader.h>
#include <sl12/descriptor_set.h>


namespace sl12
{
	//----
	bool RootSignature::Initialize(Device* pDev, const RootSignatureDesc& desc)
	{
		static const u32 kMaxParameters = 64;
		D3D12_DESCRIPTOR_RANGE ranges[kMaxParameters];
		D3D12_ROOT_PARAMETER rootParameters[kMaxParameters];

		auto getRangeFunc = [&](u32 index)
		{
			static const D3D12_DESCRIPTOR_RANGE_TYPE kType[] = {
				D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
				D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
				D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
			};
			return kType[desc.pParameters[index].type];
		};
		auto getShaderVisFunc = [&](u32 index)
		{
			switch (desc.pParameters[index].shaderVisibility)
			{
			case ShaderVisibility::Vertex: return D3D12_SHADER_VISIBILITY_VERTEX;
			case ShaderVisibility::Pixel: return D3D12_SHADER_VISIBILITY_PIXEL;
			case ShaderVisibility::Geometry: return D3D12_SHADER_VISIBILITY_GEOMETRY;
			case ShaderVisibility::Domain: return D3D12_SHADER_VISIBILITY_DOMAIN;
			case ShaderVisibility::Hull: return D3D12_SHADER_VISIBILITY_HULL;
			default: return D3D12_SHADER_VISIBILITY_ALL;
			}
		};

		for (u32 i = 0; i < desc.numParameters; ++i)
		{
			ranges[i].RangeType = getRangeFunc(i);
			ranges[i].NumDescriptors = 1;
			ranges[i].BaseShaderRegister = desc.pParameters[i].registerIndex;
			ranges[i].RegisterSpace = 0;
			ranges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			rootParameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameters[i].DescriptorTable.NumDescriptorRanges = 1;
			rootParameters[i].DescriptorTable.pDescriptorRanges = &ranges[i];
			rootParameters[i].ShaderVisibility = getShaderVisFunc(i);
		}

		D3D12_ROOT_SIGNATURE_DESC rd{};
		rd.NumParameters = desc.numParameters;
		rd.pParameters = rootParameters;
		rd.NumStaticSamplers = 0;
		rd.pStaticSamplers = nullptr;
		rd.Flags = desc.flags;

		ID3DBlob* pSignature{ nullptr };
		ID3DBlob* pError{ nullptr };
		auto hr = D3D12SerializeRootSignature(&rd, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError);
		if (FAILED(hr))
		{
			sl12::SafeRelease(pSignature);
			sl12::SafeRelease(pError);
			return false;
		}

		hr = pDev->GetDeviceDep()->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&pRootSignature_));
		sl12::SafeRelease(pSignature);
		sl12::SafeRelease(pError);
		if (FAILED(hr))
		{
			return false;
		}

		return true;
	}

	//----
	bool RootSignature::Initialize(Device* pDev, const D3D12_ROOT_SIGNATURE_DESC& desc)
	{
		ID3DBlob* blob = nullptr;
		ID3DBlob* error = nullptr;
		bool ret = true;

		auto hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
		if (FAILED(hr))
		{
			ret = false;
			goto D3D_ERROR;
		}

		hr = pDev->GetDeviceDep()->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&pRootSignature_));
		if (FAILED(hr))
		{
			ret = false;
			goto D3D_ERROR;
		}

	D3D_ERROR:
		sl12::SafeRelease(blob);
		sl12::SafeRelease(error);
		return ret;
	}

	//----
	bool RootSignature::Initialize(Device* pDev, Shader* vs, Shader* ps, Shader* gs, Shader* hs, Shader* ds)
	{
		/*
		D3D12_DESCRIPTOR_RANGE_TYPE RangeType;
		UINT NumDescriptors;
		UINT BaseShaderRegister;
		UINT RegisterSpace;
		UINT OffsetInDescriptorsFromTableStart;
		*/
		const D3D12_DESCRIPTOR_RANGE kDefaultRange[] = {
			{ D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 16, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
			{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 48, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
			{ D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 16, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
			{ D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 16, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
		};
		D3D12_DESCRIPTOR_RANGE ranges[16];
		D3D12_ROOT_PARAMETER params[16];
		int rangeCnt = 0;
		auto SetParam = [&](D3D12_SHADER_VISIBILITY vis)
		{
			params[rangeCnt].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[rangeCnt].ShaderVisibility = vis;
			params[rangeCnt].DescriptorTable.pDescriptorRanges = ranges + rangeCnt;
			params[rangeCnt].DescriptorTable.NumDescriptorRanges = 1;
		};
		D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

		if (vs)
		{
			inputIndex_.vsCbvIndex_ = rangeCnt;
			SetParam(D3D12_SHADER_VISIBILITY_VERTEX);
			ranges[rangeCnt++] = kDefaultRange[0];
			inputIndex_.vsSrvIndex_ = rangeCnt;
			SetParam(D3D12_SHADER_VISIBILITY_VERTEX);
			ranges[rangeCnt++] = kDefaultRange[1];
			inputIndex_.vsSamplerIndex_ = rangeCnt;
			SetParam(D3D12_SHADER_VISIBILITY_VERTEX);
			ranges[rangeCnt++] = kDefaultRange[2];
			flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
		}
		if (ps)
		{
			inputIndex_.psCbvIndex_ = rangeCnt;
			SetParam(D3D12_SHADER_VISIBILITY_PIXEL);
			ranges[rangeCnt++] = kDefaultRange[0];
			inputIndex_.psSrvIndex_ = rangeCnt;
			SetParam(D3D12_SHADER_VISIBILITY_PIXEL);
			ranges[rangeCnt++] = kDefaultRange[1];
			inputIndex_.psSamplerIndex_ = rangeCnt;
			SetParam(D3D12_SHADER_VISIBILITY_PIXEL);
			ranges[rangeCnt++] = kDefaultRange[2];
			inputIndex_.psUavIndex_ = rangeCnt;
			SetParam(D3D12_SHADER_VISIBILITY_PIXEL);
			ranges[rangeCnt++] = kDefaultRange[3];
			flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
		}
		if (gs)
		{
			inputIndex_.gsCbvIndex_ = rangeCnt;
			SetParam(D3D12_SHADER_VISIBILITY_GEOMETRY);
			ranges[rangeCnt++] = kDefaultRange[0];
			inputIndex_.gsSrvIndex_ = rangeCnt;
			SetParam(D3D12_SHADER_VISIBILITY_GEOMETRY);
			ranges[rangeCnt++] = kDefaultRange[1];
			inputIndex_.gsSamplerIndex_ = rangeCnt;
			SetParam(D3D12_SHADER_VISIBILITY_GEOMETRY);
			ranges[rangeCnt++] = kDefaultRange[2];
			flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
		}
		if (hs)
		{
			inputIndex_.hsCbvIndex_ = rangeCnt;
			SetParam(D3D12_SHADER_VISIBILITY_HULL);
			ranges[rangeCnt++] = kDefaultRange[0];
			inputIndex_.hsSrvIndex_ = rangeCnt;
			SetParam(D3D12_SHADER_VISIBILITY_HULL);
			ranges[rangeCnt++] = kDefaultRange[1];
			inputIndex_.hsSamplerIndex_ = rangeCnt;
			SetParam(D3D12_SHADER_VISIBILITY_HULL);
			ranges[rangeCnt++] = kDefaultRange[2];
			flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
		}
		if (ds)
		{
			inputIndex_.dsCbvIndex_ = rangeCnt;
			SetParam(D3D12_SHADER_VISIBILITY_DOMAIN);
			ranges[rangeCnt++] = kDefaultRange[0];
			inputIndex_.dsSrvIndex_ = rangeCnt;
			SetParam(D3D12_SHADER_VISIBILITY_DOMAIN);
			ranges[rangeCnt++] = kDefaultRange[1];
			inputIndex_.dsSamplerIndex_ = rangeCnt;
			SetParam(D3D12_SHADER_VISIBILITY_DOMAIN);
			ranges[rangeCnt++] = kDefaultRange[2];
			flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
		}

		D3D12_ROOT_SIGNATURE_DESC desc{};
		desc.NumParameters = rangeCnt;
		desc.pParameters = params;
		desc.NumStaticSamplers = 0;
		desc.pStaticSamplers = nullptr;
		desc.Flags = flags;

		return Initialize(pDev, desc);
	}

	//----
	bool RootSignature::Initialize(Device* pDev, Shader* cs)
	{
		/*
		D3D12_DESCRIPTOR_RANGE_TYPE RangeType;
		UINT NumDescriptors;
		UINT BaseShaderRegister;
		UINT RegisterSpace;
		UINT OffsetInDescriptorsFromTableStart;
		*/
		const D3D12_DESCRIPTOR_RANGE kDefaultRange[] = {
			{ D3D12_DESCRIPTOR_RANGE_TYPE_CBV,     16, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
			{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV,     48, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
			{ D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 16, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
			{ D3D12_DESCRIPTOR_RANGE_TYPE_UAV,     16, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
		};
		D3D12_DESCRIPTOR_RANGE ranges[4];
		D3D12_ROOT_PARAMETER params[4];
		int rangeCnt = 0;
		auto SetParam = [&](D3D12_SHADER_VISIBILITY vis)
		{
			params[rangeCnt].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[rangeCnt].ShaderVisibility = vis;
			params[rangeCnt].DescriptorTable.pDescriptorRanges = ranges + rangeCnt;
			params[rangeCnt].DescriptorTable.NumDescriptorRanges = 1;
		};
		inputIndex_.csCbvIndex_ = rangeCnt;
		SetParam(D3D12_SHADER_VISIBILITY_ALL);
		ranges[rangeCnt++] = kDefaultRange[0];
		inputIndex_.csSrvIndex_ = rangeCnt;
		SetParam(D3D12_SHADER_VISIBILITY_ALL);
		ranges[rangeCnt++] = kDefaultRange[1];
		inputIndex_.csSamplerIndex_ = rangeCnt;
		SetParam(D3D12_SHADER_VISIBILITY_ALL);
		ranges[rangeCnt++] = kDefaultRange[2];
		inputIndex_.csUavIndex_ = rangeCnt;
		SetParam(D3D12_SHADER_VISIBILITY_ALL);
		ranges[rangeCnt++] = kDefaultRange[3];

		D3D12_ROOT_SIGNATURE_DESC desc{};
		desc.NumParameters = rangeCnt;
		desc.pParameters = params;
		desc.NumStaticSamplers = 0;
		desc.pStaticSamplers = nullptr;
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		return Initialize(pDev, desc);
	}

	//----
	void RootSignature::Destroy()
	{
		SafeRelease(pRootSignature_);
	}


	//----
	bool CreateRaytracingRootSignature(
		sl12::Device* pDevice,
		sl12::u32 asCount,
		sl12::u32 globalCbvCount,
		sl12::u32 globalSrvCount,
		sl12::u32 globalUavCount,
		sl12::u32 globalSamplerCount,
		sl12::RootSignature* pGlobalRS,
		sl12::RootSignature* pLocalRS)
	{
		{
			sl12::u32 range_cnt = 0;
			D3D12_DESCRIPTOR_RANGE ranges[4]{};
			D3D12_ROOT_PARAMETER params[16]{};
			if (globalCbvCount > 0)
			{
				auto&& p = params[range_cnt];
				p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				p.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				p.DescriptorTable.NumDescriptorRanges = 1;
				p.DescriptorTable.pDescriptorRanges = &ranges[range_cnt];

				auto&& r = ranges[range_cnt++];
				r.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				r.NumDescriptors = globalCbvCount;
				r.BaseShaderRegister = 0;
				r.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
			}
			if (globalSrvCount > 0)
			{
				auto&& p = params[range_cnt];
				p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				p.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				p.DescriptorTable.NumDescriptorRanges = 1;
				p.DescriptorTable.pDescriptorRanges = &ranges[range_cnt];

				auto&& r = ranges[range_cnt++];
				r.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				r.NumDescriptors = globalSrvCount;
				r.BaseShaderRegister = asCount;
				r.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
			}
			if (globalUavCount > 0)
			{
				auto&& p = params[range_cnt];
				p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				p.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				p.DescriptorTable.NumDescriptorRanges = 1;
				p.DescriptorTable.pDescriptorRanges = &ranges[range_cnt];

				auto&& r = ranges[range_cnt++];
				r.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
				r.NumDescriptors = globalUavCount;
				r.BaseShaderRegister = 0;
				r.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
			}
			if (globalSamplerCount > 0)
			{
				auto&& p = params[range_cnt];
				p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				p.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				p.DescriptorTable.NumDescriptorRanges = 1;
				p.DescriptorTable.pDescriptorRanges = &ranges[range_cnt];

				auto&& r = ranges[range_cnt++];
				r.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
				r.NumDescriptors = globalSamplerCount;
				r.BaseShaderRegister = 0;
				r.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
			}

			for (sl12::u32 i = 0; i < asCount; i++)
			{
				auto&& p = params[range_cnt + i];
				p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
				p.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				p.Descriptor.ShaderRegister = i;
				p.Descriptor.RegisterSpace = 0;
			}

			D3D12_ROOT_SIGNATURE_DESC sigDesc{};
			sigDesc.NumParameters = range_cnt + asCount;
			sigDesc.pParameters = params;
			sigDesc.NumStaticSamplers = 0;
			sigDesc.pStaticSamplers = nullptr;
			sigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

			if (!pGlobalRS->Initialize(pDevice, sigDesc))
			{
				return false;
			}
		}
		{
			D3D12_DESCRIPTOR_RANGE ranges[] = {
				{ D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
				{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
				{ D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
				{ D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
			};
			ranges[0].NumDescriptors = sl12::kCbvMax - globalCbvCount;
			ranges[1].NumDescriptors = sl12::kSrvMax - globalSrvCount - asCount;
			ranges[2].NumDescriptors = sl12::kUavMax - globalUavCount;
			ranges[3].NumDescriptors = sl12::kSamplerMax - globalSamplerCount;
			ranges[0].BaseShaderRegister = globalCbvCount;
			ranges[1].BaseShaderRegister = globalSrvCount + asCount;
			ranges[2].BaseShaderRegister = globalUavCount;
			ranges[3].BaseShaderRegister = globalSamplerCount;

			D3D12_ROOT_PARAMETER params[ARRAYSIZE(ranges)]{};
			for (int i = 0; i < ARRAYSIZE(ranges); i++)
			{
				params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				params[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				params[i].DescriptorTable.NumDescriptorRanges = 1;
				params[i].DescriptorTable.pDescriptorRanges = &ranges[i];
			}

			D3D12_ROOT_SIGNATURE_DESC sigDesc{};
			sigDesc.NumParameters = ARRAYSIZE(params);
			sigDesc.pParameters = params;
			sigDesc.NumStaticSamplers = 0;
			sigDesc.pStaticSamplers = nullptr;
			sigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

			if (!pLocalRS->Initialize(pDevice, sigDesc))
			{
				return false;
			}
		}

		return true;
	}

}	// namespace sl12

//	EOF
