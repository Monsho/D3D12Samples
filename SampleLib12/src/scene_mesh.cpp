#include "sl12/scene_mesh.h"

#include "sl12/device.h"


namespace sl12
{
	namespace
	{
		struct MeshletBound
		{
			DirectX::XMFLOAT3	aabbMin;
			DirectX::XMFLOAT3	aabbMax;
			DirectX::XMFLOAT3	coneApex;
			DirectX::XMFLOAT3	coneAxis;
			float				coneCutoff;
			sl12::u32			pad[3];
		};	// struct MeshletBound

		struct MeshletDrawInfo
		{
			sl12::u32	indexOffset;
			sl12::u32	indexCount;
			sl12::u32	pad[2];
		};	// struct MeshletDrawInfo
	}


	//------------
	//----
	SceneSubmesh::SceneSubmesh(sl12::Device* pDevice, const sl12::ResourceItemMesh::Submesh& submesh)
		: pParentDevice_(pDevice)
	{
		// create buffers.
		pMeshletBoundsB_ = new sl12::Buffer();
		pMeshletDrawInfoB_ = new sl12::Buffer();
		pMeshletBoundsBV_ = new sl12::BufferView();
		pMeshletDrawInfoBV_ = new sl12::BufferView();
		pMeshletCB_ = new sl12::Buffer();
		pMeshletCBV_ = new sl12::ConstantBufferView();

		pMeshletBoundsB_->Initialize(
			pDevice,
			sizeof(MeshletBound) * submesh.meshlets.size(),
			sizeof(MeshletBound),
			sl12::BufferUsage::ShaderResource,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			true, false);
		pMeshletDrawInfoB_->Initialize(
			pDevice,
			sizeof(MeshletDrawInfo) * submesh.meshlets.size(),
			sizeof(MeshletDrawInfo),
			sl12::BufferUsage::ShaderResource,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			true, false);
		pMeshletBoundsBV_->Initialize(pDevice, pMeshletBoundsB_, 0, 0, sizeof(MeshletBound));
		pMeshletDrawInfoBV_->Initialize(pDevice, pMeshletDrawInfoB_, 0, 0, sizeof(MeshletDrawInfo));

		pMeshletCB_->Initialize(
			pDevice,
			sizeof(MeshletCB),
			sizeof(MeshletCB),
			sl12::BufferUsage::ConstantBuffer,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			true, false);
		pMeshletCBV_->Initialize(pDevice, pMeshletCB_);

		// upload meshlet data.
		auto bound = (MeshletBound*)pMeshletBoundsB_->Map(nullptr);
		auto draw_info = (MeshletDrawInfo*)pMeshletDrawInfoB_->Map(nullptr);
		for (auto&& meshlet : submesh.meshlets)
		{
			bound->aabbMin = meshlet.boundingInfo.box.aabbMin;
			bound->aabbMax = meshlet.boundingInfo.box.aabbMax;
			bound->coneApex = meshlet.boundingInfo.cone.apex;
			bound->coneAxis = meshlet.boundingInfo.cone.axis;
			bound->coneCutoff = meshlet.boundingInfo.cone.cutoff;

			draw_info->indexOffset = meshlet.indexOffset;
			draw_info->indexCount = meshlet.indexCount;

			bound++;
			draw_info++;
		}
		pMeshletBoundsB_->Unmap();
		pMeshletDrawInfoB_->Unmap();
	}

	//----
	SceneSubmesh::~SceneSubmesh()
	{
		if (pParentDevice_)
		{
			pParentDevice_->KillObject(pMeshletCBV_);
			pParentDevice_->KillObject(pMeshletCB_);
			pParentDevice_->KillObject(pMeshletBoundsBV_);
			pParentDevice_->KillObject(pMeshletDrawInfoBV_);
			pParentDevice_->KillObject(pMeshletBoundsB_);
			pParentDevice_->KillObject(pMeshletDrawInfoB_);
		}
	}


