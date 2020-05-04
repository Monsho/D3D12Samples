#include "rtxgi_component.h"

#include "../shader/rtxgi/volume_desc.h"

// shader include
#include "CompiledShaders/irradiance_blending.c.hlsl.h"
#include "CompiledShaders/distance_blending.c.hlsl.h"
#include "CompiledShaders/border_row_update.c.hlsl.h"
#include "CompiledShaders/border_column_update.c.hlsl.h"
#include "CompiledShaders/probe_relocation.c.hlsl.h"
#include "CompiledShaders/state_classifier.c.hlsl.h"
#include "CompiledShaders/state_activate_all.c.hlsl.h"


//----
bool RtxgiComponent::Initialize()
{
	if (!pParentDevice_)
		return false;

	// set volume descriptor.
	ddgiVolumeDesc_.viewBias = SGI_VIEW_BIAS;
	ddgiVolumeDesc_.normalBias = SGI_NORMAL_BIAS;
	ddgiVolumeDesc_.probeChangeThreshold = SGI_CHANGE_THRESHOLD;
	ddgiVolumeDesc_.probeBrightnessThreshold = SGI_BRIGHTNESS_THRESHOLD;
	ddgiVolumeDesc_.numRaysPerProbe = SGI_NUM_RAYS_PER_PROBE;
	ddgiVolumeDesc_.numIrradianceTexels = SGI_NUM_IRRADIANCE_TEXELS;
	ddgiVolumeDesc_.numDistanceTexels = SGI_NUM_DISTANCE_TEXELS;
	ddgiVolumeDesc_.probeGridCounts.x = SGI_GRID_COUNT_X;
	ddgiVolumeDesc_.probeGridCounts.y = SGI_GRID_COUNT_Y;
	ddgiVolumeDesc_.probeGridCounts.z = SGI_GRID_COUNT_Z;
	ddgiVolumeDesc_.probeGridSpacing.x = SGI_GRID_SPACING_X;
	ddgiVolumeDesc_.probeGridSpacing.y = SGI_GRID_SPACING_Y;
	ddgiVolumeDesc_.probeGridSpacing.z = SGI_GRID_SPACING_Z;

	// initialize shaders.
	if (!InitializeShaders())
		return false;

	// new volume instance.
	ddgiVolume_.reset(new rtxgi::DDGIVolume("DDGI Volume"));

	// descriptor heap.
	auto p_device_dep = pParentDevice_->GetDeviceDep();
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 32;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		desc.NodeMask = 0x01;
		auto hr = p_device_dep->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pDescriptorHeap_));
		if (FAILED(hr))
			return false;
	}
	ddgiVolumeResource_.descriptorHeap = pDescriptorHeap_;
	ddgiVolumeResource_.descriptorHeapDescSize = p_device_dep->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	ddgiVolumeResource_.descriptorHeapOffset = 0;

	// constant buffer.
	{
		auto size = rtxgi::GetDDGIVolumeConstantBufferSize();
		for (int i = 0; i < 2; i++)
		{
			if (!volumeCBs_[i].Initialize(pParentDevice_, size, 0, sl12::BufferUsage::ConstantBuffer, true, false))
			{
				return false;
			}
			if (!volumeCBVs_[i].Initialize(pParentDevice_, &volumeCBs_[0]))
			{
				return false;
			}
		}
	}

	// create texture resources.
	if (!CreateTextures())
		return false;

	// create root signature and psos.
	if (!CreatePipelines())
		return false;

	// create ddgi volume.
	auto status = ddgiVolume_->Create(ddgiVolumeDesc_, ddgiVolumeResource_);
	if (status != rtxgi::ERTXGIStatus::OK)
	{
		return false;
	}

	return true;
}

