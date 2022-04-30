#pragma once

#include <sl12/util.h>
#include <vector>


namespace sl12
{
	class Device;
	class Shader;
	struct RaytracingDescriptorCount;

	struct ShaderVisibility
	{
		enum Type
		{
			Vertex			= 0x01 << 0,
			Pixel			= 0x01 << 1,
			Geometry		= 0x01 << 2,
			Domain			= 0x01 << 3,
			Hull			= 0x01 << 4,
			Compute			= 0x01 << 5,

			All				= Vertex | Pixel | Geometry | Domain | Hull | Compute
		};
	};	// struct ShaderVisibility

	struct RootParameterType
	{
		enum Type
		{
			ConstantBuffer,
			ShaderResource,
			UnorderedAccess,
			Sampler,

			Max
		};
	};	// struct RootParameterType

	struct RootParameter
	{
		RootParameterType::Type		type;
		u32							shaderVisibility;
		u32							registerIndex;

		RootParameter(RootParameterType::Type t = RootParameterType::ConstantBuffer, u32 shaderVis = ShaderVisibility::All, u32 regIndex = 0)
			: type(t), shaderVisibility(shaderVis), registerIndex(regIndex)
		{}
	};	// struct RootParameter

	struct RootSignatureDesc
	{
		u32							numParameters = 0;
		const RootParameter*		pParameters = nullptr;
		D3D12_ROOT_SIGNATURE_FLAGS	flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	};	// struct RootSignatureDesc

	struct RootBindlessInfo
	{
		u8		space_ = 0;
		u8		index_ = 0;
		u32		maxResources_ = 0;

		RootBindlessInfo(u8 space = 0, u32 maxRes = 0)
			: space_(space), maxResources_(maxRes)
		{}
	};	// struct RootBindlessInfo

	class RootSignature
	{
	public:
		struct InputIndex
		{
			u8		vsCbvIndex_ = 0;
			u8		vsSrvIndex_ = 0;
			u8		vsSamplerIndex_ = 0;
			u8		psCbvIndex_ = 0;
			u8		psSrvIndex_ = 0;
			u8		psSamplerIndex_ = 0;
			u8		psUavIndex_ = 0;
			u8		gsCbvIndex_ = 0;
			u8		gsSrvIndex_ = 0;
			u8		gsSamplerIndex_ = 0;
			u8		hsCbvIndex_ = 0;
			u8		hsSrvIndex_ = 0;
			u8		hsSamplerIndex_ = 0;
			u8		dsCbvIndex_ = 0;
			u8		dsSrvIndex_ = 0;
			u8		dsSamplerIndex_ = 0;
			u8		csCbvIndex_ = 0;
			u8		csSrvIndex_ = 0;
			u8		csSamplerIndex_ = 0;
			u8		csUavIndex_ = 0;
			u8		msCbvIndex_ = 0;
			u8		msSrvIndex_ = 0;
			u8		msSamplerIndex_ = 0;
			u8		asCbvIndex_ = 0;
			u8		asSrvIndex_ = 0;
			u8		asSamplerIndex_ = 0;
		};	// struct InputIndex

	public:
		RootSignature()
		{}
		~RootSignature()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, const RootSignatureDesc& desc);
		bool Initialize(Device* pDev, const D3D12_ROOT_SIGNATURE_DESC& desc);
		bool Initialize(Device* pDev, const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& desc);
		bool Initialize(Device* pDev, Shader* vs, Shader* ps, Shader* gs, Shader* hs, Shader* ds);
		bool Initialize(Device* pDev, Shader* as, Shader* ms, Shader* ps);
		bool Initialize(Device* pDev, Shader* cs);
		bool InitializeWithBindless(Device* pDev, Shader* vs, Shader* ps, Shader* gs, Shader* hs, Shader* ds, const RootBindlessInfo* bindlessInfos, u32 bindlessCount);
		bool InitializeWithBindless(Device* pDev, Shader* as, Shader* ms, Shader* ps, const RootBindlessInfo* bindlessInfos, u32 bindlessCount);
		bool InitializeWithBindless(Device* pDev, Shader* cs, const RootBindlessInfo* bindlessInfos, u32 bindlessCount);
		void Destroy();

		// getter
		ID3D12RootSignature* GetRootSignature() { return pRootSignature_; }
		const InputIndex& GetInputIndex() const { return inputIndex_; }
		const std::vector<RootBindlessInfo>& GetBindlessInfos() { return bindlessInfos_; }

	private:
		ID3D12RootSignature*			pRootSignature_{ nullptr };
		InputIndex						inputIndex_;
		std::vector<RootBindlessInfo>	bindlessInfos_;
	};	// class RootSignature


	/**
	 * @brief レイトレーシング用RootSignatureを作成するヘルパー関数
	*/
	bool CreateRaytracingRootSignature(
		sl12::Device* pDevice,
		sl12::u32 asCount,
		sl12::u32 globalCbvCount,
		sl12::u32 globalSrvCount,
		sl12::u32 globalUavCount,
		sl12::u32 globalSamplerCount,
		sl12::RootSignature* pGlobalRS,
		sl12::RootSignature* pLocalRS);
	bool CreateRaytracingRootSignature(
		sl12::Device* pDevice,
		sl12::u32 asCount,
		const RaytracingDescriptorCount& globalCount,
		const RaytracingDescriptorCount& localCount,
		sl12::RootSignature* pGlobalRS,
		sl12::RootSignature* pLocalRS);

}	// namespace sl12

//	EOF
