#include <sl12/rtxgi_component.h>

#include <rtxgi/ddgi/DDGIVolume.h>
#include <rtxgi/ddgi/gfx/DDGIVolume_D3D12.h>

#include <sl12/shader_manager.h>
#include <sl12/command_list.h>
#include <sl12/device.h>
#include <sl12/shader.h>


namespace
{
	static const char*	kProbeNumTexelDefine = "RTXGI_DDGI_PROBE_NUM_TEXELS";
	static const char*	kRaysPerProbeDefine = "RTXGI_DDGI_BLEND_RAYS_PER_PROBE";
	static const char*	kRadianceDefine = "RTXGI_DDGI_BLEND_RADIANCE";
	static const char*	kSharedMemoryDefine = "RTXGI_DDGI_BLEND_SHARED_MEMORY";
	static const char*	kOutputRegDefine = "OUTPUT_REGISTER";
	
	static const char*	kBlendingShaderFile = "ProbeBlendingCS.hlsl";
	static const char*	kBorderUpdateShaderFile = "ProbeBorderUpdateCS.hlsl";
	static const char*	kClassificationShaderFile = "ProbeClassificationCS.hlsl";
	static const char*	kRelocationShaderFile = "ProbeRelocationCS.hlsl";

	static const char*	kShaderEntryPoints[] = {
		"DDGIProbeBlendingCS",
		"DDGIProbeBlendingCS",
		"DDGIProbeBorderRowUpdateCS",
		"DDGIProbeBorderColumnUpdateCS",
		"DDGIProbeBorderRowUpdateCS",
		"DDGIProbeBorderColumnUpdateCS",
		"DDGIProbeRelocationCS",
		"DDGIProbeRelocationResetCS",
		"DDGIProbeClassificationCS",
		"DDGIProbeClassificationResetCS",
	};
}

namespace sl12
{
	//----
	RtxgiComponent::RtxgiComponent(Device* p, const std::string& shaderDir)
		: pParentDevice_(p)
		, shaderDirectory_(shaderDir)
	{}

	//----
	RtxgiComponent::~RtxgiComponent()
	{
		Destroy();
	}

	//----
	bool RtxgiComponent::Initialize(ShaderManager* pManager, const RtxgiVolumeDesc* descs, int numVolumes)
	{
		if (numVolumes != 1)
		{
			sl12::ConsolePrint("[RTXGI Error] current RTXGI component support only one volume.");
			return false;
		}

		if (!descs)
			return false;
		if (!pParentDevice_ || !pManager)
			return false;

		// set volume descriptor.
		rtxgi::DDGIVolumeDesc ddgiDesc;
		ddgiDesc.name = descs->name;
		ddgiDesc.index = 0;
		ddgiDesc.rngSeed = 0;
		ddgiDesc.origin = { descs->origin.x, descs->origin.y, descs->origin.z };
		ddgiDesc.eulerAngles = { descs->angle.x, descs->angle.y, descs->angle.z };
		ddgiDesc.probeSpacing = { descs->probeSpacing.x, descs->probeSpacing.y, descs->probeSpacing.z };
		ddgiDesc.probeCounts = { descs->probeCount.x, descs->probeCount.y, descs->probeCount.z };
		ddgiDesc.probeNumRays = descs->numRays;
		ddgiDesc.probeNumIrradianceTexels = descs->numIrradianceTexels;
		ddgiDesc.probeNumDistanceTexels = descs->numDistanceTexels;
		ddgiDesc.probeMaxRayDistance = descs->maxRayDistance;
		ddgiDesc.probeDistanceExponent = descs->distanceExponent;
		ddgiDesc.probeIrradianceThreshold = descs->irradianceThreshold;
		ddgiDesc.probeBrightnessThreshold = descs->brightnessThreshold;

		ddgiDesc.showProbes = true;

		ddgiDesc.probeRayDataFormat = 1;
		ddgiDesc.probeIrradianceFormat = 0;
		ddgiDesc.probeDistanceFormat = 0;
		ddgiDesc.probeDataFormat = 0;

		ddgiDesc.probeRelocationEnabled = descs->enableRelocation;
		ddgiDesc.probeMinFrontfaceDistance = 0.1f;
		ddgiDesc.probeClassificationEnabled = descs->enableClassification;

		ddgiDesc.movementType = rtxgi::EDDGIVolumeMovementType::Default;

		// initialize shaders.
		if (!InitializeShaders(pManager, ddgiDesc))
			return false;

		// new volume instance.
		ddgiVolume_.reset(new rtxgi::d3d12::DDGIVolume());

		// descriptor heap.
		auto p_device_dep = pParentDevice_->GetDeviceDep();
		{
			D3D12_DESCRIPTOR_HEAP_DESC desc;
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.NumDescriptors =
				1			// constants structured buffer count
				+ rtxgi::GetDDGIVolumeNumUAVDescriptors() * numVolumes
				+ rtxgi::GetDDGIVolumeNumSRVDescriptors() * numVolumes;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			desc.NodeMask = 0x01;
			auto hr = p_device_dep->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pSrvDescriptorHeap_));
			if (FAILED(hr))
				return false;