//----
bool RtxgiComponent::InitializeShaders()
{
#define ShaderInit(t, b)																	\
	if (!shaders_[t].Initialize(pParentDevice_, sl12::ShaderType::Compute, b, sizeof(b)))	\
	{																						\
		return false;																		\
	}

	ShaderInit(EShaderType::IrradianceBlending, g_pIrradianceBlendingCS);
	ShaderInit(EShaderType::DistanceBlending, g_pDistanceBlendingCS);
	ShaderInit(EShaderType::BorderRowUpdate, g_pBorderRowUpdateCS);
	ShaderInit(EShaderType::BorderColumnUpdate, g_pBorderColumnUpdateCS);
	ShaderInit(EShaderType::ProbeRelocation, g_pProbeRelocationCS);
	ShaderInit(EShaderType::StateClassifier, g_pStateClassifierCS);
	ShaderInit(EShaderType::StateActivateAll, g_pStateActivateAllCS);

#undef ShaderInit

	return true;
}

//----
bool RtxgiComponent::CreateTextures()
{
	sl12::TextureDesc desc{};
	desc.dimension = sl12::TextureDimension::Texture2D;
	desc.depth = 1;
	desc.mipLevels = 1;
	desc.sampleCount = 1;
	desc.isUav = true;

	D3D12_CPU_DESCRIPTOR_HANDLE handle = ddgiVolumeResource_.descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += (ddgiVolumeResource_.descriptorHeapDescSize * ddgiVolumeResource_.descriptorHeapOffset);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	auto p_device_dep = pParentDevice_->GetDeviceDep();
	{
		rtxgi::GetDDGIVolumeTextureDimensions(ddgiVolumeDesc_, rtxgi::EDDGITextureType::RTRadiance, desc.width, desc.height);
		desc.format = uavDesc.Format = rtxgi::GetDDGIVolumeTextureFormat(rtxgi::EDDGITextureType::RTRadiance);
		desc.initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		if (!textures_[ETextureType::Radiance].Initialize(pParentDevice_, desc))
			return false;
		ddgiVolumeResource_.probeRTRadiance = textures_[ETextureType::Radiance].GetResourceDep();

		p_device_dep->CreateUnorderedAccessView(ddgiVolumeResource_.probeRTRadiance, nullptr, &uavDesc, handle);
		handle.ptr += ddgiVolumeResource_.descriptorHeapDescSize;

		// uav for sl12
		if (!textureUavs_[ETextureType::Radiance].Initialize(pParentDevice_, &textures_[ETextureType::Radiance]))
			return false;
	}
	{
		rtxgi::GetDDGIVolumeTextureDimensions(ddgiVolumeDesc_, rtxgi::EDDGITextureType::Irradiance, desc.width, desc.height);
		desc.format = uavDesc.Format = rtxgi::GetDDGIVolumeTextureFormat(rtxgi::EDDGITextureType::Irradiance);
		desc.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		if (!textures_[ETextureType::Irradiance].Initialize(pParentDevice_, desc))
			return false;
		ddgiVolumeResource_.probeIrradiance = textures_[ETextureType::Irradiance].GetResourceDep();

		p_device_dep->CreateUnorderedAccessView(ddgiVolumeResource_.probeIrradiance, nullptr, &uavDesc, handle);
		handle.ptr += ddgiVolumeResource_.descriptorHeapDescSize;

		// srv for sl12
		if (!textureSrvs_[ETextureType::Irradiance].Initialize(pParentDevice_, &textures_[ETextureType::Irradiance]))
			return false;
	}
	{
		rtxgi::GetDDGIVolumeTextureDimensions(ddgiVolumeDesc_, rtxgi::EDDGITextureType::Distance, desc.width, desc.height);
		desc.format = uavDesc.Format = rtxgi::GetDDGIVolumeTextureFormat(rtxgi::EDDGITextureType::Distance);
		desc.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		if (!textures_[ETextureType::Distance].Initialize(pParentDevice_, desc))
			return false;
		ddgiVolumeResource_.probeDistance = textures_[ETextureType::Distance].GetResourceDep();

		p_device_dep->CreateUnorderedAccessView(ddgiVolumeResource_.probeDistance, nullptr, &uavDesc, handle);
		handle.ptr += ddgiVolumeResource_.descriptorHeapDescSize;

		// srv for sl12
		if (!textureSrvs_[ETextureType::Distance].Initialize(pParentDevice_, &textures_[ETextureType::Distance]))
			return false;
	}
	{
		rtxgi::GetDDGIVolumeTextureDimensions(ddgiVolumeDesc_, rtxgi::EDDGITextureType::Offsets, desc.width, desc.height);
		if (desc.width <= 0)
			return false;
		desc.format = uavDesc.Format = rtxgi::GetDDGIVolumeTextureFormat(rtxgi::EDDGITextureType::Offsets);
		desc.initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		if (!textures_[ETextureType::Offset].Initialize(pParentDevice_, desc))
			return false;
		ddgiVolumeResource_.probeOffsets = textures_[ETextureType::Offset].GetResourceDep();

		p_device_dep->CreateUnorderedAccessView(ddgiVolumeResource_.probeOffsets, nullptr, &uavDesc, handle);
		handle.ptr += ddgiVolumeResource_.descriptorHeapDescSize;

		// uav for sl12
		if (!textureUavs_[ETextureType::Offset].Initialize(pParentDevice_, &textures_[ETextureType::Offset]))
			return false;
	}
	{
		rtxgi::GetDDGIVolumeTextureDimensions(ddgiVolumeDesc_, rtxgi::EDDGITextureType::States, desc.width, desc.height);
		if (desc.width <= 0)
			return false;
		desc.format = uavDesc.Format = rtxgi::GetDDGIVolumeTextureFormat(rtxgi::EDDGITextureType::States);
		desc.initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		if (!textures_[ETextureType::State].Initialize(pParentDevice_, desc))
			return false;
		ddgiVolumeResource_.probeStates = textures_[ETextureType::State].GetResourceDep();

		p_device_dep->CreateUnorderedAccessView(ddgiVolumeResource_.probeStates, nullptr, &uavDesc, handle);
		handle.ptr += ddgiVolumeResource_.descriptorHeapDescSize;

		// uav for sl12
		if (!textureUavs_[ETextureType::State].Initialize(pParentDevice_, &textures_[ETextureType::State]))
			return false;
	}

	return true;
}