	//------------
	//----
	SceneMesh::SceneMesh(sl12::Device* pDevice, const sl12::ResourceItemMesh* pSrcMesh)
		: pParentDevice_(pDevice)
		, pParentResource_(pSrcMesh)
		, mtxLocalToWorld_(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f)
	{
		// count total meshlets.
		auto&& submeshes = pSrcMesh->GetSubmeshes();
		sl12::u32 submesh_count = (sl12::u32)submeshes.size();
		sl12::u32 total_meshlets_count = 0;
		for (auto&& submesh : submeshes)
		{
			total_meshlets_count += (sl12::u32)submesh.meshlets.size();
		}

		// create buffers.
		pIndirectArgBuffer_ = new sl12::Buffer();
		pIndirectCountBuffer_ = new sl12::Buffer();
		pFalseNegativeBuffer_ = new sl12::Buffer();
		pFalseNegativeCountBuffer_ = new sl12::Buffer();
		pIndirectArgUAV_ = new sl12::UnorderedAccessView();
		pIndirectCountUAV_ = new sl12::UnorderedAccessView();
		pFalseNegativeUAV_ = new sl12::UnorderedAccessView();
		pFalseNegativeCountUAV_ = new sl12::UnorderedAccessView();

		pIndirectArgBuffer_->Initialize(
			pDevice,
			sizeof(D3D12_DRAW_INDEXED_ARGUMENTS) * total_meshlets_count * 2,
			sizeof(D3D12_DRAW_INDEXED_ARGUMENTS),
			sl12::BufferUsage::ShaderResource,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			false, true);
		pIndirectCountBuffer_->Initialize(
			pDevice,
			sizeof(sl12::u32) * submesh_count * 2,
			sizeof(sl12::u32),
			sl12::BufferUsage::ShaderResource,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			false, true);
		pFalseNegativeBuffer_->Initialize(
			pDevice,
			sizeof(sl12::u32) * total_meshlets_count,
			sizeof(sl12::u32),
			sl12::BufferUsage::ShaderResource,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			false, true);
		pFalseNegativeCountBuffer_->Initialize(
			pDevice,
			sizeof(sl12::u32) * submesh_count,
			sizeof(sl12::u32),
			sl12::BufferUsage::ShaderResource,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			false, true);

		pIndirectArgUAV_->Initialize(
			pDevice,
			pIndirectArgBuffer_,
			0, 0, 0, 0);
		pIndirectCountUAV_->Initialize(
			pDevice,
			pIndirectCountBuffer_,
			0, 0, 0, 0);
		pFalseNegativeUAV_->Initialize(
			pDevice,
			pFalseNegativeBuffer_,
			0, 0, 0, 0);
		pFalseNegativeCountUAV_->Initialize(
			pDevice,
			pFalseNegativeCountBuffer_,
			0, 0, 0, 0);

		// create submeshes.
		sl12::u32 submesh_offset = 0;
		sl12::u32 meshlets_offset = 0;
		for (auto&& submesh : submeshes)
		{
			auto meshlets_count = (sl12::u32)submesh.meshlets.size();
			auto ss = std::make_unique<SceneSubmesh>(pDevice, submesh);

			auto cb = &ss->cbData_;
			cb->meshletCount = meshlets_count;
			cb->indirectArg1stIndexOffset = (meshlets_offset * 2);
			cb->indirectArg2ndIndexOffset = (meshlets_offset * 2 + meshlets_count);
			cb->indirectCount1stByteOffset = sizeof(sl12::u32) * (submesh_offset * 2);
			cb->indirectCount2ndByteOffset = sizeof(sl12::u32) * (submesh_offset * 2 + 1);
			cb->falseNegativeIndexOffset = meshlets_offset;
			cb->falseNegativeCountByteOffset = sizeof(sl12::u32) * submesh_offset;
			memcpy(ss->pMeshletCB_->Map(nullptr), cb, sizeof(*cb));
			ss->pMeshletCB_->Unmap();

			sceneSubmeshes_.push_back(std::move(ss));
			submesh_offset++;
			meshlets_offset += meshlets_count;
		}
	}

	//----
	SceneMesh::~SceneMesh()
	{
		sceneSubmeshes_.clear();

		if (pParentDevice_)
		{
			pParentDevice_->KillObject(pIndirectArgUAV_);
			pParentDevice_->KillObject(pIndirectCountUAV_);
			pParentDevice_->KillObject(pFalseNegativeUAV_);
			pParentDevice_->KillObject(pFalseNegativeCountUAV_);

			pParentDevice_->KillObject(pIndirectArgBuffer_);
			pParentDevice_->KillObject(pIndirectCountBuffer_);
			pParentDevice_->KillObject(pFalseNegativeBuffer_);
			pParentDevice_->KillObject(pFalseNegativeCountBuffer_);
		}
	}

	//----
	void SceneMesh::CreateRenderCommand(ConstantBufferCache* pCBCache, RenderCommandsList& outRenderCmds)
	{
		auto ret = std::make_unique<MeshRenderCommand>(this, pCBCache);
		mtxPrevLocalToWorld_ = mtxLocalToWorld_;
		outRenderCmds.push_back(std::move(ret));
	}

}	// namespace sl12
//	EOF
