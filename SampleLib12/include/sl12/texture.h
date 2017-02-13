#pragma once

#include <sl12/util.h>
#include <DirectXTex.h>


namespace sl12
{
	class Device;
	class CommandList;

	// テクスチャの次元
	struct TextureDimension
	{
		enum Type
		{
			Texture1D = 0,
			Texture2D,
			Texture3D,

			Max
		};
	};	// struct TextureDimension

	// テクスチャ生成時の記述情報
	struct TextureDesc
	{
		TextureDimension::Type	dimension;
		u32						width, height, depth;
		u32						mipLevels;
		DXGI_FORMAT				format;
		u32						sampleCount;
		float					clearColor[4];
		float					clearDepth;
		u8						clearStencil;
		bool					isRenderTarget;
		bool					isDepthBuffer;
		bool					isUav;
	};	// struct TextureDesc

	class Texture
	{
		friend class CommandList;

	public:
		Texture()
		{}
		~Texture()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, const TextureDesc& desc);
		bool InitializeFromDXImage(Device* pDev, const DirectX::ScratchImage& image, bool isForceSRGB);
		bool InitializeFromTGA(Device* pDev, CommandList* pCmdList, const void* pTgaBin, size_t size, bool isForceSRGB);

		bool UpdateImage(Device* pDev, CommandList* pCmdList, const DirectX::ScratchImage& image, ID3D12Resource** ppSrcImage);

		void Destroy();

		// getter
		ID3D12Resource* GetResourceDep() { return pResource_; }
		const TextureDesc& GetTextureDesc() const { return textureDesc_; }
		const D3D12_RESOURCE_DESC& GetResourceDesc() const { return resourceDesc_; }

	private:
		ID3D12Resource*			pResource_{ nullptr };
		TextureDesc				textureDesc_{};
		D3D12_RESOURCE_DESC		resourceDesc_{};
		D3D12_RESOURCE_STATES	currentState_{ D3D12_RESOURCE_STATE_COMMON };
		D3D12_CLEAR_VALUE		clearValue_{};
	};	// class Texture

}	// namespace sl12

//	EOF
