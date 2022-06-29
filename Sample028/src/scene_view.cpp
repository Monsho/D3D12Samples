#include "scene_view.h"

#include "sl12/device.h"
#include "sl12/command_list.h"
#include "sl12/buffer.h"
#include "sl12/buffer_view.h"
#include "sl12/scene_mesh.h"
#include "sl12/resource_mesh.h"


namespace
{
	struct Constant
	{
		sl12::u32	CullUnitCount;
		sl12::u32	BatchCount;
	};	// struct Constant

	struct CullUnitData
	{
		sl12::u32	InstanceIndex;
		sl12::u32	BatchIndex;
		sl12::u32	MeshletIndex;
		sl12::u32	pad[1];
	};	// struct CullUnitData

	using IndirectArgData = D3D12_DRAW_INDEXED_ARGUMENTS;

	struct InstanceData
	{
		DirectX::XMFLOAT4X4	Transform;
	};	// struct InstanceData

	struct BatchData
	{
		sl12::u32	IndirectArgOffset;
		sl12::u32	MeshletOffset;
		sl12::u32	pad[2];
	};	// struct BatchData

	struct MeshletBoundData
	{
		DirectX::XMFLOAT3	aabbMin;
		DirectX::XMFLOAT3	aabbMax;
		DirectX::XMFLOAT3	coneApex;
		DirectX::XMFLOAT3	coneAxis;
		float				coneCutoff;
		sl12::u32			pad[3];
	};	// struct MeshletBoundData

	struct MeshletDrawData
	{
		sl12::u32	indexOffset;
		sl12::u32	indexCount;
		sl12::u32	pad[2];
	};	// struct MeshletDrawData

	template <class T>
	void Release(sl12::Device* pDev, T*& p)
	{
		if (p)
		{
			pDev->KillObject(p);
			p = nullptr;
		}
	}
}

namespace sl12
{
	//----
	SceneView::SceneView(Device* pDev)
		: pParentDevice_(pDev)
	{}

	//----
	SceneView::~SceneView()
	{
		ReleaseBuffers();
	}

