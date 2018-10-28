#pragma once

#include <sl12/util.h>


namespace sl12
{
	class Device;

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

	class RootSignature
	{
	public:
		RootSignature()
		{}
		~RootSignature()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, const RootSignatureDesc& desc);
		bool Initialize(Device* pDev, const D3D12_ROOT_SIGNATURE_DESC& desc);
		void Destroy();

		// getter
		ID3D12RootSignature* GetRootSignature() { return pRootSignature_; }

	private:
		ID3D12RootSignature*		pRootSignature_{ nullptr };
	};	// class RootSignature

}	// namespace sl12

//	EOF
