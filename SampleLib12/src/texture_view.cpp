#include <sl12/texture_view.h>

#include <sl12/device.h>
#include <sl12/texture.h>
#include <sl12/buffer.h>
#include <sl12/descriptor.h>
#include <sl12/descriptor_heap.h>


namespace sl12
{
	//----
	bool TextureView::Initialize(Device* pDev, Texture* pTex)
	{
		const D3D12_RESOURCE_DESC& resDesc = pTex->GetResourceDesc();
		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		viewDesc.Format = pTex->GetTextureDesc().format;
		switch (viewDesc.Format)
		{
		case DXGI_FORMAT_D32_FLOAT:
			viewDesc.Format = DXGI_FORMAT_R32_FLOAT; break;
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			viewDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS; break;
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
			viewDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; break;
		case DXGI_FORMAT_D16_UNORM:
			viewDesc.Format = DXGI_FORMAT_R16_UNORM; break;
		}
		if (resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
		{
			if (resDesc.DepthOrArraySize == 1)
			{
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
				viewDesc.Texture1D.MipLevels = resDesc.MipLevels;
				viewDesc.Texture1D.MostDetailedMip = 0;
				viewDesc.Texture1D.ResourceMinLODClamp = 0.0f;
			}
			else
			{
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
				viewDesc.Texture1DArray.MipLevels = resDesc.MipLevels;
				viewDesc.Texture1DArray.MostDetailedMip = 0;
				viewDesc.Texture1DArray.ResourceMinLODClamp = 0.0f;
				viewDesc.Texture1DArray.FirstArraySlice = 0;
				viewDesc.Texture1DArray.ArraySize = resDesc.DepthOrArraySize;
			}
		}
		else if (resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
		{
			if (resDesc.SampleDesc.Count == 1)
			{
				if (resDesc.DepthOrArraySize == 1)
				{
					viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					viewDesc.Texture2D.MipLevels = resDesc.MipLevels;
					viewDesc.Texture2D.MostDetailedMip = 0;
					viewDesc.Texture2D.PlaneSlice = 0;
					viewDesc.Texture2D.ResourceMinLODClamp = 0.0f;
				}
				else
				{
					viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
					viewDesc.Texture2DArray.MipLevels = resDesc.MipLevels;
					viewDesc.Texture2DArray.MostDetailedMip = 0;
					viewDesc.Texture2DArray.PlaneSlice = 0;
					viewDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
					viewDesc.Texture2DArray.FirstArraySlice = 0;
					viewDesc.Texture2DArray.ArraySize = resDesc.DepthOrArraySize;
				}
			}
			else
			{
				if (resDesc.DepthOrArraySize == 1)
				{
					viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
				}
				else
				{
					viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
					viewDesc.Texture2DMSArray.FirstArraySlice = 0;
					viewDesc.Texture2DMSArray.ArraySize = resDesc.DepthOrArraySize;
				}
			}
		}
		else
		{
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			viewDesc.Texture3D.MipLevels = resDesc.MipLevels;
			viewDesc.Texture3D.MostDetailedMip = 0;
			viewDesc.Texture3D.ResourceMinLODClamp = 0.0f;
		}

		pDesc_ = pDev->GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).CreateDescriptor();
		if (!pDesc_)
		{
			return false;
		}

		pDev->GetDeviceDep()->CreateShaderResourceView(pTex->GetResourceDep(), &viewDesc, pDesc_->GetCpuHandle());

		return true;
	}

	//----
	void TextureView::Destroy()
	{
		SafeRelease(pDesc_);
	}


	//----
	bool RenderTargetView::Initialize(Device* pDev, Texture* pTex, u32 mipSlice, u32 firstArray, u32 arraySize)
	{
		const D3D12_RESOURCE_DESC& resDesc = pTex->GetResourceDesc();
		D3D12_RENDER_TARGET_VIEW_DESC viewDesc{};
		viewDesc.Format = resDesc.Format;
		if (resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
		{
			if (resDesc.DepthOrArraySize == 1)
			{
				viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
				viewDesc.Texture1D.MipSlice = mipSlice;
			}
			else
			{
				viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
				viewDesc.Texture1DArray.MipSlice = mipSlice;
				viewDesc.Texture1DArray.FirstArraySlice = firstArray;
				viewDesc.Texture1DArray.ArraySize = arraySize;
			}
		}
		else if (resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
		{
			if (resDesc.SampleDesc.Count == 1)
			{
				if (resDesc.DepthOrArraySize == 1)
				{
					viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
					viewDesc.Texture2D.MipSlice = mipSlice;
					viewDesc.Texture2D.PlaneSlice = 0;
				}
				else
				{
					viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
					viewDesc.Texture2DArray.MipSlice = mipSlice;
					viewDesc.Texture2DArray.PlaneSlice = 0;
					viewDesc.Texture2DArray.FirstArraySlice = firstArray;
					viewDesc.Texture2DArray.ArraySize = arraySize;
				}
			}
			else
			{
				if (resDesc.DepthOrArraySize == 1)
				{
					viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
				}
				else
				{
					viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
					viewDesc.Texture2DMSArray.FirstArraySlice = firstArray;
					viewDesc.Texture2DMSArray.ArraySize = arraySize;
				}
			}
		}
		else if (resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
		{
			viewDesc.Texture3D.MipSlice = mipSlice;
			viewDesc.Texture3D.FirstWSlice = firstArray;
			viewDesc.Texture3D.WSize = arraySize;
		}
		else
		{
			return false;
		}

		pDesc_ = pDev->GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV).CreateDescriptor();
		if (!pDesc_)
		{
			return false;
		}

		pDev->GetDeviceDep()->CreateRenderTargetView(pTex->GetResourceDep(), &viewDesc, pDesc_->GetCpuHandle());

		return true;
	}

	//----
	void RenderTargetView::Destroy()
	{
		SafeRelease(pDesc_);
	}


	//----
	bool DepthStencilView::Initialize(Device* pDev, Texture* pTex, u32 mipSlice, u32 firstArray, u32 arraySize)
	{
		const D3D12_RESOURCE_DESC& resDesc = pTex->GetResourceDesc();
		D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc{};
		viewDesc.Format = pTex->GetTextureDesc().format;
		if (resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
		{
			if (resDesc.DepthOrArraySize == 1)
			{
				viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
				viewDesc.Texture1D.MipSlice = mipSlice;
			}
			else
			{
				viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
				viewDesc.Texture1DArray.MipSlice = mipSlice;
				viewDesc.Texture1DArray.FirstArraySlice = firstArray;
				viewDesc.Texture1DArray.ArraySize = arraySize;
			}
		}
		else if (resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
		{
			if (resDesc.SampleDesc.Count == 1)
			{
				if (resDesc.DepthOrArraySize == 1)
				{
					viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
					viewDesc.Texture2D.MipSlice = mipSlice;
				}
				else
				{
					viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
					viewDesc.Texture2DArray.MipSlice = mipSlice;
					viewDesc.Texture2DArray.FirstArraySlice = firstArray;
					viewDesc.Texture2DArray.ArraySize = arraySize;
				}
			}
			else
			{
				if (resDesc.DepthOrArraySize == 1)
				{
					viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
				}
				else
				{
					viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
					viewDesc.Texture2DMSArray.FirstArraySlice = firstArray;
					viewDesc.Texture2DMSArray.ArraySize = arraySize;
				}
			}
		}
		else
		{
			return false;
		}

		pDesc_ = pDev->GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV).CreateDescriptor();
		if (!pDesc_)
		{
			return false;
		}

		pDev->GetDeviceDep()->CreateDepthStencilView(pTex->GetResourceDep(), &viewDesc, pDesc_->GetCpuHandle());

		return true;
	}

	//----
	void DepthStencilView::Destroy()
	{
		SafeRelease(pDesc_);
	}


	//----
	bool UnorderedAccessView::Initialize(Device* pDev, Texture* pTex, u32 mipSlice, u32 firstArray, u32 arraySize)
	{
		const D3D12_RESOURCE_DESC& resDesc = pTex->GetResourceDesc();
		D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};
		viewDesc.Format = resDesc.Format;
		if (resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
		{
			if (resDesc.DepthOrArraySize == 1)
			{
				viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
				viewDesc.Texture1D.MipSlice = mipSlice;
			}
			else
			{
				viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
				viewDesc.Texture1DArray.MipSlice = mipSlice;
				viewDesc.Texture1DArray.FirstArraySlice = firstArray;
				viewDesc.Texture1DArray.ArraySize = arraySize;
			}
		}
		else if (resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
		{
			if (resDesc.SampleDesc.Count == 1)
			{
				if (resDesc.DepthOrArraySize == 1)
				{
					viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
					viewDesc.Texture2D.MipSlice = mipSlice;
					viewDesc.Texture2D.PlaneSlice = 0;
				}
				else
				{
					viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
					viewDesc.Texture2DArray.MipSlice = mipSlice;
					viewDesc.Texture2DArray.FirstArraySlice = firstArray;
					viewDesc.Texture2DArray.ArraySize = arraySize;
				}
			}
			else
			{
				return false;
			}
		}
		else if (resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
		{
			viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
			viewDesc.Texture3D.MipSlice = mipSlice;
			viewDesc.Texture3D.FirstWSlice = firstArray;
			viewDesc.Texture3D.WSize = arraySize;
		}
		else
		{
			return false;
		}

		pDesc_ = pDev->GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).CreateDescriptor();
		if (!pDesc_)
		{
			return false;
		}

		pDev->GetDeviceDep()->CreateUnorderedAccessView(pTex->GetResourceDep(), nullptr, &viewDesc, pDesc_->GetCpuHandle());

		return true;
	}

	//----
	bool UnorderedAccessView::Initialize(Device* pDev, Buffer* pBuff, u32 firstElement, u32 stride, u64 offset)
	{
		const D3D12_RESOURCE_DESC& resDesc = pBuff->GetResourceDesc();
		D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};
		viewDesc.Format = resDesc.Format;
		viewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		if (stride == 0)
		{
			viewDesc.Buffer.FirstElement = firstElement;
			viewDesc.Buffer.NumElements = 0;
			viewDesc.Buffer.StructureByteStride = stride;
			viewDesc.Buffer.CounterOffsetInBytes = offset;
			viewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
		}
		else
		{
			viewDesc.Buffer.FirstElement = firstElement;
			viewDesc.Buffer.NumElements = static_cast<u32>((resDesc.Width / static_cast<u64>(stride)) - static_cast<u64>(firstElement));
			viewDesc.Buffer.StructureByteStride = stride;
			viewDesc.Buffer.CounterOffsetInBytes = offset;
			viewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		}

		pDesc_ = pDev->GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).CreateDescriptor();
		if (!pDesc_)
		{
			return false;
		}

		pDev->GetDeviceDep()->CreateUnorderedAccessView(pBuff->GetResourceDep(), nullptr, &viewDesc, pDesc_->GetCpuHandle());

		return true;
	}

	//----
	void UnorderedAccessView::Destroy()
	{
		SafeRelease(pDesc_);
	}

}	// namespace sl12

//	EOF
