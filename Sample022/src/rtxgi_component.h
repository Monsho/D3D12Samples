#pragma once

#include <rtxgi/ddgi/DDGIVolume.h>

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
			BorderRowUpdate,
			BorderColumnUpdate,
			ProbeRelocation,
			StateClassifier,
			StateActivateAll,
			Max
		};
	};

	struct ETextureType
	{
		enum Type
		{
			Radiance,
			Irradiance,
			Distance,
			Offset,
			State,
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
	void UpdateProbes(sl12::CommandList* pCmdList);
	void RelocateProbes(sl12::CommandList* pCmdList, float distanceScale);
	void ClassifyProbes(sl12::CommandList* pCmdList);

	sl12::TextureView* GetIrradianceSRV()
	{
		return &textureSrvs_[ETextureType::Irradiance];
	}
	sl12::TextureView* GetDistanceSRV()
	{
		return &textureSrvs_[ETextureType::Distance];
	}
	sl12::Texture* GetRadiance()
	{
		return &textures_[ETextureType::Radiance];
	}
	sl12::UnorderedAccessView* GetRadianceUAV()
	{
		return &textureUavs_[ETextureType::Radiance];
	}
	sl12::UnorderedAccessView* GetOffsetUAV()
	{
		return &textureUavs_[ETextureType::Offset];
	}
	sl12::UnorderedAccessView* GetStateUAV()
	{
		return &textureUavs_[ETextureType::State];
	}
	sl12::ConstantBufferView* GetCurrentVolumeCBV()
	{
		return &volumeCBVs_[flipIndex_];
	}
	const rtxgi::DDGIVolume* GetDDGIVolume() const
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
	void SetDescChangeThreshold(float v)
	{
		ddgiVolume_->SetProbeChangeThreshold(v);
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
	std::unique_ptr<rtxgi::DDGIVolume>	ddgiVolume_ = nullptr;
	rtxgi::DDGIVolumeDesc				ddgiVolumeDesc_{};
	rtxgi::DDGIVolumeResources			ddgiVolumeResource_{};

	// system objects
	sl12::Device*				pParentDevice_ = nullptr;
	sl12::Shader				shaders_[EShaderType::Max];
	ID3D12DescriptorHeap*		pDescriptorHeap_ = nullptr;
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
