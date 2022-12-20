#include <sl12/texture.h>

#include <sl12/device.h>
#include <sl12/command_list.h>
#include <sl12/fence.h>
#include <sl12/swapchain.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../../External/stb/stb_image.h"
#define TINYEXR_IMPLEMENTATION
#include "../../External/tinyexr/tinyexr.h"


namespace sl12
{
	//----
	bool Texture::Initialize(Device* pDev, const TextureDesc& desc)
	{
		const D3D12_RESOURCE_DIMENSION kDimensionTable[] = {
			D3D12_RESOURCE_DIMENSION_TEXTURE1D,
			D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			D3D12_RESOURCE_DIMENSION_TEXTURE3D,
		};

		D3D12_HEAP_PROPERTIES prop{};
		prop.Type = D3D12_HEAP_TYPE_DEFAULT;
		prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		prop.CreationNodeMask = 1;
		prop.VisibleNodeMask = 1;

		if (desc.forceSysRam)
		{
			prop.Type = D3D12_HEAP_TYPE_CUSTOM;
			prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
			prop.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
		}

		D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE;

		resourceDesc_.Dimension = kDimensionTable[desc.dimension];
		resourceDesc_.Alignment = 0;
		resourceDesc_.Width = desc.width;
		resourceDesc_.Height = desc.height;
		resourceDesc_.DepthOrArraySize = desc.depth;
		resourceDesc_.MipLevels = desc.mipLevels;
		resourceDesc_.Format = desc.format;
		resourceDesc_.SampleDesc.Count = desc.sampleCount;
		resourceDesc_.SampleDesc.Quality = 0;
		resourceDesc_.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc_.Flags = desc.isRenderTarget ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE;
		resourceDesc_.Flags |= desc.isDepthBuffer ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_NONE;
		resourceDesc_.Flags |= desc.isUav ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;

		// 深度バッファの場合はリソースフォーマットをTYPELESSにしなければならない
		switch (resourceDesc_.Format)
		{
		case DXGI_FORMAT_D32_FLOAT:
			resourceDesc_.Format = DXGI_FORMAT_R32_TYPELESS; break;
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			resourceDesc_.Format = DXGI_FORMAT_R32G8X24_TYPELESS; break;
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
			resourceDesc_.Format = DXGI_FORMAT_R24G8_TYPELESS; break;
		case DXGI_FORMAT_D16_UNORM:
			resourceDesc_.Format = DXGI_FORMAT_R16_TYPELESS; break;
		}

		currentState_ = desc.initialState;

		D3D12_CLEAR_VALUE* pClearValue = nullptr;
		D3D12_CLEAR_VALUE clearValue{};
		if (desc.isRenderTarget)
		{
			pClearValue = &clearValue_;
			clearValue_.Format = desc.format;
			memcpy(clearValue_.Color, desc.clearColor, sizeof(clearValue_.Color));

			if (currentState_ == D3D12_RESOURCE_STATE_COMMON)
				currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
		}
		else if (desc.isDepthBuffer)
		{
			pClearValue = &clearValue_;
			clearValue_.Format = desc.format;
			clearValue_.DepthStencil.Depth = desc.clearDepth;
			clearValue_.DepthStencil.Stencil = desc.clearStencil;

			if (currentState_ == D3D12_RESOURCE_STATE_COMMON)
				currentState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}
		else
		{
			if (currentState_ == D3D12_RESOURCE_STATE_COMMON)
				currentState_ = D3D12_RESOURCE_STATE_COPY_DEST;
		}

		auto hr = pDev->GetDeviceDep()->CreateCommittedResource(&prop, flags, &resourceDesc_, currentState_, pClearValue, IID_PPV_ARGS(&pResource_));
		if (FAILED(hr))
		{
			return false;
		}

		textureDesc_ = desc;

		return true;
	}

