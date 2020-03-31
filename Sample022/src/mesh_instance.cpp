#include "mesh_instance.h"


//----
bool MeshletRenderComponent::Initialize(sl12::Device* pDev, const std::vector<sl12::ResourceItemMesh::Meshlet>& meshlets)
{
	struct MeshletData
	{
		DirectX::XMFLOAT3	aabbMin;
		sl12::u32			indexOffset;
		DirectX::XMFLOAT3	aabbMax;
		sl12::u32			indexCount;
	};

	if (!meshletB_.Initialize(pDev, sizeof(MeshletData) * meshlets.size(), sizeof(MeshletData), sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
	{
		return false;
	}
	if (!meshletBV_.Initialize(pDev, &meshletB_, 0, (sl12::u32)meshlets.size(), sizeof(MeshletData)))
	{
		return false;
	}

	if (!indirectArgumentB_.Initialize(pDev, sizeof(D3D12_DRAW_INDEXED_ARGUMENTS) * meshlets.size(), sizeof(D3D12_DRAW_INDEXED_ARGUMENTS), sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, false, true))
	{
		return false;
	}
	if (!indirectArgumentUAV_.Initialize(pDev, &indirectArgumentB_, 0, (sl12::u32)meshlets.size(), sizeof(D3D12_DRAW_INDEXED_ARGUMENTS), 0))
	{
		return false;
	}

	{
		MeshletData* data = (MeshletData*)meshletB_.Map(nullptr);
		for (auto&& m : meshlets)
		{
			data->aabbMin = m.boundingInfo.box.aabbMin;
			data->aabbMax = m.boundingInfo.box.aabbMax;
			data->indexOffset = m.indexOffset;
			data->indexCount = m.indexCount;
			data++;
		}
		meshletB_.Unmap();
	}

	return true;
}

//----
void MeshletRenderComponent::Destroy()
{
	meshletBV_.Destroy();
	meshletB_.Destroy();
	indirectArgumentUAV_.Destroy();
	indirectArgumentB_.Destroy();
}

//----
void MeshletRenderComponent::TransitionIndirectArgument(sl12::CommandList* pCmdList, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
	pCmdList->TransitionBarrier(&indirectArgumentB_, before, after);
}


//----
bool MeshInstance::Initialize(sl12::Device* pDevice, sl12::ResourceHandle res)
{
	pParentDevice_ = pDevice;
	hResMesh_ = res;
	auto id = DirectX::XMMatrixIdentity();
	DirectX::XMStoreFloat4x4(&mtxTransform_, id);

	auto mesh_item = res.GetItem<sl12::ResourceItemMesh>();
	if (!mesh_item->GetSubmeshes()[0].meshlets.empty())
	{
		auto&& submeshes = mesh_item->GetSubmeshes();
		meshletComponents_.reserve(submeshes.size());
		for (auto&& submesh : submeshes)
		{
			MeshletRenderComponent* comp = new MeshletRenderComponent();
			if (!comp->Initialize(pDevice, submesh.meshlets))
			{
				return false;
			}
			meshletComponents_.push_back(comp);
		}
	}

	return true;
}

//----
void MeshInstance::Destroy()
{
	for (auto&& item : meshletComponents_)
		pParentDevice_->KillObject(item);
	meshletComponents_.clear();
}


//	EOF