	//----
	void SceneView::BeginNewFrame(CommandList* pCmdList, SceneRoot* pSceneRoot, RenderCommandsList& RenderCmds)
	{
		// gather mesh cmds.
		drawMeshCmds_.clear();
		drawMeshCmds_.reserve(RenderCmds.size());
		for (auto&& cmd : RenderCmds)
		{
			if (cmd->GetType() == RenderCommandType::Mesh)
			{
				drawMeshCmds_.push_back(static_cast<MeshRenderCommand*>(cmd.get()));
			}
		}

		// create buffers for culling.
		if (pSceneRoot->IsDirty())
		{
			ReleaseBuffers();
			batchInfos_.clear();

			struct RenderItem
			{
				const ResourceItemMesh*	pItem;
				std::vector<u32>		meshletOffsets;
			};

			std::vector<CullUnitData> cullUnits;
			std::vector<BatchData> batches;
			std::vector<MeshletBoundData> meshletBounds;
			std::vector<MeshletDrawData> meshletDraws;
			std::vector<RenderItem> renderItems;
			u32 instanceIndex = 0;
			for (auto&& cmd : drawMeshCmds_)
			{
				auto pSceneMesh = cmd->GetParentMesh();
				auto pResMesh = pSceneMesh->GetParentResource();

				// find or create render item data.
				RenderItem newRenderItem;
				RenderItem* pRenderItem = &newRenderItem;
				auto findItem = std::find_if(renderItems.begin(), renderItems.end(), [&](const RenderItem& v) { return v.pItem == pResMesh; });
				if (findItem != renderItems.end())
				{
					pRenderItem = &*findItem;
				}
				else
				{
					u32 meshletOffset = (u32)meshletBounds.size();
					newRenderItem.pItem = pResMesh;
					newRenderItem.meshletOffsets.reserve(pResMesh->GetSubmeshes().size());
					for (auto&& submesh : pResMesh->GetSubmeshes())
					{
						newRenderItem.meshletOffsets.push_back(meshletOffset);
						meshletOffset += (u32)submesh.meshlets.size();

						for (auto&& meshlet : submesh.meshlets)
						{
							MeshletBoundData bound;
							bound.aabbMin = meshlet.boundingInfo.box.aabbMin;
							bound.aabbMax = meshlet.boundingInfo.box.aabbMax;
							bound.coneApex = meshlet.boundingInfo.cone.apex;
							bound.coneAxis = meshlet.boundingInfo.cone.axis;
							bound.coneCutoff = meshlet.boundingInfo.cone.cutoff;
							meshletBounds.push_back(bound);

							MeshletDrawData draw;
							draw.indexOffset = meshlet.indexOffset;
							draw.indexCount = meshlet.indexCount;
							meshletDraws.push_back(draw);
						}
					}
					renderItems.push_back(newRenderItem);
				}

				// fill data for instance.
				CullUnitData cullUnit;
				cullUnit.InstanceIndex = instanceIndex;
				u32 submeshIndex = 0;
				for (auto&& submesh : pResMesh->GetSubmeshes())
				{
					cullUnit.BatchIndex = (u32)batches.size();
					cullUnit.MeshletIndex = 0;

					BatchData batch;
					batch.IndirectArgOffset = (u32)cullUnits.size();
					batch.MeshletOffset = pRenderItem->meshletOffsets[submeshIndex];
					batches.push_back(batch);

					BatchInfo info;
					info.instanceIndex = instanceIndex;
					info.submeshIndex = submeshIndex;
					info.indirectArgByteOffset = batch.IndirectArgOffset * sizeof(IndirectArgData);
					info.drawCountByteOffset = (u32)((batches.size() - 1) * sizeof(u32));
					batchInfos_.push_back(info);

					for (auto&& meshlet : submesh.meshlets)
					{
						cullUnits.push_back(cullUnit);
						cullUnit.MeshletIndex++;
					}

					submeshIndex++;
				}

				instanceIndex++;
			}
			cullUnitCount_ = (u32)cullUnits.size();
			batchCount_ = (u32)batches.size();

			auto CreateBuffer = [&](const void* pData, size_t size, size_t stride, D3D12_RESOURCE_STATES initState, bool isUAV)
			{
				D3D12_RESOURCE_STATES state = (pData != nullptr) ? D3D12_RESOURCE_STATE_COPY_DEST : initState;
				Buffer* ret = new Buffer();
				ret->Initialize(pParentDevice_, size, stride, BufferUsage::ShaderResource, state, false, isUAV);

				if (pData)
				{
					Buffer* src = new Buffer();
					src->Initialize(pParentDevice_, size, stride, BufferUsage::ShaderResource, true, false);
					memcpy(src->Map(), pData, size);
					src->Unmap();

					pCmdList->GetLatestCommandList()->CopyBufferRegion(ret->GetResourceDep(), 0, src->GetResourceDep(), 0, size);
					pCmdList->TransitionBarrier(ret, state, initState);

					pParentDevice_->KillObject(src);
				}

				return ret;
			};

			pConstant_ = new Buffer();
			pConstant_->Initialize(pParentDevice_, sizeof(Constant), 0, BufferUsage::ConstantBuffer, true, false);
			{
				Constant* p = (Constant*)pConstant_->Map();
				p->CullUnitCount = (u32)cullUnits.size();
				p->BatchCount = (u32)batches.size();
			}
			pConstantCBV_ = new ConstantBufferView();
			pConstantCBV_->Initialize(pParentDevice_, pConstant_);

			// create buffers.
			pCullUnit_ = CreateBuffer(cullUnits.data(), cullUnits.size() * sizeof(CullUnitData), sizeof(CullUnitData), D3D12_RESOURCE_STATE_GENERIC_READ, false);
			pIndirectArg_ = CreateBuffer(nullptr, cullUnits.size() * 2 * sizeof(IndirectArgData), sizeof(u32), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, true);
			pDrawCount_ = CreateBuffer(nullptr, (batches.size() * 2 + 1) * sizeof(u32), sizeof(u32), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, true);
			pFalseNegativeIndex_ = CreateBuffer(nullptr, cullUnits.size() * sizeof(u32), sizeof(u32), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
			pFalseNegativeIndirect_ = CreateBuffer(nullptr, 3 * sizeof(u32), sizeof(u32), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, true);
			pInstanceData_ = CreateBuffer(nullptr, drawMeshCmds_.size() * sizeof(InstanceData), sizeof(InstanceData), D3D12_RESOURCE_STATE_GENERIC_READ, false);
			pBatchData_ = CreateBuffer(batches.data(), batches.size() * sizeof(BatchData), sizeof(BatchData), D3D12_RESOURCE_STATE_GENERIC_READ, false);
			pMeshletBoundData_ = CreateBuffer(meshletBounds.data(), meshletBounds.size() * sizeof(MeshletBoundData), sizeof(MeshletBoundData), D3D12_RESOURCE_STATE_GENERIC_READ, false);
			pMeshletDrawData_ = CreateBuffer(meshletDraws.data(), meshletDraws.size() * sizeof(MeshletDrawData), sizeof(MeshletDrawData), D3D12_RESOURCE_STATE_GENERIC_READ, false);

			// create views.
			pCullUnitBV_ = new BufferView();
			pCullUnitBV_->Initialize(pParentDevice_, pCullUnit_, 0, (u32)cullUnits.size(), sizeof(CullUnitData));

			pIndirectArgUAV_ = new UnorderedAccessView();
			pIndirectArgUAV_->Initialize(pParentDevice_, pIndirectArg_, 0, 0, 0, 0);

			pDrawCountUAV_ = new UnorderedAccessView();
			pDrawCountUAV_->Initialize(pParentDevice_, pDrawCount_, 0, 0, 0, 0);

			pFalseNegativeIndexBV_ = new BufferView();
			pFalseNegativeIndexUAV_ = new UnorderedAccessView();
			pFalseNegativeIndirectUAV_ = new UnorderedAccessView();
			pFalseNegativeIndexBV_->Initialize(pParentDevice_, pFalseNegativeIndex_, 0, (u32)cullUnits.size(), sizeof(u32));
			pFalseNegativeIndexUAV_->Initialize(pParentDevice_, pFalseNegativeIndex_, 0, 0, 0, 0);
			pFalseNegativeIndirectUAV_->Initialize(pParentDevice_, pFalseNegativeIndirect_, 0, 0, 0, 0);

			pInstanceDataBV_ = new BufferView();
			pInstanceDataBV_->Initialize(pParentDevice_, pInstanceData_, 0, (u32)drawMeshCmds_.size(), sizeof(InstanceData));

			pBatchDataBV_ = new BufferView();
			pBatchDataBV_->Initialize(pParentDevice_, pBatchData_, 0, (u32)batches.size(), sizeof(BatchData));

			pMeshletBoundDataBV_ = new BufferView();
			pMeshletDrawDataBV_ = new BufferView();
			pMeshletBoundDataBV_->Initialize(pParentDevice_, pMeshletBoundData_, 0, (u32)meshletBounds.size(), sizeof(MeshletBoundData));
			pMeshletDrawDataBV_->Initialize(pParentDevice_, pMeshletDrawData_, 0, (u32)meshletDraws.size(), sizeof(MeshletDrawData));
		}

		// fill instance data.
		std::vector<InstanceData> instances;
		instances.reserve(drawMeshCmds_.size());
		for (auto&& cmd : drawMeshCmds_)
		{
			InstanceData data;
			data.Transform = cmd->GetParentMesh()->GetMtxLocalToWorld();
			instances.push_back(data);
		}
		Buffer* src = new Buffer();
		src->Initialize(pParentDevice_, instances.size() * sizeof(InstanceData), sizeof(InstanceData), BufferUsage::ShaderResource, true, false);
		memcpy(src->Map(), instances.data(), instances.size() * sizeof(InstanceData));
		src->Unmap();
		pCmdList->TransitionBarrier(pInstanceData_, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST);
		pCmdList->GetLatestCommandList()->CopyBufferRegion(pInstanceData_->GetResourceDep(), 0, src->GetResourceDep(), 0, instances.size() * sizeof(InstanceData));
		pCmdList->TransitionBarrier(pInstanceData_, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
		pParentDevice_->KillObject(src);
	}

	//----
	void SceneView::ReleaseBuffers()
	{
		Release(pParentDevice_, pConstant_);
		Release(pParentDevice_, pCullUnit_);
		Release(pParentDevice_, pIndirectArg_);
		Release(pParentDevice_, pDrawCount_);
		Release(pParentDevice_, pFalseNegativeIndex_);
		Release(pParentDevice_, pInstanceData_);
		Release(pParentDevice_, pBatchData_);
		Release(pParentDevice_, pMeshletBoundData_);
		Release(pParentDevice_, pMeshletDrawData_);

		Release(pParentDevice_, pConstantCBV_);
		Release(pParentDevice_, pCullUnitBV_);
		Release(pParentDevice_, pIndirectArgUAV_);
		Release(pParentDevice_, pDrawCountUAV_);
		Release(pParentDevice_, pFalseNegativeIndexBV_);
		Release(pParentDevice_, pFalseNegativeIndexUAV_);
		Release(pParentDevice_, pFalseNegativeIndirectUAV_);
		Release(pParentDevice_, pInstanceDataBV_);
		Release(pParentDevice_, pBatchDataBV_);
		Release(pParentDevice_, pMeshletBoundDataBV_);
		Release(pParentDevice_, pMeshletDrawDataBV_);
	}

}	// namespace sl12

//	EOF
