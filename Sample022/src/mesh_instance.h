#pragma once

#include "sl12/device.h"
#include "sl12/command_list.h"
#include "sl12/acceleration_structure.h"
#include "sl12/resource_mesh.h"
#include "sl12/buffer.h"
#include "sl12/buffer_view.h"

#include <map>


class MeshletRenderComponent
{
public:
	MeshletRenderComponent()
	{}
	~MeshletRenderComponent()
	{
		Destroy();
	}

	bool Initialize(sl12::Device* pDev, const std::vector<sl12::ResourceItemMesh::Meshlet>& meshlets);

	void Destroy();

	void TransitionIndirectArgument(sl12::CommandList* pCmdList, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

	sl12::BufferView& GetMeshletBV()
	{
		return meshletBV_;
	}
	sl12::Buffer& GetIndirectArgumentB()
	{
		return indirectArgumentB_;
	}
	sl12::UnorderedAccessView& GetIndirectArgumentUAV()
	{
		return indirectArgumentUAV_;
	}

private:
	sl12::Buffer				meshletB_;				// meshlet info structured buffer.
	sl12::BufferView			meshletBV_;				// meshletB_ view.
	sl12::Buffer				indirectArgumentB_;		// indirect argument buffer.
	sl12::UnorderedAccessView	indirectArgumentUAV_;	// indirectArgumentB_ view.
};	// class MeshletRenderComponent

class MeshInstance
{
public:
	MeshInstance()
	{}
	~MeshInstance()
	{
		Destroy();
	}

	bool Initialize(sl12::Device* pDevice, sl12::ResourceHandle res);

	void Destroy();

private:
	sl12::Device*							pParentDevice_ = nullptr;
	sl12::ResourceHandle					hResMesh_;
	DirectX::XMFLOAT4X4						mtxTransform_;
	std::vector<MeshletRenderComponent*>	meshletComponents_;
};	// class MeshInstance


//	EOF
