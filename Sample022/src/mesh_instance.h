#pragma once

#include "sl12/device.h"
#include "sl12/command_list.h"
#include "sl12/acceleration_structure.h"
#include "sl12/resource_mesh.h"
#include "sl12/buffer.h"
#include "sl12/buffer_view.h"

#include <map>


namespace sl12
{
	class MeshletRenderComponent
	{
	public:
		MeshletRenderComponent()
		{}
		~MeshletRenderComponent()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, const std::vector<ResourceItemMesh::Meshlet>& meshlets);

		void Destroy();

		void TransitionIndirectArgument(CommandList* pCmdList, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

		BufferView& GetMeshletBV()
		{
			return meshletBV_;
		}
		Buffer& GetIndirectArgumentB()
		{
			return indirectArgumentB_;
		}
		UnorderedAccessView& GetIndirectArgumentUAV()
		{
			return indirectArgumentUAV_;
		}

	private:
		Buffer				meshletB_;				// meshlet info structured buffer.
		BufferView			meshletBV_;				// meshletB_ view.
		Buffer				indirectArgumentB_;		// indirect argument buffer.
		UnorderedAccessView	indirectArgumentUAV_;	// indirectArgumentB_ view.
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

		bool Initialize(Device* pDevice, ResourceHandle res);

		void Destroy();

		ResourceHandle GetResMesh()
		{
			return hResMesh_;
		}
		const ResourceHandle GetResMesh() const
		{
			return hResMesh_;
		}

		void SetMtxTransform(const DirectX::XMFLOAT4X4& t)
		{
			mtxTransform_ = t;
		}
		const DirectX::XMFLOAT4X4& GetMtxTransform() const
		{
			return mtxTransform_;
		}

	private:
		Device*									pParentDevice_ = nullptr;
		ResourceHandle							hResMesh_;
		DirectX::XMFLOAT4X4						mtxTransform_;
		std::vector<MeshletRenderComponent*>	meshletComponents_;
	};	// class MeshInstance

}	// namespace sl12

//	EOF
