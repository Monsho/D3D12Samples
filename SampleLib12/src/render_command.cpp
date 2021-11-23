#include "sl12/render_command.h"
#include "sl12/scene_mesh.h"


namespace sl12
{
	//------------
	//----
	SubmeshRenderCommand::SubmeshRenderCommand(SceneSubmesh* pSubmesh)
		: pParentSubmesh_(pSubmesh)
	{
		isUnbound_ = true;
	}

	//----
	SubmeshRenderCommand::~SubmeshRenderCommand()
	{}


	//------------
	//----
	MeshRenderCommand::MeshRenderCommand(SceneMesh* pMesh, ConstantBufferCache* pCBCache)
		: pParentMesh_(pMesh)
	{
		isUnbound_ = false;

		// ready constant buffer.
		struct
		{
			DirectX::XMFLOAT4X4	curr;
			DirectX::XMFLOAT4X4	prev;
		} cb;

		cb.curr = pMesh->GetMtxLocalToWorld();
		cb.prev = pMesh->GetMtxPrevLocalToWorld();

		cbHandle_ = pCBCache->GetUnusedConstBuffer(sizeof(cb), &cb);

		// create submesh commands.
		auto sbCount = pMesh->GetSubmeshCount();
		submeshCommands_.resize(sbCount);
		for (u32 i = 0; i < sbCount; i++)
		{
			submeshCommands_[i] = std::make_unique<SubmeshRenderCommand>(pMesh->GetSubmesh(i));
		}

		// calc bounding sphere.
		auto&& bounding = pMesh->GetParentResource()->GetBoundingInfo();
		
		DirectX::XMVECTOR aabbMin = DirectX::XMLoadFloat3(&bounding.box.aabbMin);
		DirectX::XMVECTOR aabbMax = DirectX::XMLoadFloat3(&bounding.box.aabbMax);
		DirectX::XMMATRIX tr = DirectX::XMLoadFloat4x4(&cb.curr);
		aabbMin = DirectX::XMVector3TransformCoord(aabbMin, tr);
		aabbMax = DirectX::XMVector3TransformCoord(aabbMax, tr);
		DirectX::XMVECTOR center = DirectX::XMVectorScale(DirectX::XMVectorAdd(aabbMax, aabbMin), 0.5f);
		float radius = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(center, aabbMin)));
		DirectX::XMFLOAT3 c;
		DirectX::XMStoreFloat3(&c, center);
		boundingSphere_ = BoundingSphere(c, radius);
	}

	//----
	MeshRenderCommand::~MeshRenderCommand()
	{
		submeshCommands_.clear();
	}

}	// namespace sl12

//	EOF