			desc.NumDescriptors = rtxgi::GetDDGIVolumeNumRTVDescriptors() * numVolumes;
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			// Create the RTV heap
			hr = p_device_dep->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pRtvDescriptorHeap_));
			if (FAILED(hr))
				return false;
		}

		// DDGI resources
		ddgiVolumeResource_ = std::make_unique<rtxgi::d3d12::DDGIVolumeResources>();
		ddgiVolumeResource_->descriptorHeapDesc.heap = pSrvDescriptorHeap_;
		ddgiVolumeResource_->descriptorHeapDesc.constsOffset = 0;														// constants structured buffer start index.
		ddgiVolumeResource_->descriptorHeapDesc.uavOffset = 1;															// uav start index.
		ddgiVolumeResource_->descriptorHeapDesc.srvOffset = 1 + rtxgi::GetDDGIVolumeNumUAVDescriptors() * numVolumes;	// srv start index.

		// unmanaged mode + no bindless.
		ddgiVolumeResource_->managed.enabled = false;
		ddgiVolumeResource_->unmanaged.enabled = true;
		ddgiVolumeResource_->unmanaged.rootParamSlotRootConstants = 0;
		ddgiVolumeResource_->unmanaged.rootParamSlotDescriptorTable = 1;

		// constant buffer.
		{
			sl12::u32 stride = sizeof(rtxgi::DDGIVolumeDescGPUPacked);
			sl12::u32 size = stride * numVolumes;
			if (!constantSTB_.Initialize(pParentDevice_, size, stride, sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_COMMON, false, false))
			{
				return false;
			}
			if (!constantSTBUpload_.Initialize(pParentDevice_, size * 2, stride, sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
			{
				return false;
			}
			ddgiVolumeResource_->constantsBuffer = constantSTB_.GetResourceDep();
			ddgiVolumeResource_->constantsBufferUpload = constantSTBUpload_.GetResourceDep();
			ddgiVolumeResource_->constantsBufferSizeInBytes = size;

			if (!constantSTBView_.Initialize(pParentDevice_, &constantSTB_, 0, numVolumes, stride))
			{
				return false;
			}

			// create SRV for RTXGI.
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Buffer.NumElements = numVolumes;
			srvDesc.Buffer.StructureByteStride = stride;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			auto p_device_dep = pParentDevice_->GetDeviceDep();
			UINT srvDescSize = p_device_dep->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			D3D12_CPU_DESCRIPTOR_HANDLE handle;
			handle = ddgiVolumeResource_->descriptorHeapDesc.heap->GetCPUDescriptorHandleForHeapStart();
			handle.ptr += (ddgiVolumeResource_->descriptorHeapDesc.constsOffset * srvDescSize);
			p_device_dep->CreateShaderResourceView(constantSTB_.GetResourceDep(), &srvDesc, handle);
		}

		// create texture resources.
		if (!CreateTextures(ddgiDesc, ddgiVolumeResource_.get()))
			return false;

		// create root signature and psos.
		if (!CreatePipelines(ddgiVolumeResource_.get()))
			return false;

		// create ddgi volume.
		auto status = ddgiVolume_->Create(ddgiDesc, *ddgiVolumeResource_.get());
		if (status != rtxgi::ERTXGIStatus::OK)
		{
			return false;
		}

		return true;
	}

	//----
	bool RtxgiComponent::InitializeShaders(ShaderManager* pManager, const rtxgi::DDGIVolumeDesc& ddgiDesc)
	{
		ShaderHandle handles[EShaderType::Max];

		// create base defines.
		std::vector<ShaderDefine> baseDefines;
		baseDefines.push_back(ShaderDefine("HLSL", ""));									// HLSL

		baseDefines.push_back(ShaderDefine("RTXGI_DDGI_RESOURCE_MANAGEMENT", "0"));			// unmanaged mode.
		baseDefines.push_back(ShaderDefine("RTXGI_COORDINATE_SYSTEM", "2"));				// right hand y-up.
		baseDefines.push_back(ShaderDefine("RTXGI_DDGI_SHADER_REFLECTION", "0"));			// unuse shader reflection.
		baseDefines.push_back(ShaderDefine("RTXGI_DDGI_BINDLESS_RESOURCES", "0"));			// no bindless resources.
		baseDefines.push_back(ShaderDefine("RTXGI_DDGI_DEBUG_PROBE_INDEXING", "0"));		// no debug.
		baseDefines.push_back(ShaderDefine("RTXGI_DDGI_DEBUG_OCTAHEDRAL_INDEXING", "0"));	// no debug.
		baseDefines.push_back(ShaderDefine("RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING", "0"));	// no debug.

		// register settings.
		baseDefines.push_back(ShaderDefine("CONSTS_REGISTER", "b0"));
		baseDefines.push_back(ShaderDefine("CONSTS_SPACE", "space1"));
		baseDefines.push_back(ShaderDefine("VOLUME_CONSTS_REGISTER", "t0"));
		baseDefines.push_back(ShaderDefine("VOLUME_CONSTS_SPACE", "space1"));
		baseDefines.push_back(ShaderDefine("RAY_DATA_REGISTER", "u0"));
		baseDefines.push_back(ShaderDefine("RAY_DATA_SPACE", "space1"));
		baseDefines.push_back(ShaderDefine("OUTPUT_SPACE", "space1"));
		baseDefines.push_back(ShaderDefine("PROBE_DATA_REGISTER", "u3"));
		baseDefines.push_back(ShaderDefine("PROBE_DATA_SPACE", "space1"));

		std::string rays_text = std::to_string(ddgiDesc.probeNumRays);
		std::string irr_text = std::to_string(ddgiDesc.probeNumIrradianceTexels);
		std::string dist_text = std::to_string(ddgiDesc.probeNumDistanceTexels);

		std::string file_dir = shaderDirectory_;
		if (file_dir[file_dir.length() - 1] != '\\' || file_dir[file_dir.length() - 1] != '/')
		{
			file_dir += "/";
		}

		// blending shader.
		{
			auto defines = baseDefines;
			defines.push_back(ShaderDefine(kRaysPerProbeDefine, rays_text.c_str()));
			defines.push_back(ShaderDefine(kProbeNumTexelDefine, irr_text.c_str()));
			defines.push_back(ShaderDefine(kRadianceDefine, "1"));
			defines.push_back(ShaderDefine(kSharedMemoryDefine, "1"));
			defines.push_back(ShaderDefine(kOutputRegDefine, "u1"));

			handles[EShaderType::IrradianceBlending] = pManager->CompileFromFile(
				file_dir + kBlendingShaderFile,
				kShaderEntryPoints[EShaderType::IrradianceBlending],
				ShaderType::Compute, 6, 1, nullptr, &defines);
		}
		{
			auto defines = baseDefines;
			defines.push_back(ShaderDefine(kRaysPerProbeDefine, rays_text.c_str()));
			defines.push_back(ShaderDefine(kProbeNumTexelDefine, dist_text.c_str()));
			defines.push_back(ShaderDefine(kRadianceDefine, "0"));
			defines.push_back(ShaderDefine(kSharedMemoryDefine, "1"));
			defines.push_back(ShaderDefine(kOutputRegDefine, "u2"));

			handles[EShaderType::DistanceBlending] = pManager->CompileFromFile(
				file_dir + kBlendingShaderFile,
				kShaderEntryPoints[EShaderType::DistanceBlending],
				ShaderType::Compute, 6, 1, nullptr, &defines);
		}

		// border update shader.
		{
			auto defines = baseDefines;
			defines.push_back(ShaderDefine(kProbeNumTexelDefine, irr_text.c_str()));
			defines.push_back(ShaderDefine(kRadianceDefine, "1"));
			defines.push_back(ShaderDefine(kOutputRegDefine, "u1"));

			handles[EShaderType::BorderRowUpdateIrradiance] = pManager->CompileFromFile(
				file_dir + kBorderUpdateShaderFile,
				kShaderEntryPoints[EShaderType::BorderRowUpdateIrradiance],
				ShaderType::Compute, 6, 1, nullptr, &defines);

			handles[EShaderType::BorderClmUpdateIrradiance] = pManager->CompileFromFile(
				file_dir + kBorderUpdateShaderFile,
				kShaderEntryPoints[EShaderType::BorderClmUpdateIrradiance],
				ShaderType::Compute, 6, 1, nullptr, &defines);
		}
		{
			auto defines = baseDefines;
			defines.push_back(ShaderDefine(kProbeNumTexelDefine, dist_text.c_str()));
			defines.push_back(ShaderDefine(kRadianceDefine, "0"));
			defines.push_back(ShaderDefine(kOutputRegDefine, "u2"));

			handles[EShaderType::BorderRowUpdateDistance] = pManager->CompileFromFile(
				file_dir + kBorderUpdateShaderFile,
				kShaderEntryPoints[EShaderType::BorderRowUpdateDistance],
				ShaderType::Compute, 6, 1, nullptr, &defines);

			handles[EShaderType::BorderClmUpdateDistance] = pManager->CompileFromFile(
				file_dir + kBorderUpdateShaderFile,
				kShaderEntryPoints[EShaderType::BorderClmUpdateDistance],
				ShaderType::Compute, 6, 1, nullptr, &defines);
		}

		// relocation shader.
		{
			handles[EShaderType::ProbeRelocation] = pManager->CompileFromFile(
				file_dir + kRelocationShaderFile,
				kShaderEntryPoints[EShaderType::ProbeRelocation],
				ShaderType::Compute, 6, 1, nullptr, &baseDefines);

			handles[EShaderType::ProbeRelocationReset] = pManager->CompileFromFile(
				file_dir + kRelocationShaderFile,
				kShaderEntryPoints[EShaderType::ProbeRelocationReset],
				ShaderType::Compute, 6, 1, nullptr, &baseDefines);
		}

		// classification shader.
		{
			handles[EShaderType::ProbeClassification] = pManager->CompileFromFile(
				file_dir + kClassificationShaderFile,
				kShaderEntryPoints[EShaderType::ProbeClassification],
				ShaderType::Compute, 6, 1, nullptr, &baseDefines);

			handles[EShaderType::ProbeClassificationReset] = pManager->CompileFromFile(
				file_dir + kClassificationShaderFile,
				kShaderEntryPoints[EShaderType::ProbeClassificationReset],
				ShaderType::Compute, 6, 1, nullptr, &baseDefines);
		}

		// wait compile.
		while (pManager->IsCompiling())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		// store shaders.
		for (int i = 0; i < EShaderType::Max; i++)
		{
			assert(handles[i].IsValid());
			shaders_[i] = handles[i].GetShader();
		}

		return true;
	}

	//----
	bool RtxgiComponent::CreateTextures(const rtxgi::DDGIVolumeDesc& ddgiDesc, rtxgi::d3d12::DDGIVolumeResources* ddgiResource)
	{
		sl12::TextureDesc desc{};
		desc.dimension = sl12::TextureDimension::Texture2D;
		desc.depth = 1;
		desc.mipLevels = 1;
		desc.sampleCount = 1;
		desc.isUav = true;

		// create texture resources.
		rtxgi::GetDDGIVolumeTextureDimensions(ddgiDesc, rtxgi::EDDGIVolumeTextureType::RayData, desc.width, desc.height);
		desc.format = rtxgi::d3d12::GetDDGIVolumeTextureFormat(rtxgi::EDDGIVolumeTextureType::RayData, ddgiDesc.probeRayDataFormat);
		desc.initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		desc.isRenderTarget = false;
		if (!textures_[ETextureType::RayData].Initialize(pParentDevice_, desc))
		{
			return false;
		}
		ddgiResource->unmanaged.probeRayData = textures_[ETextureType::RayData].GetResourceDep();

		rtxgi::GetDDGIVolumeTextureDimensions(ddgiDesc, rtxgi::EDDGIVolumeTextureType::Irradiance, desc.width, desc.height);
		desc.format = rtxgi::d3d12::GetDDGIVolumeTextureFormat(rtxgi::EDDGIVolumeTextureType::Irradiance, ddgiDesc.probeIrradianceFormat);
		desc.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		desc.isRenderTarget = true;
		if (!textures_[ETextureType::Irradiance].Initialize(pParentDevice_, desc))
		{
			return false;
		}
		ddgiResource->unmanaged.probeIrradiance = textures_[ETextureType::Irradiance].GetResourceDep();

		rtxgi::GetDDGIVolumeTextureDimensions(ddgiDesc, rtxgi::EDDGIVolumeTextureType::Distance, desc.width, desc.height);
		desc.format = rtxgi::d3d12::GetDDGIVolumeTextureFormat(rtxgi::EDDGIVolumeTextureType::Distance, ddgiDesc.probeDistanceFormat);
		desc.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		desc.isRenderTarget = true;
		if (!textures_[ETextureType::Distance].Initialize(pParentDevice_, desc))
		{
			return false;
		}
		ddgiResource->unmanaged.probeDistance = textures_[ETextureType::Distance].GetResourceDep();

		rtxgi::GetDDGIVolumeTextureDimensions(ddgiDesc, rtxgi::EDDGIVolumeTextureType::Data, desc.width, desc.height);
		desc.format = rtxgi::d3d12::GetDDGIVolumeTextureFormat(rtxgi::EDDGIVolumeTextureType::Data, ddgiDesc.probeDataFormat);
		desc.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		desc.isRenderTarget = false;
		if (!textures_[ETextureType::ProbeData].Initialize(pParentDevice_, desc))
		{
			return false;
		}
		ddgiResource->unmanaged.probeData = textures_[ETextureType::ProbeData].GetResourceDep();

		// store descriptor heap.
		{
			auto p_device_dep = pParentDevice_->GetDeviceDep();

			D3D12_CPU_DESCRIPTOR_HANDLE srvHandle, uavHandle, rtvHandle;
			srvHandle = uavHandle = ddgiResource->descriptorHeapDesc.heap->GetCPUDescriptorHandleForHeapStart();
			rtvHandle = pRtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

			UINT srvDescSize = p_device_dep->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			UINT rtvDescSize = p_device_dep->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

			uavHandle.ptr += (ddgiResource->descriptorHeapDesc.uavOffset * srvDescSize);
			srvHandle.ptr += (ddgiResource->descriptorHeapDesc.srvOffset * srvDescSize);

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
				srvDesc.Format = uavDesc.Format = rtxgi::d3d12::GetDDGIVolumeTextureFormat(rtxgi::EDDGIVolumeTextureType::RayData, ddgiDesc.probeRayDataFormat);
				p_device_dep->CreateUnorderedAccessView(ddgiResource->unmanaged.probeRayData, nullptr, &uavDesc, uavHandle);
				p_device_dep->CreateShaderResourceView(ddgiResource->unmanaged.probeRayData, &srvDesc, srvHandle);
			}

			uavHandle.ptr += srvDescSize;
			srvHandle.ptr += srvDescSize;

			// irradiance.
			{
				srvDesc.Format = uavDesc.Format = rtxgi::d3d12::GetDDGIVolumeTextureFormat(rtxgi::EDDGIVolumeTextureType::Irradiance, ddgiDesc.probeIrradianceFormat);
				p_device_dep->CreateUnorderedAccessView(ddgiResource->unmanaged.probeIrradiance, nullptr, &uavDesc, uavHandle);
				p_device_dep->CreateShaderResourceView(ddgiResource->unmanaged.probeIrradiance, &srvDesc, srvHandle);

				ddgiResource->unmanaged.probeIrradianceRTV.ptr = rtvHandle.ptr + (ddgiDesc.index * rtxgi::GetDDGIVolumeNumRTVDescriptors() * rtvDescSize);
				p_device_dep->CreateRenderTargetView(ddgiResource->unmanaged.probeIrradiance, &rtvDesc, ddgiResource->unmanaged.probeIrradianceRTV);
			}

			uavHandle.ptr += srvDescSize;
			srvHandle.ptr += srvDescSize;
			rtvHandle.ptr += rtvDescSize;

			// distance.
			{
				srvDesc.Format = uavDesc.Format = rtxgi::d3d12::GetDDGIVolumeTextureFormat(rtxgi::EDDGIVolumeTextureType::Distance, ddgiDesc.probeDistanceFormat);
				p_device_dep->CreateUnorderedAccessView(ddgiResource->unmanaged.probeDistance, nullptr, &uavDesc, uavHandle);
				p_device_dep->CreateShaderResourceView(ddgiResource->unmanaged.probeDistance, &srvDesc, srvHandle);

				ddgiResource->unmanaged.probeDistanceRTV.ptr = ddgiResource->unmanaged.probeIrradianceRTV.ptr + rtvDescSize;
				p_device_dep->CreateRenderTargetView(ddgiResource->unmanaged.probeDistance, &rtvDesc, ddgiResource->unmanaged.probeDistanceRTV);
			}

			uavHandle.ptr += srvDescSize;
			srvHandle.ptr += srvDescSize;

			// Probe data texture descriptors
			{
				srvDesc.Format = uavDesc.Format = rtxgi::d3d12::GetDDGIVolumeTextureFormat(rtxgi::EDDGIVolumeTextureType::Data, ddgiDesc.probeDataFormat);
				p_device_dep->CreateUnorderedAccessView(ddgiResource->unmanaged.probeData, nullptr, &uavDesc, uavHandle);
				p_device_dep->CreateShaderResourceView(ddgiResource->unmanaged.probeData, &srvDesc, srvHandle);
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
	bool RtxgiComponent::CreatePipelines(rtxgi::d3d12::DDGIVolumeResources* ddgiResource)
	{
		auto p_device_dep = pParentDevice_->GetDeviceDep();

		// create root signature.
		{
			ID3DBlob* signature;
			if (!rtxgi::d3d12::GetDDGIVolumeRootSignatureDesc(ddgiResource->descriptorHeapDesc.constsOffset, ddgiResource->descriptorHeapDesc.uavOffset, signature))
				return false;
			if (signature == nullptr)
				return false;

			HRESULT hr = p_device_dep->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&pRootSignature_));
			sl12::SafeRelease(signature);
			if (FAILED(hr))
				return false;

			ddgiResource->unmanaged.rootSignature = pRootSignature_;
		}

		// create psos.
		{
			D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.pRootSignature = ddgiResource->unmanaged.rootSignature;

			{
				psoDesc.CS.BytecodeLength = shaders_[EShaderType::IrradianceBlending]->GetSize();
				psoDesc.CS.pShaderBytecode = shaders_[EShaderType::IrradianceBlending]->GetData();
				auto hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::IrradianceBlending]));
				if (FAILED(hr))
					return false;
				ddgiResource->unmanaged.probeBlendingIrradiancePSO = psos_[EShaderType::IrradianceBlending];
			}

			{
				psoDesc.CS.BytecodeLength = shaders_[EShaderType::DistanceBlending]->GetSize();
				psoDesc.CS.pShaderBytecode = shaders_[EShaderType::DistanceBlending]->GetData();
				auto hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::DistanceBlending]));
				if (FAILED(hr))
					return false;
				ddgiResource->unmanaged.probeBlendingDistancePSO = psos_[EShaderType::DistanceBlending];
			}

			{
				psoDesc.CS.BytecodeLength = shaders_[EShaderType::BorderRowUpdateIrradiance]->GetSize();
				psoDesc.CS.pShaderBytecode = shaders_[EShaderType::BorderRowUpdateIrradiance]->GetData();
				auto hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::BorderRowUpdateIrradiance]));
				if (FAILED(hr))
					return false;
				ddgiResource->unmanaged.probeBorderRowUpdateIrradiancePSO = psos_[EShaderType::BorderRowUpdateIrradiance];
			}

			{
				psoDesc.CS.BytecodeLength = shaders_[EShaderType::BorderClmUpdateIrradiance]->GetSize();
				psoDesc.CS.pShaderBytecode = shaders_[EShaderType::BorderClmUpdateIrradiance]->GetData();
				auto hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::BorderClmUpdateIrradiance]));
				if (FAILED(hr))
					return false;
				ddgiResource->unmanaged.probeBorderColumnUpdateIrradiancePSO = psos_[EShaderType::BorderClmUpdateIrradiance];
			}

			{
				psoDesc.CS.BytecodeLength = shaders_[EShaderType::BorderRowUpdateDistance]->GetSize();
				psoDesc.CS.pShaderBytecode = shaders_[EShaderType::BorderRowUpdateDistance]->GetData();
				auto hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::BorderRowUpdateDistance]));
				if (FAILED(hr))
					return false;
				ddgiResource->unmanaged.probeBorderRowUpdateDistancePSO = psos_[EShaderType::BorderRowUpdateDistance];
			}

			{
				psoDesc.CS.BytecodeLength = shaders_[EShaderType::BorderClmUpdateDistance]->GetSize();
				psoDesc.CS.pShaderBytecode = shaders_[EShaderType::BorderClmUpdateDistance]->GetData();
				auto hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::BorderClmUpdateDistance]));
				if (FAILED(hr))
					return false;
				ddgiResource->unmanaged.probeBorderColumnUpdateDistancePSO = psos_[EShaderType::BorderClmUpdateDistance];
			}

			{
				psoDesc.CS.BytecodeLength = shaders_[EShaderType::ProbeRelocation]->GetSize();
				psoDesc.CS.pShaderBytecode = shaders_[EShaderType::ProbeRelocation]->GetData();
				auto hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::ProbeRelocation]));
				if (FAILED(hr))
					return false;
				ddgiResource->unmanaged.probeRelocation.updatePSO = psos_[EShaderType::ProbeRelocation];
			}

			{
				psoDesc.CS.BytecodeLength = shaders_[EShaderType::ProbeRelocationReset]->GetSize();
				psoDesc.CS.pShaderBytecode = shaders_[EShaderType::ProbeRelocationReset]->GetData();
				auto hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::ProbeRelocationReset]));
				if (FAILED(hr))
					return false;
				ddgiResource->unmanaged.probeRelocation.resetPSO = psos_[EShaderType::ProbeRelocationReset];
			}

			{
				psoDesc.CS.BytecodeLength = shaders_[EShaderType::ProbeClassification]->GetSize();
				psoDesc.CS.pShaderBytecode = shaders_[EShaderType::ProbeClassification]->GetData();
				auto hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::ProbeClassification]));
				if (FAILED(hr))
					return false;
				ddgiResource->unmanaged.probeClassification.updatePSO = psos_[EShaderType::ProbeClassification];
			}

			{
				psoDesc.CS.BytecodeLength = shaders_[EShaderType::ProbeClassificationReset]->GetSize();
				psoDesc.CS.pShaderBytecode = shaders_[EShaderType::ProbeClassificationReset]->GetData();
				auto hr = p_device_dep->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psos_[EShaderType::ProbeClassificationReset]));
				if (FAILED(hr))
					return false;
				ddgiResource->unmanaged.probeClassification.resetPSO = psos_[EShaderType::ProbeClassificationReset];
			}
		}

		return true;
	}

	//----
	void RtxgiComponent::Destroy()
	{
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

	//----
	int RtxgiComponent::GetNumProbes() const
	{
		return ddgiVolume_->GetNumProbes();
	}
	int RtxgiComponent::GetNumRaysPerProbe() const
	{
		return ddgiVolume_->GetNumRaysPerProbe();
	}

	//----
	ConstantBufferCache::Handle RtxgiComponent::CreateConstantBuffer(ConstantBufferCache* pCache, int volumeIndex)
	{
		// TODO: need implement multi volume.
		rtxgi::DDGIConstants cb;
		cb.volumeIndex = volumeIndex;
		cb.uavOffset = 0;
		cb.srvOffset = 0;
		return pCache->GetUnusedConstBuffer(sizeof(cb), &cb);
	}

	//----
	void RtxgiComponent::SetDescHysteresis(float v)
	{
		ddgiVolume_->SetProbeHysteresis(v);
	}
	void RtxgiComponent::SetDescIrradianceThreshold(float v)
	{
		ddgiVolume_->SetProbeIrradianceThreshold(v);
	}
	void RtxgiComponent::SetDescBrightnessThreshold(float v)
	{
		ddgiVolume_->SetProbeBrightnessThreshold(v);
	}

}	// namespace sl12


//	EOF