	//----
	bool Texture::InitializeFromDXImage(Device* pDev, const DirectX::ScratchImage& image, bool isForceSRGB, bool forceSysRam)
	{
		if (!pDev)
		{
			return false;
		}

		// テクスチャオブジェクト作成
		const DirectX::TexMetadata& meta = image.GetMetadata();
		if (!meta.mipLevels || !meta.arraySize)
		{
			return false;
		}
		if ((meta.width > UINT32_MAX) || (meta.height > UINT32_MAX)
			|| (meta.mipLevels > UINT16_MAX) || (meta.arraySize > UINT16_MAX))
		{
			return false;
		}

		DXGI_FORMAT format = meta.format;
		if (isForceSRGB)
		{
			switch (format)
			{
			case DXGI_FORMAT_R8G8B8A8_UNORM: format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; break;
			case DXGI_FORMAT_BC1_UNORM: format = DXGI_FORMAT_BC1_UNORM_SRGB; break;
			case DXGI_FORMAT_BC2_UNORM: format = DXGI_FORMAT_BC2_UNORM_SRGB; break;
			case DXGI_FORMAT_BC3_UNORM: format = DXGI_FORMAT_BC3_UNORM_SRGB; break;
			case DXGI_FORMAT_B8G8R8A8_UNORM: format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; break;
			case DXGI_FORMAT_B8G8R8X8_UNORM: format = DXGI_FORMAT_B8G8R8X8_UNORM_SRGB; break;
			case DXGI_FORMAT_BC7_UNORM: format = DXGI_FORMAT_BC7_UNORM_SRGB; break;
			}
		}

		D3D12_RESOURCE_DESC desc = {};
		desc.Width = static_cast<u32>(meta.width);
		desc.Height = static_cast<u32>(meta.height);
		desc.MipLevels = static_cast<u16>(meta.mipLevels);
		desc.DepthOrArraySize = (meta.dimension == DirectX::TEX_DIMENSION_TEXTURE3D)
			? static_cast<u16>(meta.depth)
			: static_cast<u16>(meta.arraySize);
		desc.Format = format;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		desc.SampleDesc.Count = 1;
		desc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(meta.dimension);

		D3D12_HEAP_PROPERTIES heapProp = {};
		heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProp.CreationNodeMask = 1;
		heapProp.VisibleNodeMask = 1;

		if (forceSysRam)
		{
			heapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
			heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
			heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
		}

		auto hr = pDev->GetDeviceDep()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&pResource_));
		if (FAILED(hr))
		{
			return false;
		}

		// 情報を格納
		resourceDesc_ = desc;
		currentState_ = D3D12_RESOURCE_STATE_COPY_DEST;
		memset(&textureDesc_, 0, sizeof(textureDesc_));
		textureDesc_.width = static_cast<u32>(desc.Width);
		textureDesc_.height = desc.Height;
		textureDesc_.depth = desc.DepthOrArraySize;
		textureDesc_.mipLevels = desc.MipLevels;
		textureDesc_.sampleCount = 1;
		textureDesc_.format = desc.Format;
		switch (meta.dimension)
		{
		case DirectX::TEX_DIMENSION_TEXTURE1D: textureDesc_.dimension = TextureDimension::Texture1D; break;
		case DirectX::TEX_DIMENSION_TEXTURE2D: textureDesc_.dimension = TextureDimension::Texture2D; break;
		case DirectX::TEX_DIMENSION_TEXTURE3D: textureDesc_.dimension = TextureDimension::Texture3D; break;
		default: textureDesc_.dimension = TextureDimension::Max;
		}

		return true;
	}

	//----
	bool Texture::InitializeFromDDS(Device* pDev, CommandList* pCmdList, const void* pDdsBin, size_t size, sl12::u32 mipLevels, bool isForceSRGB, bool forceSysRam)
	{
		auto image = InitializeFromDDSwoLoad(pDev, pDdsBin, size, mipLevels);
		if (image.get() == nullptr)
		{
			return false;
		}

		// D3D12リソースを作成
		if (!InitializeFromDXImage(pDev, *image, isForceSRGB, forceSysRam))
		{
			return false;
		}

		// コピー命令発行
		ID3D12Resource* pSrcImage = nullptr;
		if (!UpdateImage(pDev, pCmdList, *image, &pSrcImage))
		{
			return false;
		}
		pDev->PendingKill(new ReleaseObjectItem<ID3D12Resource>(pSrcImage));

		return true;
	}

	//----
	std::unique_ptr<DirectX::ScratchImage> Texture::InitializeFromDDSwoLoad(Device* pDev, const void* pTgaBin, size_t size, sl12::u32 mipLevels)
	{
		if (!pDev)
		{
			return std::unique_ptr<DirectX::ScratchImage>();
		}
		if (!pTgaBin || !size)
		{
			return std::unique_ptr<DirectX::ScratchImage>();
		}

		// TGAファイルフォーマットからイメージリソースを作成
		std::unique_ptr<DirectX::ScratchImage> image(new DirectX::ScratchImage());
		DirectX::TexMetadata info;
		auto hr = DirectX::LoadFromDDSMemory(pTgaBin, size, DirectX::DDS_FLAGS_NONE, &info, *image);
		if (FAILED(hr))
		{
			return false;
		}

		// ミップマップ生成
		if (mipLevels != 1 && info.mipLevels == 1)
		{
			std::unique_ptr<DirectX::ScratchImage> mipped_image(new DirectX::ScratchImage());
			DirectX::GenerateMipMaps(*image->GetImage(0, 0, 0), DirectX::TEX_FILTER_CUBIC | DirectX::TEX_FILTER_FORCE_NON_WIC, 0, *mipped_image);
			image.swap(mipped_image);
		}

		return std::move(image);
	}

	//----
	bool Texture::InitializeFromTGA(Device* pDev, CommandList* pCmdList, const void* pTgaBin, size_t size, sl12::u32 mipLevels, bool isForceSRGB, bool forceSysRam)
	{
		auto image = InitializeFromTGAwoLoad(pDev, pTgaBin, size, mipLevels);
		if (image.get() == nullptr)
		{
			return false;
		}

		// D3D12リソースを作成
		if (!InitializeFromDXImage(pDev, *image, isForceSRGB, forceSysRam))
		{
			return false;
		}

		// コピー命令発行
		ID3D12Resource* pSrcImage = nullptr;
		if (!UpdateImage(pDev, pCmdList, *image, &pSrcImage))
		{
			return false;
		}
		pDev->PendingKill(new ReleaseObjectItem<ID3D12Resource>(pSrcImage));

		return true;
	}

	//----
	std::unique_ptr<DirectX::ScratchImage> Texture::InitializeFromTGAwoLoad(Device* pDev, const void* pTgaBin, size_t size, sl12::u32 mipLevels)
	{
		if (!pDev)
		{
			return std::unique_ptr<DirectX::ScratchImage>();
		}
		if (!pTgaBin || !size)
		{
			return std::unique_ptr<DirectX::ScratchImage>();
		}

		// TGAファイルフォーマットからイメージリソースを作成
		std::unique_ptr<DirectX::ScratchImage> image(new DirectX::ScratchImage());
		auto hr = DirectX::LoadFromTGAMemory(pTgaBin, size, nullptr, *image);
		if (FAILED(hr))
		{
			return false;
		}

		// ミップマップ生成
		if (mipLevels != 1)
		{
			std::unique_ptr<DirectX::ScratchImage> mipped_image(new DirectX::ScratchImage());
			DirectX::GenerateMipMaps(*image->GetImage(0, 0, 0), DirectX::TEX_FILTER_CUBIC | DirectX::TEX_FILTER_FORCE_NON_WIC, 0, *mipped_image);
			image.swap(mipped_image);
		}

		return std::move(image);
	}

	//----
	bool Texture::InitializeFromPNG(Device* pDev, CommandList* pCmdList, const void* pPngBin, size_t size, sl12::u32 mipLevels, bool isForceSRGB, bool forceSysRam)
	{
		auto image = InitializeFromPNGwoLoad(pDev, pPngBin, size, mipLevels);
		if (image.get() == nullptr)
		{
			return false;
		}

		// D3D12リソースを作成
		if (!InitializeFromDXImage(pDev, *image, isForceSRGB, forceSysRam))
		{
			return false;
		}

		// コピー命令発行
		ID3D12Resource* pSrcImage = nullptr;
		if (!UpdateImage(pDev, pCmdList, *image, &pSrcImage))
		{
			return false;
		}
		pDev->PendingKill(new ReleaseObjectItem<ID3D12Resource>(pSrcImage));

		return true;
	}

	//----
	std::unique_ptr<DirectX::ScratchImage> Texture::InitializeFromPNGwoLoad(Device* pDev, const void* pPngBin, size_t size, sl12::u32 mipLevels)
	{
		if (!pDev)
		{
			return std::unique_ptr<DirectX::ScratchImage>();
		}
		if (!pPngBin || !size)
		{
			return std::unique_ptr<DirectX::ScratchImage>();
		}

		// stbでpngを読み込む
		int width, height, bpp;
		auto pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(pPngBin), static_cast<int>(size), &width, &height, &bpp, 0);
		if (!pixels || (bpp != 3 && bpp != 4))
		{
			return false;
		}

		// DirectXTex形式のイメージへ変換
		std::unique_ptr<DirectX::ScratchImage> image(new DirectX::ScratchImage());
		auto hr = image->Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1);
		if (FAILED(hr))
		{
			return false;
		}
		if (bpp == 3)
		{
			auto src = pixels;
			auto dst = image->GetPixels();
			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < height; x++)
				{
					dst[0] = src[0];
					dst[1] = src[1];
					dst[2] = src[2];
					dst[3] = 0xff;
					src += 3;
					dst += 4;
				}
			}
		}
		else
		{
			auto src = pixels;
			auto dst = image->GetPixels();
			memcpy(dst, src, width * height * bpp);
		}

		stbi_image_free(pixels);

		// ミップマップ生成
		if (mipLevels != 1)
		{
			std::unique_ptr<DirectX::ScratchImage> mipped_image(new DirectX::ScratchImage());
			DirectX::GenerateMipMaps(*image->GetImage(0, 0, 0), DirectX::TEX_FILTER_CUBIC | DirectX::TEX_FILTER_FORCE_NON_WIC, 0, *mipped_image);
			image.swap(mipped_image);
		}

		return std::move(image);
	}

	//----
	bool Texture::InitializeFromEXR(Device* pDev, CommandList* pCmdList, const void* pPngBin, size_t size, sl12::u32 mipLevels, bool forceSysRam)
	{
		auto image = InitializeFromEXRwoLoad(pDev, pPngBin, size, mipLevels);
		if (image.get() == nullptr)
		{
			return false;
		}

		// D3D12リソースを作成
		if (!InitializeFromDXImage(pDev, *image, false, forceSysRam))
		{
			return false;
		}

		// コピー命令発行
		ID3D12Resource* pSrcImage = nullptr;
		if (!UpdateImage(pDev, pCmdList, *image, &pSrcImage))
		{
			return false;
		}
		pDev->PendingKill(new ReleaseObjectItem<ID3D12Resource>(pSrcImage));

		return true;
	}

	//----
	std::unique_ptr<DirectX::ScratchImage> Texture::InitializeFromEXRwoLoad(Device* pDev, const void* pExrBin, size_t size, sl12::u32 mipLevels)
	{
		if (!pDev)
		{
			return std::unique_ptr<DirectX::ScratchImage>();
		}
		if (!pExrBin || !size)
		{
			return std::unique_ptr<DirectX::ScratchImage>();
		}

		// tinyexrでexrを読み込む
		EXRVersion exr_version;
		auto ret = ParseEXRVersionFromMemory(&exr_version, (const u8*)pExrBin, size);
		if (ret != 0)
		{
			return std::unique_ptr<DirectX::ScratchImage>();
		}
		if (exr_version.multipart)
		{
			return std::unique_ptr<DirectX::ScratchImage>();
		}
		EXRHeader exr_header;
		InitEXRHeader(&exr_header);
		const char* err_code;
		ret = ParseEXRHeaderFromMemory(&exr_header, &exr_version, (const u8*)pExrBin, size, &err_code);
		if (ret != 0)
		{
			FreeEXRErrorMessage(err_code);
			FreeEXRHeader(&exr_header);
			return std::unique_ptr<DirectX::ScratchImage>();
		}
		if (exr_header.tiled)
		{
			FreeEXRHeader(&exr_header);
			return std::unique_ptr<DirectX::ScratchImage>();
		}
		EXRImage exr_image;
		InitEXRImage(&exr_image);
		ret = LoadEXRImageFromMemory(&exr_image, &exr_header, (const u8*)pExrBin, size, &err_code);
		if (ret != 0)
		{
			FreeEXRErrorMessage(err_code);
			FreeEXRImage(&exr_image);
			FreeEXRHeader(&exr_header);
			return std::unique_ptr<DirectX::ScratchImage>();
		}

		// DirectXTex形式のイメージへ変換
		std::unique_ptr<DirectX::ScratchImage> image(new DirectX::ScratchImage());
		if (exr_header.num_channels == 1)
		{
			auto pixel_size =
				exr_header.channels[0].pixel_type == TINYEXR_PIXELTYPE_FLOAT
				? sizeof(float)
				: sizeof(short);
			auto format =
				exr_header.channels[0].pixel_type == TINYEXR_PIXELTYPE_FLOAT
				? DXGI_FORMAT_R32_FLOAT
				: DXGI_FORMAT_R16_FLOAT;
			auto hr = image->Initialize2D(format, exr_image.width, exr_image.height, 1, 1);
			if (FAILED(hr))
			{
				FreeEXRImage(&exr_image);
				FreeEXRHeader(&exr_header);
				return false;
			}

			auto dst = image->GetPixels();
			memcpy(dst, &exr_image.images[0][0], exr_image.width * exr_image.height * pixel_size);
		}
		else
		{
			int idx_r = -1, idx_g = -1, idx_b = -1, idx_a = -1;
			int pixel_type = -1;
			for (int c = 0; c < exr_header.num_channels; c++)
			{
				if (strcmp(exr_header.channels[c].name, "R") == 0) idx_r = c;
				else if (strcmp(exr_header.channels[c].name, "G") == 0) idx_g = c;
				else if (strcmp(exr_header.channels[c].name, "B") == 0) idx_b = c;
				else if (strcmp(exr_header.channels[c].name, "A") == 0) idx_a = c;

				if (pixel_type < 0)
					pixel_type = exr_header.channels[c].pixel_type;
				else if (pixel_type != exr_header.channels[c].pixel_type)
				{
					FreeEXRImage(&exr_image);
					FreeEXRHeader(&exr_header);
					return false;
				}
			}

			auto pixel_size =
				pixel_type == TINYEXR_PIXELTYPE_FLOAT
				? sizeof(float)
				: sizeof(short);
			auto format =
				pixel_type == TINYEXR_PIXELTYPE_FLOAT
				? DXGI_FORMAT_R32G32B32A32_FLOAT
				: DXGI_FORMAT_R16G16B16A16_FLOAT;
			auto hr = image->Initialize2D(format, exr_image.width, exr_image.height, 1, 1);
			if (FAILED(hr))
			{
				FreeEXRImage(&exr_image);
				FreeEXRHeader(&exr_header);
				return false;
			}

			if (pixel_type == TINYEXR_PIXELTYPE_FLOAT)
			{
				auto dst = (float*)image->GetPixels();
				for (int i = 0; i < exr_image.width * exr_image.height; i++)
				{
					dst[i * 4 + 0] = reinterpret_cast<float**>(exr_image.images)[idx_r][i];
					dst[i * 4 + 1] = reinterpret_cast<float**>(exr_image.images)[idx_g][i];
					dst[i * 4 + 2] = reinterpret_cast<float**>(exr_image.images)[idx_b][i];
					if (idx_a >= 0)
						dst[i * 4 + 3] = reinterpret_cast<float**>(exr_image.images)[idx_a][i];
					else
						dst[i * 4 + 3] = 1.0f;
				}
			}
			else
			{
				auto dst = (short*)image->GetPixels();
				for (int i = 0; i < exr_image.width * exr_image.height; i++)
				{
					dst[i * 4 + 0] = reinterpret_cast<short**>(exr_image.images)[idx_r][i];
					dst[i * 4 + 1] = reinterpret_cast<short**>(exr_image.images)[idx_g][i];
					dst[i * 4 + 2] = reinterpret_cast<short**>(exr_image.images)[idx_b][i];
					if (idx_a >= 0)
						dst[i * 4 + 3] = reinterpret_cast<short**>(exr_image.images)[idx_a][i];
					else
						dst[i * 4 + 3] = 0x3c00;
				}
			}
		}

		FreeEXRImage(&exr_image);
		FreeEXRHeader(&exr_header);

		// ミップマップ生成
		if (mipLevels != 1)
		{
			std::unique_ptr<DirectX::ScratchImage> mipped_image(new DirectX::ScratchImage());
			DirectX::GenerateMipMaps(*image->GetImage(0, 0, 0), DirectX::TEX_FILTER_CUBIC | DirectX::TEX_FILTER_FORCE_NON_WIC, 0, *mipped_image);
			image.swap(mipped_image);
		}

		return std::move(image);
	}

	//----
	bool Texture::InitializeFromHDR(Device* pDev, CommandList* pCmdList, const void* pPngBin, size_t size, sl12::u32 mipLevels, bool forceSysRam)
	{
		auto image = InitializeFromHDRwoLoad(pDev, pPngBin, size, mipLevels);
		if (image.get() == nullptr)
		{
			return false;
		}

		// D3D12リソースを作成
		if (!InitializeFromDXImage(pDev, *image, false, forceSysRam))
		{
			return false;
		}

		// コピー命令発行
		ID3D12Resource* pSrcImage = nullptr;
		if (!UpdateImage(pDev, pCmdList, *image, &pSrcImage))
		{
			return false;
		}
		pDev->PendingKill(new ReleaseObjectItem<ID3D12Resource>(pSrcImage));

		return true;
	}

	//----
	std::unique_ptr<DirectX::ScratchImage> Texture::InitializeFromHDRwoLoad(Device* pDev, const void* pTgaBin, size_t size, sl12::u32 mipLevels)
	{
		if (!pDev)
		{
			return std::unique_ptr<DirectX::ScratchImage>();
		}
		if (!pTgaBin || !size)
		{
			return std::unique_ptr<DirectX::ScratchImage>();
		}

		// TGAファイルフォーマットからイメージリソースを作成
		std::unique_ptr<DirectX::ScratchImage> image(new DirectX::ScratchImage());
		auto hr = DirectX::LoadFromHDRMemory(pTgaBin, size, nullptr, *image);
		if (FAILED(hr))
		{
			return false;
		}

		// ミップマップ生成
		if (mipLevels != 1)
		{
			std::unique_ptr<DirectX::ScratchImage> mipped_image(new DirectX::ScratchImage());
			DirectX::GenerateMipMaps(*image->GetImage(0, 0, 0), DirectX::TEX_FILTER_CUBIC | DirectX::TEX_FILTER_FORCE_NON_WIC, 0, *mipped_image);
			image.swap(mipped_image);
		}

		return std::move(image);
	}

	//----
	bool Texture::InitializeFromImageBin(Device* pDev, CommandList* pCmdList, const TextureDesc& desc, const void* pImageBin)
	{
		if (!pDev)
		{
			return false;
		}
		if (!pImageBin)
		{
			return false;
		}

		// D3D12リソースを作成
		if (!Initialize(pDev, desc))
		{
			return false;
		}

		// コピー命令発行
		ID3D12Resource* pSrcImage = nullptr;
		if (!UpdateImage(pDev, pCmdList, pImageBin, &pSrcImage))
		{
			return false;
		}
		pDev->PendingKill(new ReleaseObjectItem<ID3D12Resource>(pSrcImage));

		return true;
	}

	//----
	bool Texture::InitializeFromSwapchain(Device* pDev, Swapchain* pSwapchain, int bufferIndex)
	{
		if (!pDev)
		{
			return false;
		}
		if (!pSwapchain)
		{
			return false;
		}

		auto hr = pSwapchain->GetSwapchain()->GetBuffer(bufferIndex, IID_PPV_ARGS(&pResource_));
		if (FAILED(hr))
		{
			return false;
		}

		resourceDesc_ = pResource_->GetDesc();

		memset(&textureDesc_, 0, sizeof(textureDesc_));
		textureDesc_.dimension = TextureDimension::Texture2D;
		textureDesc_.width = static_cast<u32>(resourceDesc_.Width);
		textureDesc_.height = resourceDesc_.Height;
		textureDesc_.depth = resourceDesc_.DepthOrArraySize;
		textureDesc_.mipLevels = resourceDesc_.MipLevels;
		textureDesc_.format = resourceDesc_.Format;
		textureDesc_.sampleCount = resourceDesc_.SampleDesc.Count;
		textureDesc_.isRenderTarget = true;

		currentState_ = D3D12_RESOURCE_STATE_PRESENT;

		return true;
	}

	//----
	bool Texture::UpdateImage(Device* pDev, CommandList* pCmdList, const DirectX::ScratchImage& image, ID3D12Resource** ppSrcImage)
	{
		const DirectX::TexMetadata& meta = image.GetMetadata();

		// リソースのサイズ等を取得
		u32 numSubresources = static_cast<u32>(meta.arraySize * meta.mipLevels);
		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprint(numSubresources);
		std::vector<u32> numRows(numSubresources);
		std::vector<u64> rowSize(numSubresources);
		u64 totalSize;
		pDev->GetDeviceDep()->GetCopyableFootprints(&resourceDesc_, 0, numSubresources, 0, footprint.data(), numRows.data(), rowSize.data(), &totalSize);

		// アップロード用のオブジェクトを作成
		D3D12_HEAP_PROPERTIES heapProp = {};
		heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProp.CreationNodeMask = 1;
		heapProp.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment = 0;
		desc.Width = totalSize;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		ID3D12Resource* pSrcImage{ nullptr };
		auto hr = pDev->GetDeviceDep()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&pSrcImage));
		if (FAILED(hr))
		{
			return false;
		}

		// リソースをマップしてイメージ情報をコピー
		u8* pData{ nullptr };
		hr = pSrcImage->Map(0, NULL, reinterpret_cast<void**>(&pData));
		if (FAILED(hr))
		{
			SafeRelease(pSrcImage);
			return false;
		}

		for (u32 d = 0; d < meta.arraySize; d++)
		{
			for (u32 m = 0; m < meta.mipLevels; m++)
			{
				size_t i = d * meta.mipLevels + m;
				if (rowSize[i] >(SIZE_T) -1)
				{
					SafeRelease(pSrcImage);
					return false;
				}
				D3D12_MEMCPY_DEST dstData = { pData + footprint[i].Offset, footprint[i].Footprint.RowPitch, footprint[i].Footprint.RowPitch * numRows[i] };
				const DirectX::Image* pImage = image.GetImage(m, 0, d);
				if (rowSize[i] == footprint[i].Footprint.RowPitch)
				{
					memcpy(dstData.pData, pImage->pixels, pImage->rowPitch * numRows[i]);
				}
				else if (rowSize[i] < footprint[i].Footprint.RowPitch)
				{
					u8* p_src = pImage->pixels;
					u8* p_dst = reinterpret_cast<u8*>(dstData.pData);
					for (u32 r = 0; r < numRows[i]; r++, p_src += pImage->rowPitch, p_dst += footprint[i].Footprint.RowPitch)
					{
						memcpy(p_dst, p_src, rowSize[i]);
					}
				}
			}
		}

		pSrcImage->Unmap(0, nullptr);

		// コピー命令を発行
		for (u32 i = 0; i < numSubresources; i++)
		{
			D3D12_TEXTURE_COPY_LOCATION src, dst;
			src.pResource = pSrcImage;
			src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			src.PlacedFootprint = footprint[i];
			dst.pResource = pResource_;
			dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dst.SubresourceIndex = i;
			pCmdList->GetCommandList()->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
		}

		*ppSrcImage = pSrcImage;

		return true;
	}

	//----
	bool Texture::UpdateImage(Device* pDev, CommandList* pCmdList, const void* pImageBin, ID3D12Resource** ppSrcImage)
	{
		// リソースのサイズ等を取得
		u32 numSubresources = resourceDesc_.DepthOrArraySize * resourceDesc_.MipLevels;
		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprint(numSubresources);
		std::vector<u32> numRows(numSubresources);
		std::vector<u64> rowSize(numSubresources);
		u64 totalSize;
		pDev->GetDeviceDep()->GetCopyableFootprints(&resourceDesc_, 0, numSubresources, 0, footprint.data(), numRows.data(), rowSize.data(), &totalSize);

		// アップロード用のオブジェクトを作成
		D3D12_HEAP_PROPERTIES heapProp = {};
		heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProp.CreationNodeMask = 1;
		heapProp.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment = 0;
		desc.Width = totalSize;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		ID3D12Resource* pSrcImage{ nullptr };
		auto hr = pDev->GetDeviceDep()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&pSrcImage));
		if (FAILED(hr))
		{
			return false;
		}

		// リソースをマップしてイメージ情報をコピー
		u8* pData{ nullptr };
		hr = pSrcImage->Map(0, NULL, reinterpret_cast<void**>(&pData));
		if (FAILED(hr))
		{
			SafeRelease(pSrcImage);
			return false;
		}

		{
			if (rowSize[0] >(SIZE_T) -1)
			{
				SafeRelease(pSrcImage);
				return false;
			}
			D3D12_MEMCPY_DEST dstData = { pData + footprint[0].Offset, footprint[0].Footprint.RowPitch, footprint[0].Footprint.RowPitch * numRows[0] };
			memcpy(dstData.pData, pImageBin, footprint[0].Footprint.RowPitch * footprint[0].Footprint.Height);
		}

		pSrcImage->Unmap(0, nullptr);

		// コピー命令を発行
		for (u32 i = 0; i < numSubresources; i++)
		{
			D3D12_TEXTURE_COPY_LOCATION src, dst;
			src.pResource = pSrcImage;
			src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			src.PlacedFootprint = footprint[i];
			dst.pResource = pResource_;
			dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dst.SubresourceIndex = i;
			pCmdList->GetCommandList()->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
		}

		*ppSrcImage = pSrcImage;

		return true;
	}

	//----
	void Texture::Destroy()
	{
		SafeRelease(pResource_);
	}

}	// namespace sl12

//	EOF