//----
bool RtxgiComponent::CreatePipelines()
{
	auto p_device_dep = pParentDevice_->GetDeviceDep();

	// create root signature.
	{
		ID3DBlob* signature;
		if (!rtxgi::GetDDGIVolumeRootSignatureDesc(ddgiVolumeResource_.descriptorHeapOffset, &signature))
			return false;
		if (signature == nullptr)
			return false;

		HRESULT hr = p_device_dep->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&pRootSignature_));
		sl12::SafeRelease(signature);
		if (FAILED(hr))
			return false;

		ddgiVolumeResource_.rootSignature = pRootSignature_;
	}

	// create psos.
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = pRootSignature_;

		psoDesc.CS.BytecodeLength = shaders_[EShaderType::IrradianceBlending].GetSize();
		psoDesc.CS.pShaderBytecode = shaders_[EShaderType::IrradianceBlending].GetData();
		auto hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::IrradianceBlending]));
		if (FAILED(hr))
			return false;
		ddgiVolumeResource_.probeRadianceBlendingPSO = psos_[EShaderType::IrradianceBlending];

		psoDesc.CS.BytecodeLength = shaders_[EShaderType::DistanceBlending].GetSize();
		psoDesc.CS.pShaderBytecode = shaders_[EShaderType::DistanceBlending].GetData();
		hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::DistanceBlending]));
		if (FAILED(hr))
			return false;
		ddgiVolumeResource_.probeDistanceBlendingPSO = psos_[EShaderType::DistanceBlending];

		psoDesc.CS.BytecodeLength = shaders_[EShaderType::BorderRowUpdate].GetSize();
		psoDesc.CS.pShaderBytecode = shaders_[EShaderType::BorderRowUpdate].GetData();
		hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::BorderRowUpdate]));
		if (FAILED(hr))
			return false;
		ddgiVolumeResource_.probeBorderRowPSO = psos_[EShaderType::BorderRowUpdate];

		psoDesc.CS.BytecodeLength = shaders_[EShaderType::BorderColumnUpdate].GetSize();
		psoDesc.CS.pShaderBytecode = shaders_[EShaderType::BorderColumnUpdate].GetData();
		hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::BorderColumnUpdate]));
		if (FAILED(hr))
			return false;
		ddgiVolumeResource_.probeBorderColumnPSO = psos_[EShaderType::BorderColumnUpdate];

		psoDesc.CS.BytecodeLength = shaders_[EShaderType::ProbeRelocation].GetSize();
		psoDesc.CS.pShaderBytecode = shaders_[EShaderType::ProbeRelocation].GetData();
		hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::ProbeRelocation]));
		if (FAILED(hr))
			return false;
		ddgiVolumeResource_.probeRelocationPSO = psos_[EShaderType::ProbeRelocation];

		psoDesc.CS.BytecodeLength = shaders_[EShaderType::StateClassifier].GetSize();
		psoDesc.CS.pShaderBytecode = shaders_[EShaderType::StateClassifier].GetData();
		hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::StateClassifier]));
		if (FAILED(hr))
			return false;
		ddgiVolumeResource_.probeStateClassifierPSO = psos_[EShaderType::StateClassifier];

		psoDesc.CS.BytecodeLength = shaders_[EShaderType::StateActivateAll].GetSize();
		psoDesc.CS.pShaderBytecode = shaders_[EShaderType::StateActivateAll].GetData();
		hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::StateActivateAll]));
		if (FAILED(hr))
			return false;
		ddgiVolumeResource_.probeStateClassifierActivateAllPSO = psos_[EShaderType::StateActivateAll];
	}

	return true;
}

