#include <sl12/root_signature.h>

#include <sl12/device.h>


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
		rd.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

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
	void RootSignature::Destroy()
	{
		SafeRelease(pRootSignature_);
	}

}	// namespace sl12

//	EOF
