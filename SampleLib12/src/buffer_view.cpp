#include <sl12/buffer_view.h>

#include <sl12/device.h>
#include <sl12/descriptor.h>
#include <sl12/descriptor_heap.h>
#include <sl12/buffer.h>


namespace sl12
{
	//----
	bool ConstantBufferView::Initialize(Device* pDev, Buffer* pBuffer, size_t offset, size_t size)
	{
		auto AlignOk = [](size_t s)
		{
			size_t a = s - (s / D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT * D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			return a == 0;
		};

		if (!pDev)
		{
			return false;
		}
		if (!pBuffer)
		{
			return false;
		}
		if (pBuffer->GetBufferUsage() != BufferUsage::ConstantBuffer)
		{
			return false;
		}
		if (!AlignOk(offset) || !AlignOk(size))
		{
			return false;
		}
		if ((offset + size) > pBuffer->GetResourceDesc().Width)
		{
			return false;
		}

		descInfo_ = pDev->GetViewDescriptorHeap().Allocate();
		if (!descInfo_.IsValid())
		{
			return false;
		}

		D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc{};
		viewDesc.BufferLocation = pBuffer->GetResourceDep()->GetGPUVirtualAddress() + offset;
		viewDesc.SizeInBytes = static_cast<u32>(size > 0 ? size : (pBuffer->GetResourceDesc().Width - offset));
		pDev->GetDeviceDep()->CreateConstantBufferView(&viewDesc, descInfo_.cpuHandle);

		return true;
	}

	//----
	void ConstantBufferView::Destroy()
	{
		descInfo_.Free();
	}


	//----
	bool VertexBufferView::Initialize(Device* pDev, Buffer* pBuffer, size_t offset, size_t size)
	{
		if (!pBuffer)
		{
			return false;
		}
		if (pBuffer->GetBufferUsage() != BufferUsage::VertexBuffer)
		{
			return false;
		}

		view_.BufferLocation = pBuffer->GetResourceDep()->GetGPUVirtualAddress() + offset;
		view_.SizeInBytes = (size == 0) ? static_cast<u32>(pBuffer->GetSize() - offset) : static_cast<u32>(size);
		view_.StrideInBytes = static_cast<u32>(pBuffer->GetStride());
		bufferOffset_ = offset;

		return true;
	}
	bool VertexBufferView::Initialize(Device* pDev, Buffer* pBuffer, size_t offset, size_t size, size_t stride)
	{
		if (!pBuffer)
		{
			return false;
		}
		if (pBuffer->GetBufferUsage() != BufferUsage::VertexBuffer)
		{
			return false;
		}

		view_.BufferLocation = pBuffer->GetResourceDep()->GetGPUVirtualAddress() + offset;
		view_.SizeInBytes = (size == 0) ? static_cast<u32>(pBuffer->GetSize() - offset) : static_cast<u32>(size);
		view_.StrideInBytes = static_cast<u32>(stride);
		bufferOffset_ = offset;

		return true;
	}

	//----
	void VertexBufferView::Destroy()
	{}


	//----
	bool IndexBufferView::Initialize(Device* pDev, Buffer* pBuffer, size_t offset, size_t size)
	{
		if (!pBuffer)
		{
			return false;
		}
		if (pBuffer->GetBufferUsage() != BufferUsage::IndexBuffer)
		{
			return false;
		}
		if (pBuffer->GetStride() != 4 && pBuffer->GetStride() != 2)
		{
			return false;
		}

		view_.BufferLocation = pBuffer->GetResourceDep()->GetGPUVirtualAddress() + offset;
		view_.SizeInBytes = (size == 0) ? static_cast<u32>(pBuffer->GetSize() - offset) : static_cast<u32>(size);
		view_.Format = (pBuffer->GetStride() == 4) ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
		bufferOffset_ = offset;

		return true;
	}
	bool IndexBufferView::Initialize(Device* pDev, Buffer* pBuffer, size_t offset, size_t size, size_t stride)
	{
		if (!pBuffer)
		{
			return false;
		}
		if (pBuffer->GetBufferUsage() != BufferUsage::IndexBuffer)
		{
			return false;
		}
		if (stride != 4 && stride != 2)
		{
			return false;
		}

		view_.BufferLocation = pBuffer->GetResourceDep()->GetGPUVirtualAddress() + offset;
		view_.SizeInBytes = (size == 0) ? static_cast<u32>(pBuffer->GetSize() - offset) : static_cast<u32>(size);
		view_.Format = (stride == 4) ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
		bufferOffset_ = offset;

		return true;
	}

	//----
	void IndexBufferView::Destroy()
	{}


	//----
	bool BufferView::Initialize(Device* pDev, Buffer* pBuffer, u32 firstElement, u32 numElement, u32 stride)
	{
		const D3D12_RESOURCE_DESC& resDesc = pBuffer->GetResourceDesc();
		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		if (stride == 0)
		{
			viewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			viewDesc.Buffer.FirstElement = firstElement;
			viewDesc.Buffer.NumElements = (numElement == 0) ? (static_cast<u32>(resDesc.Width / 4) - firstElement) : numElement;
			viewDesc.Buffer.StructureByteStride = 0;
			viewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
		}
		else
		{
			viewDesc.Format = DXGI_FORMAT_UNKNOWN;
			viewDesc.Buffer.FirstElement = firstElement;
			viewDesc.Buffer.NumElements = (numElement == 0) ? (static_cast<u32>((resDesc.Width / static_cast<u64>(stride)) - static_cast<u64>(firstElement))) : numElement;
			viewDesc.Buffer.StructureByteStride = stride;
			viewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		}

		descInfo_ = pDev->GetViewDescriptorHeap().Allocate();
		if (!descInfo_.IsValid())
		{
			return false;
		}

		pDev->GetDeviceDep()->CreateShaderResourceView(pBuffer->GetResourceDep(), &viewDesc, descInfo_.cpuHandle);

		viewDesc_ = viewDesc;

		return true;
	}

	//----
	void BufferView::Destroy()
	{
		descInfo_.Free();
	}

}	// namespace sl12

//	EOF
