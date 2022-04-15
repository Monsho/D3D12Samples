#include "rtxgi_component.h"

#include <rtxgi/ddgi/DDGIVolume.h>

#include "../shader/rtxgi/volume_desc.h"

// shader include
#include "CompiledShaders/rtxgi_irradiance_blending.c.hlsl.h"
#include "CompiledShaders/rtxgi_distance_blending.c.hlsl.h"
#include "CompiledShaders/rtxgi_border_row_update_irradiance.c.hlsl.h"
#include "CompiledShaders/rtxgi_border_row_update_distance.c.hlsl.h"
#include "CompiledShaders/rtxgi_border_clm_update_irradiance.c.hlsl.h"
#include "CompiledShaders/rtxgi_border_clm_update_distance.c.hlsl.h"
#include "CompiledShaders/rtxgi_probe_relocation.c.hlsl.h"
#include "CompiledShaders/rtxgi_probe_relocation_reset.c.hlsl.h"
#include "CompiledShaders/rtxgi_probe_classification.c.hlsl.h"
#include "CompiledShaders/rtxgi_probe_classification_reset.c.hlsl.h"


//----
bool RtxgiComponent::Initialize()
{
	if (!pParentDevice_)
		return false;

	// set volume descriptor.
	ddgiVolumeDesc_.name = "Sample Volume";
	ddgiVolumeDesc_.index = 0;
	ddgiVolumeDesc_.rngSeed = 0;
	ddgiVolumeDesc_.origin = { 0.0f, 0.0f, 0.0f };
	ddgiVolumeDesc_.eulerAngles = { 0.0f, 0.0f, 0.0f };
	ddgiVolumeDesc_.probeSpacing = { SGI_GRID_SPACING_X, SGI_GRID_SPACING_Y, SGI_GRID_SPACING_Z };
	ddgiVolumeDesc_.probeCounts = { SGI_GRID_COUNT_X, SGI_GRID_COUNT_Y, SGI_GRID_COUNT_Z };
	ddgiVolumeDesc_.probeNumRays = SGI_NUM_RAYS_PER_PROBE;
	ddgiVolumeDesc_.probeNumIrradianceTexels = SGI_NUM_IRRADIANCE_TEXELS;
	ddgiVolumeDesc_.probeNumDistanceTexels = SGI_NUM_DISTANCE_TEXELS;
	ddgiVolumeDesc_.probeHysteresis = SGI_HYSTERESIS;
	ddgiVolumeDesc_.probeNormalBias = SGI_NORMAL_BIAS;
	ddgiVolumeDesc_.probeViewBias = SGI_VIEW_BIAS;
	ddgiVolumeDesc_.probeMaxRayDistance = SGI_MAX_RAY_DISTANCE;
	ddgiVolumeDesc_.probeIrradianceThreshold = SGI_IRRADIANCE_THRESHOLD;
	ddgiVolumeDesc_.probeBrightnessThreshold = SGI_BRIGHTNESS_THRESHOLD;

	ddgiVolumeDesc_.showProbes = true;

	ddgiVolumeDesc_.probeRayDataFormat = 1;
	ddgiVolumeDesc_.probeIrradianceFormat = 1;
	ddgiVolumeDesc_.probeDistanceFormat = 1;
	ddgiVolumeDesc_.probeDataFormat = 1;

	ddgiVolumeDesc_.probeRelocationEnabled = true;
	ddgiVolumeDesc_.probeMinFrontfaceDistance = 0.1f;
	ddgiVolumeDesc_.probeClassificationEnabled = true;

	ddgiVolumeDesc_.movementType = rtxgi::EDDGIVolumeMovementType::Default;

	// initialize shaders.
	if (!InitializeShaders())
		return false;

	// new volume instance.
	ddgiVolume_.reset(new rtxgi::d3d12::DDGIVolume());

	// TEST: only one DDGIVolume
	static const sl12::u32 kMaxVolumes = 1;

	// descriptor heap.
	auto p_device_dep = pParentDevice_->GetDeviceDep();
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 
			1			// constants structured buffer count
			+ rtxgi::GetDDGIVolumeNumUAVDescriptors() * kMaxVolumes
			+ rtxgi::GetDDGIVolumeNumSRVDescriptors() * kMaxVolumes;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		desc.NodeMask = 0x01;
		auto hr = p_device_dep->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pSrvDescriptorHeap_));
		if (FAILED(hr))
			return false;

		desc.NumDescriptors = rtxgi::GetDDGIVolumeNumRTVDescriptors() * kMaxVolumes;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		// Create the RTV heap
		hr = p_device_dep->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pRtvDescriptorHeap_));
		if (FAILED(hr))
			return false;
	}
	ddgiVolumeResource_.descriptorHeapDesc.heap = pSrvDescriptorHeap_;
	ddgiVolumeResource_.descriptorHeapDesc.constsOffset = 0;														// constants structured buffer start index.
	ddgiVolumeResource_.descriptorHeapDesc.uavOffset = 1;															// uav start index.
	ddgiVolumeResource_.descriptorHeapDesc.srvOffset = 1 + rtxgi::GetDDGIVolumeNumUAVDescriptors() * kMaxVolumes;	// srv start index.

	// unmanaged mode + no bindless.
	ddgiVolumeResource_.managed.enabled = false;
	ddgiVolumeResource_.unmanaged.enabled = true;
	ddgiVolumeResource_.unmanaged.rootParamSlotRootConstants = 0;
	ddgiVolumeResource_.unmanaged.rootParamSlotDescriptorTable = 1;

	// constant buffer.
	{
		sl12::u32 stride = sizeof(rtxgi::DDGIVolumeDescGPUPacked);
		sl12::u32 size = stride * kMaxVolumes;
		if (!constantSTB_.Initialize(pParentDevice_, size, stride, sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_COPY_DEST, false, false))
		{
			return false;
		}
		if (!constantSTBUpload_.Initialize(pParentDevice_, size * 2, stride, sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
		{
			return false;
		}
		ddgiVolumeResource_.constantsBuffer = constantSTB_.GetResourceDep();
		ddgiVolumeResource_.constantsBufferUpload = constantSTBUpload_.GetResourceDep();
		ddgiVolumeResource_.constantsBufferSizeInBytes = size;

		if (!constantSTBView_.Initialize(pParentDevice_, &constantSTB_, 0, kMaxVolumes, stride))
		{
			return false;
		}

		// create SRV for RTXGI.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.NumElements = kMaxVolumes;
		srvDesc.Buffer.StructureByteStride = stride;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		auto p_device_dep = pParentDevice_->GetDeviceDep();
		UINT srvDescSize = p_device_dep->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		D3D12_CPU_DESCRIPTOR_HANDLE handle;
		handle = ddgiVolumeResource_.descriptorHeapDesc.heap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += (ddgiVolumeResource_.descriptorHeapDesc.constsOffset * srvDescSize);
		p_device_dep->CreateShaderResourceView(constantSTB_.GetResourceDep(), &srvDesc, handle);
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

	ShaderInit(EShaderType::IrradianceBlending, g_pRTXGIIrradianceBlendingCS);
	ShaderInit(EShaderType::DistanceBlending, g_pRTXGIDistanceBlendingCS);
	ShaderInit(EShaderType::BorderRowUpdateIrradiance, g_pRTXGIBorderRowUpdateIrradianceCS);
	ShaderInit(EShaderType::BorderClmUpdateIrradiance, g_pRTXGIBorderClmUpdateIrradianceCS);
	ShaderInit(EShaderType::BorderRowUpdateDistance, g_pRTXGIBorderRowUpdateDistanceCS);
	ShaderInit(EShaderType::BorderClmUpdateDistance, g_pRTXGIBorderClmUpdateDistanceCS);
	ShaderInit(EShaderType::ProbeRelocation, g_pRTXGIProbeRelocationCS);
	ShaderInit(EShaderType::ProbeRelocationReset, g_pRTXGIProbeRelocationResetCS);
	ShaderInit(EShaderType::ProbeClassification, g_pRTXGIProbeClassificationCS);
	ShaderInit(EShaderType::ProbeClassificationReset, g_pRTXGIProbeClassificationResetCS);

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

	// create texture resources.
	rtxgi::GetDDGIVolumeTextureDimensions(ddgiVolumeDesc_, rtxgi::EDDGIVolumeTextureType::RayData, desc.width, desc.height);
	desc.format = rtxgi::d3d12::GetDDGIVolumeTextureFormat(rtxgi::EDDGIVolumeTextureType::RayData, ddgiVolumeDesc_.probeRayDataFormat);
	desc.initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	desc.isRenderTarget = false;
	if (!textures_[ETextureType::RayData].Initialize(pParentDevice_, desc))
	{
		return false;
	}
	ddgiVolumeResource_.unmanaged.probeRayData = textures_[ETextureType::RayData].GetResourceDep();

	rtxgi::GetDDGIVolumeTextureDimensions(ddgiVolumeDesc_, rtxgi::EDDGIVolumeTextureType::Irradiance, desc.width, desc.height);
	desc.format = rtxgi::d3d12::GetDDGIVolumeTextureFormat(rtxgi::EDDGIVolumeTextureType::Irradiance, ddgiVolumeDesc_.probeIrradianceFormat);
	desc.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	desc.isRenderTarget = true;
	if (!textures_[ETextureType::Irradiance].Initialize(pParentDevice_, desc))
	{
		return false;
	}
	ddgiVolumeResource_.unmanaged.probeIrradiance = textures_[ETextureType::Irradiance].GetResourceDep();

	rtxgi::GetDDGIVolumeTextureDimensions(ddgiVolumeDesc_, rtxgi::EDDGIVolumeTextureType::Distance, desc.width, desc.height);
	desc.format = rtxgi::d3d12::GetDDGIVolumeTextureFormat(rtxgi::EDDGIVolumeTextureType::Distance, ddgiVolumeDesc_.probeDistanceFormat);
	desc.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	desc.isRenderTarget = true;
	if (!textures_[ETextureType::Distance].Initialize(pParentDevice_, desc))
	{
		return false;
	}
	ddgiVolumeResource_.unmanaged.probeDistance = textures_[ETextureType::Distance].GetResourceDep();

	rtxgi::GetDDGIVolumeTextureDimensions(ddgiVolumeDesc_, rtxgi::EDDGIVolumeTextureType::Data, desc.width, desc.height);
	desc.format = rtxgi::d3d12::GetDDGIVolumeTextureFormat(rtxgi::EDDGIVolumeTextureType::Data, ddgiVolumeDesc_.probeDataFormat);
	desc.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	desc.isRenderTarget = false;
	if (!textures_[ETextureType::ProbeData].Initialize(pParentDevice_, desc))
	{
		return false;
	}
	ddgiVolumeResource_.unmanaged.probeData = textures_[ETextureType::ProbeData].GetResourceDep();

	// store descriptor heap.
	{
		auto p_device_dep = pParentDevice_->GetDeviceDep();

		D3D12_CPU_DESCRIPTOR_HANDLE srvHandle, uavHandle, rtvHandle;
		srvHandle = uavHandle = ddgiVolumeResource_.descriptorHeapDesc.heap->GetCPUDescriptorHandleForHeapStart();
		rtvHandle = pRtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

		UINT srvDescSize = p_device_dep->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		UINT rtvDescSize = p_device_dep->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		uavHandle.ptr += (ddgiVolumeResource_.descriptorHeapDesc.uavOffset * srvDescSize);
		srvHandle.ptr += (ddgiVolumeResource_.descriptorHeapDesc.srvOffset * srvDescSize);

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

		// ray data.
		{
			srvDesc.Format = uavDesc.Format = rtxgi::d3d12::GetDDGIVolumeTextureFormat(rtxgi::EDDGIVolumeTextureType::RayData, ddgiVolumeDesc_.probeRayDataFormat);
			p_device_dep->CreateUnorderedAccessView(ddgiVolumeResource_.unmanaged.probeRayData, nullptr, &uavDesc, uavHandle);
			p_device_dep->CreateShaderResourceView(ddgiVolumeResource_.unmanaged.probeRayData, &srvDesc, srvHandle);
		}

		uavHandle.ptr += srvDescSize;
		srvHandle.ptr += srvDescSize;

		// irradiance.
		{
			srvDesc.Format = uavDesc.Format = rtxgi::d3d12::GetDDGIVolumeTextureFormat(rtxgi::EDDGIVolumeTextureType::Irradiance, ddgiVolumeDesc_.probeIrradianceFormat);
			p_device_dep->CreateUnorderedAccessView(ddgiVolumeResource_.unmanaged.probeIrradiance, nullptr, &uavDesc, uavHandle);
			p_device_dep->CreateShaderResourceView(ddgiVolumeResource_.unmanaged.probeIrradiance, &srvDesc, srvHandle);

			ddgiVolumeResource_.unmanaged.probeIrradianceRTV.ptr = rtvHandle.ptr + (ddgiVolumeDesc_.index * rtxgi::GetDDGIVolumeNumRTVDescriptors() * rtvDescSize);
			p_device_dep->CreateRenderTargetView(ddgiVolumeResource_.unmanaged.probeIrradiance, &rtvDesc, ddgiVolumeResource_.unmanaged.probeIrradianceRTV);
		}

		uavHandle.ptr += srvDescSize;
		srvHandle.ptr += srvDescSize;
		rtvHandle.ptr += rtvDescSize;

		// distance.
		{
			srvDesc.Format = uavDesc.Format = rtxgi::d3d12::GetDDGIVolumeTextureFormat(rtxgi::EDDGIVolumeTextureType::Distance, ddgiVolumeDesc_.probeDistanceFormat);
			p_device_dep->CreateUnorderedAccessView(ddgiVolumeResource_.unmanaged.probeDistance, nullptr, &uavDesc, uavHandle);
			p_device_dep->CreateShaderResourceView(ddgiVolumeResource_.unmanaged.probeDistance, &srvDesc, srvHandle);

			ddgiVolumeResource_.unmanaged.probeDistanceRTV.ptr = ddgiVolumeResource_.unmanaged.probeIrradianceRTV.ptr + rtvDescSize;
			p_device_dep->CreateRenderTargetView(ddgiVolumeResource_.unmanaged.probeDistance, &rtvDesc, ddgiVolumeResource_.unmanaged.probeDistanceRTV);
		}

		uavHandle.ptr += srvDescSize;
		srvHandle.ptr += srvDescSize;

		// Probe data texture descriptors
		{
			srvDesc.Format = uavDesc.Format = rtxgi::d3d12::GetDDGIVolumeTextureFormat(rtxgi::EDDGIVolumeTextureType::Data, ddgiVolumeDesc_.probeDataFormat);
			p_device_dep->CreateUnorderedAccessView(ddgiVolumeResource_.unmanaged.probeData, nullptr, &uavDesc, uavHandle);
			p_device_dep->CreateShaderResourceView(ddgiVolumeResource_.unmanaged.probeData, &srvDesc, srvHandle);
		}
	}

	// create views for sl12
	for (int i = 0; i < ETextureType::Max; i++)
	{
		if (!textureSrvs_[i].Initialize(pParentDevice_, &textures_[i]))
		{
			return false;
		}
		if (!textureUavs_[i].Initialize(pParentDevice_, &textures_[i]))
		{
			return false;
		}
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
		if (!rtxgi::d3d12::GetDDGIVolumeRootSignatureDesc(ddgiVolumeResource_.descriptorHeapDesc.constsOffset, ddgiVolumeResource_.descriptorHeapDesc.uavOffset, signature))
			return false;
		if (signature == nullptr)
			return false;

		HRESULT hr = p_device_dep->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&pRootSignature_));
		sl12::SafeRelease(signature);
		if (FAILED(hr))
			return false;

		ddgiVolumeResource_.unmanaged.rootSignature = pRootSignature_;
	}

	// create psos.
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = ddgiVolumeResource_.unmanaged.rootSignature;

		{
			psoDesc.CS.BytecodeLength = shaders_[EShaderType::IrradianceBlending].GetSize();
			psoDesc.CS.pShaderBytecode = shaders_[EShaderType::IrradianceBlending].GetData();
			auto hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::IrradianceBlending]));
			if (FAILED(hr))
				return false;
			ddgiVolumeResource_.unmanaged.probeBlendingIrradiancePSO = psos_[EShaderType::IrradianceBlending];
		}

		{
			psoDesc.CS.BytecodeLength = shaders_[EShaderType::DistanceBlending].GetSize();
			psoDesc.CS.pShaderBytecode = shaders_[EShaderType::DistanceBlending].GetData();
			auto hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::DistanceBlending]));
			if (FAILED(hr))
				return false;
			ddgiVolumeResource_.unmanaged.probeBlendingDistancePSO = psos_[EShaderType::DistanceBlending];
		}

		{
			psoDesc.CS.BytecodeLength = shaders_[EShaderType::BorderRowUpdateIrradiance].GetSize();
			psoDesc.CS.pShaderBytecode = shaders_[EShaderType::BorderRowUpdateIrradiance].GetData();
			auto hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::BorderRowUpdateIrradiance]));
			if (FAILED(hr))
				return false;
			ddgiVolumeResource_.unmanaged.probeBorderRowUpdateIrradiancePSO = psos_[EShaderType::BorderRowUpdateIrradiance];
		}

		{
			psoDesc.CS.BytecodeLength = shaders_[EShaderType::BorderClmUpdateIrradiance].GetSize();
			psoDesc.CS.pShaderBytecode = shaders_[EShaderType::BorderClmUpdateIrradiance].GetData();
			auto hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::BorderClmUpdateIrradiance]));
			if (FAILED(hr))
				return false;
			ddgiVolumeResource_.unmanaged.probeBorderColumnUpdateIrradiancePSO = psos_[EShaderType::BorderClmUpdateIrradiance];
		}

		{
			psoDesc.CS.BytecodeLength = shaders_[EShaderType::BorderRowUpdateDistance].GetSize();
			psoDesc.CS.pShaderBytecode = shaders_[EShaderType::BorderRowUpdateDistance].GetData();
			auto hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::BorderRowUpdateDistance]));
			if (FAILED(hr))
				return false;
			ddgiVolumeResource_.unmanaged.probeBorderRowUpdateDistancePSO = psos_[EShaderType::BorderRowUpdateDistance];
		}

		{
			psoDesc.CS.BytecodeLength = shaders_[EShaderType::BorderClmUpdateDistance].GetSize();
			psoDesc.CS.pShaderBytecode = shaders_[EShaderType::BorderClmUpdateDistance].GetData();
			auto hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::BorderClmUpdateDistance]));
			if (FAILED(hr))
				return false;
			ddgiVolumeResource_.unmanaged.probeBorderColumnUpdateDistancePSO = psos_[EShaderType::BorderClmUpdateDistance];
		}

		{
			psoDesc.CS.BytecodeLength = shaders_[EShaderType::ProbeRelocation].GetSize();
			psoDesc.CS.pShaderBytecode = shaders_[EShaderType::ProbeRelocation].GetData();
			auto hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::ProbeRelocation]));
			if (FAILED(hr))
				return false;
			ddgiVolumeResource_.unmanaged.probeRelocation.updatePSO = psos_[EShaderType::ProbeRelocation];
		}

		{
			psoDesc.CS.BytecodeLength = shaders_[EShaderType::ProbeRelocationReset].GetSize();
			psoDesc.CS.pShaderBytecode = shaders_[EShaderType::ProbeRelocationReset].GetData();
			auto hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::ProbeRelocationReset]));
			if (FAILED(hr))
				return false;
			ddgiVolumeResource_.unmanaged.probeRelocation.resetPSO = psos_[EShaderType::ProbeRelocationReset];
		}

		{
			psoDesc.CS.BytecodeLength = shaders_[EShaderType::ProbeClassification].GetSize();
			psoDesc.CS.pShaderBytecode = shaders_[EShaderType::ProbeClassification].GetData();
			auto hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::ProbeClassification]));
			if (FAILED(hr))
				return false;
			ddgiVolumeResource_.unmanaged.probeClassification.updatePSO = psos_[EShaderType::ProbeClassification];
		}

		{
			psoDesc.CS.BytecodeLength = shaders_[EShaderType::ProbeClassificationReset].GetSize();
			psoDesc.CS.pShaderBytecode = shaders_[EShaderType::ProbeClassificationReset].GetData();
			auto hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::ProbeClassificationReset]));
			if (FAILED(hr))
				return false;
			ddgiVolumeResource_.unmanaged.probeClassification.resetPSO = psos_[EShaderType::ProbeClassificationReset];
		}
	}

	return true;
}

