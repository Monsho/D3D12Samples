#pragma once

#include <memory>
#include <sl12/buffer.h>
#include <sl12/texture.h>
#include <sl12/buffer_view.h>
#include <sl12/texture_view.h>
#include <sl12/constant_buffer_cache.h>


namespace rtxgi
{
	struct DDGIVolumeDesc;

	namespace d3d12
	{
		class DDGIVolume;
		struct DDGIVolumeResources;
	}	// namespace d3d12
}	// namespace rtxgi

namespace sl12
{
	class Device;
	class CommandList;
	class Shader;
	class ShaderManager;

	struct RtxgiVolumeDesc
	{
		std::string			name;
		DirectX::XMFLOAT3	origin = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
		DirectX::XMFLOAT3	angle = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
		DirectX::XMFLOAT3	probeSpacing = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
		DirectX::XMINT3		probeCount = DirectX::XMINT3(0, 0, 0);
		int					numRays = 144;
		int					numIrradianceTexels = 6;
		int					numDistanceTexels = 14;
		float				maxRayDistance = 20.0f;
		float				distanceExponent = 50.0f;
		float				hysteresis = 0.97f;
		float				irradianceThreshold = 0.25f;
		float				brightnessThreshold = 0.1f;
		bool				enableRelocation = true;
		bool				enableClassification = true;
	};	// struct RtxgiVolumeDesc

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
		RtxgiComponent(Device* p, const std::string& shaderDir);
		~RtxgiComponent();

		bool Initialize(ShaderManager* pManager, const RtxgiVolumeDesc* descs, int numVolumes);
		void Destroy();

		void UpdateVolume(const DirectX::XMFLOAT3* pTranslate);
		void ClearProbes(CommandList* pCmdList);
		void UploadConstants(CommandList* pCmdList, u32 frameIndex);
		void UpdateProbes(CommandList* pCmdList);
		void RelocateProbes(CommandList* pCmdList, float distanceScale);
		void ClassifyProbes(CommandList* pCmdList);

		BufferView* GetConstantSTBView()
		{
			return &constantSTBView_;
		}
		TextureView* GetIrradianceSRV()
		{
			return &textureSrvs_[ETextureType::Irradiance];
		}
		TextureView* GetDistanceSRV()
		{
			return &textureSrvs_[ETextureType::Distance];
		}
		TextureView* GetProbeDataSRV()
		{
			return &textureSrvs_[ETextureType::ProbeData];
		}
		Texture* GetRayData()
		{
			return &textures_[ETextureType::RayData];
		}
		UnorderedAccessView* GetRayDataUAV()
		{
			return &textureUavs_[ETextureType::RayData];
		}
		UnorderedAccessView* GetProbeDataUAV()
		{
			return &textureUavs_[ETextureType::ProbeData];
		}
		ConstantBufferView* GetCurrentVolumeCBV()
		{
			return &volumeCBVs_[flipIndex_];
		}
		const rtxgi::d3d12::DDGIVolume* GetDDGIVolume() const
		{
			return ddgiVolume_.get();
		}
		int GetNumProbes() const;
		int GetNumRaysPerProbe() const;

		ConstantBufferCache::Handle CreateConstantBuffer(ConstantBufferCache* pCache, int volumeIndex);

		void SetDescHysteresis(float v);
		void SetDescIrradianceThreshold(float v);
		void SetDescBrightnessThreshold(float v);

	private:
		bool InitializeShaders(ShaderManager* pManager, const rtxgi::DDGIVolumeDesc& ddgiDesc);
		bool CreateTextures(const rtxgi::DDGIVolumeDesc& ddgiDesc, rtxgi::d3d12::DDGIVolumeResources* ddgiResource);
		bool CreatePipelines(rtxgi::d3d12::DDGIVolumeResources* ddgiResource);

	private:
		// rtxgi
		std::unique_ptr<rtxgi::d3d12::DDGIVolume>			ddgiVolume_ = nullptr;
		std::unique_ptr<rtxgi::d3d12::DDGIVolumeResources>	ddgiVolumeResource_ = nullptr;

		// system objects
		Device*						pParentDevice_ = nullptr;
		Shader*						shaders_[EShaderType::Max];
		ID3D12DescriptorHeap*		pSrvDescriptorHeap_ = nullptr;
		ID3D12DescriptorHeap*		pRtvDescriptorHeap_ = nullptr;
		Buffer						constantSTB_;
		Buffer						constantSTBUpload_;
		BufferView					constantSTBView_;
		Buffer						volumeCBs_[2];
		ConstantBufferView			volumeCBVs_[2];
		Texture						textures_[ETextureType::Max];
		TextureView					textureSrvs_[ETextureType::Max];
		UnorderedAccessView			textureUavs_[ETextureType::Max];
		ID3D12RootSignature*		pRootSignature_ = nullptr;
		ID3D12PipelineState*		psos_[EShaderType::Max]{};

		std::string					shaderDirectory_;
		int							flipIndex_ = 0;
	};	// class RtxgiComponent

}	// namespace sl12


//	EOF
