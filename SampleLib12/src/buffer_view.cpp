#include <sl12/buffer_view.h>

#include <sl12/device.h>
#include <sl12/descriptor.h>
#include <sl12/descriptor_heap.h>
#include <sl12/buffer.h>


namespace sl12
{
	//----
	bool ConstantBufferView::Initialize(Device* pDev, Buffer* pBuffer)
	{
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

		descInfo_ = pDev->GetViewDescriptorHeap().Allocate();
		if (!descInfo_.IsValid())
		{
			return false;
		}

		D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc{};
		viewDesc.BufferLocation = pBuffer->GetResourceDep()->GetGPUVirtualAddress();
		viewDesc.SizeInBytes = static_cast<u32>(pBuffer->GetResourceDesc().Width);
		pDev->GetDeviceDep()->CreateConstantBufferView(&viewDesc, descInfo_.cpuHandle);

		return true;
	}

	//----
	void ConstantBufferView::Destroy()
	{
		descInfo_.Free();
	}


	//----
	bool VertexBufferView::Initialize(Device* pDev, Buffer* pBuffer)
	{
		if (!pBuffer)
		{
			return false;
		}
		if (pBuffer->GetBufferUsage() != BufferUsage::VertexBuffer)
		{
			return false;
		}

		view_.BufferLocation = pBuffer->GetResourceDep()->GetGPUVirtualAddress();
		view_.SizeInBytes = static_cast<u32>(pBuffer->GetSize());
		view_.StrideInBytes = static_cast<u32>(pBuffer->GetStride());

		return true;
	}

	//----
	void VertexBufferView::Destroy()
	{}


	//----
	bool IndexBufferView::Initialize(Device* pDev, Buffer* pBuffer)
	{
		if (!pBuffer)
		{
			return false;
		}
		if (pBuffer->GetBufferUsage() != BufferUsage::IndexBuffer)
		{
			return false;
		}

		view_.BufferLocation = pBuffer->GetResourceDep()->GetGPUVirtualAddress();
		view_.SizeInBytes = static_cast<u32>(pBuffer->GetSize());
		view_.Format = (pBuffer->GetStride() == 4) ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;

		return true;
	}

	//----
	void IndexBufferView::Destroy()
	{}


	//----
	bool BufferView::Initialize(Device* pDev, Buffer* pBuffer, u32 firstElement, u32 stride)
	{
		const D3D12_RESOURCE_DESC& resDesc = pBuffer->GetResourceDesc();
		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		if (stride == 0)
		{
			viewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			viewDesc.Buffer.FirstElement = firstElement;
			viewDesc.Buffer.NumElements = static_cast<u32>(resDesc.Width / 4);
			viewDesc.Buffer.StructureByteStride = 0;
			viewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
		}
		else
		{
			viewDesc.Format = DXGI_FORMAT_UNKNOWN;
			viewDesc.Buffer.FirstElement = firstElement;
			viewDesc.Buffer.NumElements = static_cast<u32>((resDesc.Width / static_cast<u64>(stride)) - static_cast<u64>(firstElement));
			viewDesc.Buffer.StructureByteStride = stride;
			viewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		}

		descInfo_ = pDev->GetViewDescriptorHeap().Allocate();
		if (!descInfo_.IsValid())
		{
			return false;
		}

		pDev->GetDeviceDep()->CreateShaderResourceView(pBuffer->GetResourceDep(), &viewDesc, descInfo_.cpuHandle);

		return true;
	}

	//----
	void BufferView::Destroy()
	{
		descInfo_.Free();
	}

}	// namespace sl12

//	EOF