//----
void RtxgiComponent::Destroy()
{
	for (auto&& v : shaders_) v.Destroy();
	sl12::SafeRelease(pSrvDescriptorHeap_);
	sl12::SafeRelease(pRtvDescriptorHeap_);
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
	if (pTranslate)
	{
		rtxgi::float3 origin;
		origin.x = pTranslate->x;
		origin.y = pTranslate->y;
		origin.z = pTranslate->z;
		ddgiVolume_->SetOrigin(origin);
	}
	ddgiVolume_->Update();
}

//----
void RtxgiComponent::ClearProbes(sl12::CommandList* pCmdList)
{
	ddgiVolume_->ClearProbes(pCmdList->GetLatestCommandList());
}

//----
void RtxgiComponent::UploadConstants(sl12::CommandList* pCmdList, sl12::u32 frameIndex)
{
	auto volumes = ddgiVolume_.get();
	rtxgi::d3d12::UploadDDGIVolumeConstants(pCmdList->GetLatestCommandList(), frameIndex & 0x01, 1, &volumes);
}

//----
void RtxgiComponent::UpdateProbes(sl12::CommandList* pCmdList)
{
	auto volumes = ddgiVolume_.get();
	rtxgi::d3d12::UpdateDDGIVolumeProbes(pCmdList->GetLatestCommandList(), 1, &volumes);
}

//----
void RtxgiComponent::RelocateProbes(sl12::CommandList* pCmdList, float distanceScale)
{
	auto volumes = ddgiVolume_.get();
	rtxgi::d3d12::RelocateDDGIVolumeProbes(pCmdList->GetLatestCommandList(), 1, &volumes);
}

//----
void RtxgiComponent::ClassifyProbes(sl12::CommandList* pCmdList)
{
	auto volumes = ddgiVolume_.get();
	rtxgi::d3d12::ClassifyDDGIVolumeProbes(pCmdList->GetLatestCommandList(), 1, &volumes);
}


//	EOF
