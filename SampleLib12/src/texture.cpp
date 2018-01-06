#include <sl12/texture.h>

#include <sl12/device.h>
#include <sl12/command_list.h>
#include <sl12/fence.h>
#include <sl12/swapchain.h>


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
	bool Texture::InitializeFromDXImage(Device* pDev, const DirectX::ScratchImage& image, bool isForceSRGB)
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
	bool Texture::InitializeFromTGA(Device* pDev, CommandList* pCmdList, const void* pTgaBin, size_t size, bool isForceSRGB)
	{
		if (!pDev)
		{
			return false;
		}
		if (!pTgaBin || !size)
		{
			return false;
		}

		// TGAファイルフォーマットからイメージリソースを作成
		DirectX::ScratchImage image;
		auto hr = DirectX::LoadFromTGAMemory(pTgaBin, size, nullptr, image);
		if (FAILED(hr))
		{
			return false;
		}

		// D3D12リソースを作成
		if (!InitializeFromDXImage(pDev, image, isForceSRGB))
		{
			return false;
		}

		// コピー命令発行
		ID3D12Resource* pSrcImage = nullptr;
		pCmdList->Reset();
		if (!UpdateImage(pDev, pCmdList, image, &pSrcImage))
		{
			return false;
		}
		pCmdList->Close();
		pCmdList->Execute();

		Fence fence;
		fence.Initialize(pDev);
		fence.Signal(pCmdList->GetParentQueue());
		fence.WaitSignal();

		fence.Destroy();
		SafeRelease(pSrcImage);

		return true;
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
		pCmdList->Reset();
		if (!UpdateImage(pDev, pCmdList, pImageBin, &pSrcImage))
		{
			return false;
		}
		pCmdList->Close();
		pCmdList->Execute();

		Fence fence;
		fence.Initialize(pDev);
		fence.Signal(pCmdList->GetParentQueue());
		fence.WaitSignal();

		fence.Destroy();
		SafeRelease(pSrcImage);

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
				memcpy(dstData.pData, pImage->pixels, pImage->rowPitch * pImage->height);
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
