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

		pDesc_ = pDev->GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).CreateDescriptor();
		if (!pDesc_)
		{
			return false;
		}

		D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc{};
		viewDesc.BufferLocation = pBuffer->GetResource()->GetGPUVirtualAddress();
		viewDesc.SizeInBytes = static_cast<u32>(pBuffer->GetResourceDesc().Width);
		pDev->GetDeviceDep()->CreateConstantBufferView(&viewDesc, pDesc_->GetCpuHandle());

		return true;
	}

	//----
	void ConstantBufferView::Destroy()
	{
		SafeRelease(pDesc_);
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

		view_.BufferLocation = pBuffer->GetResource()->GetGPUVirtualAddress();
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

		view_.BufferLocation = pBuffer->GetResource()->GetGPUVirtualAddress();
		view_.SizeInBytes = static_cast<u32>(pBuffer->GetSize());
		view_.Format = (pBuffer->GetStride() == 4) ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;

		return true;
	}

	//----
	void IndexBufferView::Destroy()
	{}

}	// namespace sl12

//	EOF
