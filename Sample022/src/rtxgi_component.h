#pragma once

#include "../shader/rtxgi/rtxgi_common_defines.h"
#include <rtxgi/ddgi/DDGIVolume.h>
#include <rtxgi/ddgi/gfx/DDGIVolume_D3D12.h>

#include <memory>
#include <sl12/device.h>
#include <sl12/shader.h>
#include <sl12/buffer.h>
#include <sl12/texture.h>
#include <sl12/command_list.h>
#include <sl12/buffer_view.h>
#include <sl12/texture_view.h>


class RtxgiComponent
{
	struct EShaderType
	{
		enum Type
		{
			IrradianceBlending,
			DistanceBlending,
			BorderRowUpdateIrradiance,
			BorderClmUpdateIrradiance,
			BorderRowUpdateDistance,
			BorderClmUpdateDistance,
			ProbeRelocation,
			ProbeRelocationReset,
			ProbeClassification,
			ProbeClassificationReset,

			Max
		};
	};

	struct ETextureType
	{
		enum Type
		{
			RayData,
			Irradiance,
			Distance,
			ProbeData,

			Max
		};
	};

public:
	RtxgiComponent(sl12::Device* p)
		: pParentDevice_(p)
	{}
	~RtxgiComponent()
	{
		Destroy();
	}

	bool Initialize();
	void Destroy();

	void UpdateVolume(const DirectX::XMFLOAT3* pTranslate);
	void ClearProbes(sl12::CommandList* pCmdList);
	void UploadConstants(sl12::CommandList* pCmdList, sl12::u32 frameIndex);
	void UpdateProbes(sl12::CommandList* pCmdList);
	void RelocateProbes(sl12::CommandList* pCmdList, float distanceScale);
	void ClassifyProbes(sl12::CommandList* pCmdList);

	sl12::BufferView* GetConstantSTBView()
	{
		return &constantSTBView_;
	}
	sl12::TextureView* GetIrradianceSRV()
	{
		return &textureSrvs_[ETextureType::Irradiance];
	}
	sl12::TextureView* GetDistanceSRV()
	{
		return &textureSrvs_[ETextureType::Distance];
	}
	sl12::TextureView* GetProbeDataSRV()
	{
		return &textureSrvs_[ETextureType::ProbeData];
	}
	sl12::Texture* GetRayData()
	{
		return &textures_[ETextureType::RayData];
	}
	sl12::UnorderedAccessView* GetRayDataUAV()
	{
		return &textureUavs_[ETextureType::RayData];
	}
	sl12::UnorderedAccessView* GetProbeDataUAV()
	{
		return &textureUavs_[ETextureType::ProbeData];
	}
	sl12::ConstantBufferView* GetCurrentVolumeCBV()
	{
		return &volumeCBVs_[flipIndex_];
	}
	const rtxgi::d3d12::DDGIVolume* GetDDGIVolume() const
	{
		return ddgiVolume_.get();
	}
	const rtxgi::DDGIVolumeDesc& GetDDGIVolumeDesc() const
	{
		return ddgiVolumeDesc_;
	}

	void SetDescHysteresis(float v)
	{
		ddgiVolume_->SetProbeHysteresis(v);
	}
	void SetDescIrradianceThreshold(float v)
	{
		ddgiVolume_->SetProbeIrradianceThreshold(v);
	}
	void SetDescBrightnessThreshold(float v)
	{
		ddgiVolume_->SetProbeBrightnessThreshold(v);
	}

private:
	bool InitializeShaders();
	bool CreateTextures();
	bool CreatePipelines();

private:
	// rtxgi
	std::unique_ptr<rtxgi::d3d12::DDGIVolume>	ddgiVolume_ = nullptr;
	rtxgi::DDGIVolumeDesc						ddgiVolumeDesc_{};
	rtxgi::d3d12::DDGIVolumeResources			ddgiVolumeResource_{};

	// system objects
	sl12::Device*				pParentDevice_ = nullptr;
	sl12::Shader				shaders_[EShaderType::Max];
	ID3D12DescriptorHeap*		pSrvDescriptorHeap_ = nullptr;
	ID3D12DescriptorHeap*		pRtvDescriptorHeap_ = nullptr;
	sl12::Buffer				constantSTB_;
	sl12::Buffer				constantSTBUpload_;
	sl12::BufferView			constantSTBView_;
	sl12::Buffer				volumeCBs_[2];
	sl12::ConstantBufferView	volumeCBVs_[2];
	sl12::Texture				textures_[ETextureType::Max];
	sl12::TextureView			textureSrvs_[ETextureType::Max];
	sl12::UnorderedAccessView	textureUavs_[ETextureType::Max];
	ID3D12RootSignature*		pRootSignature_ = nullptr;
	ID3D12PipelineState*		psos_[EShaderType::Max]{};

	int							flipIndex_ = 0;
};	// class RtxgiComponent


//	EOF