//----
void RtxgiComponent::Destroy()
{
	for (auto&& v : shaders_) v.Destroy();
	sl12::SafeRelease(pDescriptorHeap_);
	for (auto&& v : volumeCBVs_) v.Destroy();
	for (auto&& v : volumeCBs_) v.Destroy();
	for (auto&& v : textureUavs_) v.Destroy();
	for (auto&& v : textureSrvs_) v.Destroy();
	for (auto&& v : textures_) v.Destroy();
	sl12::SafeRelease(pRootSignature_);
	for (auto&& v : psos_) sl12::SafeRelease(v);

	ddgiVolume_.reset(nullptr);
}

//----
void RtxgiComponent::UpdateVolume(const DirectX::XMFLOAT3* pTranslate)
{
	flipIndex_ = 1 - flipIndex_;
	if (pTranslate)
	{
		rtxgi::float3 v = { pTranslate->x, pTranslate->y, pTranslate->z };
		ddgiVolume_->Move(v);
	}

	ddgiVolume_->Update(volumeCBs_[flipIndex_].GetResourceDep(), 0);
}

//----
void RtxgiComponent::UpdateProbes(sl12::CommandList* pCmdList)
{
	ddgiVolume_->UpdateProbes(pCmdList->GetDxrCommandList());
}

//----
void RtxgiComponent::RelocateProbes(sl12::CommandList* pCmdList, float distanceScale)
{
	ddgiVolume_->RelocateProbes(pCmdList->GetDxrCommandList(), distanceScale);
}

//----
void RtxgiComponent::ClassifyProbes(sl12::CommandList* pCmdList)
{
	ddgiVolume_->ClassifyProbes(pCmdList->GetDxrCommandList());
}


//	EOF
