#include <sl12/acceleration_structure.h>


namespace sl12
{
	//-------------------------------------------------------------------
	// Geometry Desc���O�p�|���S�����b�V���Ƃ��ď�����
	//-------------------------------------------------------------------
	void GeometryStructureDesc::InitializeAsTriangle(
		D3D12_RAYTRACING_GEOMETRY_FLAGS	flags,
		sl12::Buffer*		pVertexBuffer,
		sl12::Buffer*		pIndexBuffer,
		sl12::Buffer*		pTransformBuffer,
		UINT64				vertexStride,
		UINT				vertexCount,
		DXGI_FORMAT			vertexFormat,
		UINT				indexCount,
		DXGI_FORMAT			indexFormat)
	{
		if (!pVertexBuffer || !pIndexBuffer)
			return;

		dxrDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		dxrDesc.Flags = flags;
		dxrDesc.Triangles.VertexBuffer.StartAddress = pVertexBuffer->GetResourceDep()->GetGPUVirtualAddress();
		dxrDesc.Triangles.VertexBuffer.StrideInBytes = vertexStride;
		dxrDesc.Triangles.VertexCount = vertexCount;
		dxrDesc.Triangles.VertexFormat = vertexFormat;
		dxrDesc.Triangles.IndexBuffer = pIndexBuffer->GetResourceDep()->GetGPUVirtualAddress();
		dxrDesc.Triangles.IndexCount = indexCount;
		dxrDesc.Triangles.IndexFormat = indexFormat;
		dxrDesc.Triangles.Transform3x4 = pTransformBuffer ? pTransformBuffer->GetResourceDep()->GetGPUVirtualAddress() : 0;
	}
	void GeometryStructureDesc::InitializeAsTriangle(
		D3D12_RAYTRACING_GEOMETRY_FLAGS	flags,
		sl12::Buffer*		pVertexBuffer,
		sl12::Buffer*		pIndexBuffer,
		sl12::Buffer*		pTransformBuffer,
		UINT64				vertexStride,
		UINT				vertexCount,
		UINT64				vertexOffset,
		DXGI_FORMAT			vertexFormat,
		UINT				indexCount,
		UINT64				indexOffset,
		DXGI_FORMAT			indexFormat)
	{
		if (!pVertexBuffer || !pIndexBuffer)
			return;

		dxrDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		dxrDesc.Flags = flags;
		dxrDesc.Triangles.VertexBuffer.StartAddress = pVertexBuffer->GetResourceDep()->GetGPUVirtualAddress() + vertexOffset;
		dxrDesc.Triangles.VertexBuffer.StrideInBytes = vertexStride;
		dxrDesc.Triangles.VertexCount = vertexCount;
		dxrDesc.Triangles.VertexFormat = vertexFormat;
		dxrDesc.Triangles.IndexBuffer = pIndexBuffer->GetResourceDep()->GetGPUVirtualAddress() + indexOffset;
		dxrDesc.Triangles.IndexCount = indexCount;
		dxrDesc.Triangles.IndexFormat = indexFormat;
		dxrDesc.Triangles.Transform3x4 = pTransformBuffer ? pTransformBuffer->GetResourceDep()->GetGPUVirtualAddress() : 0;
	}

	//-------------------------------------------------------------------
	// Geometry Desc��AABB�Ƃ��ď�����
	//-------------------------------------------------------------------
	void GeometryStructureDesc::InitializeAsAABB(
		D3D12_RAYTRACING_GEOMETRY_FLAGS	flags,
		sl12::Buffer*		pAABBBuffer,
		UINT64				bufferStride,
		UINT				aabbCount)
	{
		if (!pAABBBuffer)
			return;

		dxrDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
		dxrDesc.Flags = flags;
		dxrDesc.AABBs.AABBs.StartAddress = pAABBBuffer->GetResourceDep()->GetGPUVirtualAddress();
		dxrDesc.AABBs.AABBs.StrideInBytes = bufferStride;
		dxrDesc.AABBs.AABBCount = aabbCount;
	}


	//-------------------------------------------------------------------
	// Top AS�Ƃ��ď�����
	//-------------------------------------------------------------------
	bool StructureInputDesc::InitializeAsTop(
		sl12::Device*										pDevice,
		UINT												instanceCount,
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS	flags)
	{
		if (!pDevice)
			return false;

		inputDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		inputDesc.Flags = flags;
		inputDesc.NumDescs = instanceCount;
		inputDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

		pDevice->GetDxrDeviceDep()->GetRaytracingAccelerationStructurePrebuildInfo(&inputDesc, &prebuildInfo);
		if (prebuildInfo.ResultDataMaxSizeInBytes == 0)
			return false;

		return true;
	}

	//-------------------------------------------------------------------
	// Bottom AS�Ƃ��ď�����
	//-------------------------------------------------------------------
	bool StructureInputDesc::InitializeAsBottom(
		sl12::Device*										pDevice,
		GeometryStructureDesc*								pGeos,
		UINT												geosCount,
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS	flags)
	{
		if (!pDevice)
			return false;

		geos.resize(geosCount);
		for (UINT i = 0; i < geosCount; i++)
			geos[i] = pGeos[i].dxrDesc;

		inputDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		inputDesc.Flags = flags;
		inputDesc.NumDescs = geosCount;
		inputDesc.pGeometryDescs = geos.data();
		inputDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

		pDevice->GetDxrDeviceDep()->GetRaytracingAccelerationStructurePrebuildInfo(&inputDesc, &prebuildInfo);
		if (prebuildInfo.ResultDataMaxSizeInBytes == 0)
			return false;

		return true;
	}


	//-------------------------------------------------------------------
	// �C���X�^���X�L�q�q������������
	//-------------------------------------------------------------------
	void TopInstanceDesc::Initialize(
		const DirectX::XMFLOAT4X4&			transform,
		UINT								id,
		UINT								mask,
		UINT								contribution,
		UINT								flags,
		class BottomAccelerationStructure*	pBottomAS)
	{
		dxrDesc.Transform[0][0] = transform._11;
		dxrDesc.Transform[0][1] = transform._21;
		dxrDesc.Transform[0][2] = transform._31;
		dxrDesc.Transform[0][3] = transform._41;
		dxrDesc.Transform[1][0] = transform._12;
		dxrDesc.Transform[1][1] = transform._22;
		dxrDesc.Transform[1][2] = transform._32;
		dxrDesc.Transform[1][3] = transform._42;
		dxrDesc.Transform[2][0] = transform._13;
		dxrDesc.Transform[2][1] = transform._23;
		dxrDesc.Transform[2][2] = transform._33;
		dxrDesc.Transform[2][3] = transform._43;
		dxrDesc.InstanceID = id;
		dxrDesc.InstanceMask = mask;
		dxrDesc.InstanceContributionToHitGroupIndex = contribution;
		dxrDesc.Flags = flags;
		dxrDesc.AccelerationStructure = pBottomAS->GetDxrBuffer().GetResourceDep()->GetGPUVirtualAddress();
	}
	void TopInstanceDesc::Initialize(
		const DirectX::XMFLOAT4X4&			transform,
		class BottomAccelerationStructure*	pBottomAS)
	{
		Initialize(transform, 0, 0xff, 0, 0, pBottomAS);
	}
	void TopInstanceDesc::Initialize(
		class BottomAccelerationStructure*	pBottomAS)
	{
		DirectX::XMFLOAT4X4 transform;
		DirectX::XMStoreFloat4x4(&transform, DirectX::XMMatrixIdentity());
		Initialize(transform, 0, 0xff, 0, 0, pBottomAS);
	}


	//-------------------------------------------------------------------
	// AS�p�̃o�b�t�@�𐶐�����
	// �I�v�V�����ŃX�N���b�`�o�b�t�@������
	//-------------------------------------------------------------------
	bool AccelerationStructure::CreateBuffer(
		Device*			pDevice,
		size_t			size,
		size_t			scratchSize)
	{
		pDxrBuffer_ = new Buffer();
		if (!pDxrBuffer_)
		{
			return false;
		}
		if (!pDxrBuffer_->Initialize(pDevice, size, 0, BufferUsage::AccelerationStructure, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, false, true))
		{
			return false;
		}

		if (scratchSize > 0)
		{
			pScratchBuffer_ = new Buffer();
			scratchCreated_ = true;
			if (!pScratchBuffer_->Initialize(pDevice, scratchSize, 0, BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false, true))
			{
				return false;
			}
		}

		return true;
	}

	//-------------------------------------------------------------------
	// �X�N���b�`�o�b�t�@���O������ݒ肷��
	//-------------------------------------------------------------------
	void AccelerationStructure::SetScratchBuffer(Buffer* p)
	{
		if (!p)
			return;

		if (scratchCreated_)
		{
			SafeDelete(pScratchBuffer_);
			scratchCreated_ = false;
		}
		pScratchBuffer_ = p;
	}

	//-------------------------------------------------------------------
	// �j������
	//-------------------------------------------------------------------
	void AccelerationStructure::Destroy()
	{
		DestroyScratchBuffer();
		SafeDelete(pDxrBuffer_);
	}

	//-------------------------------------------------------------------
	// �X�N���b�`�o�b�t�@�̂ݔj������
	//-------------------------------------------------------------------
	void AccelerationStructure::DestroyScratchBuffer()
	{
		if (scratchCreated_)
		{
			SafeDelete(pScratchBuffer_);
			scratchCreated_ = false;
		}
		pScratchBuffer_ = nullptr;
	}


	//-------------------------------------------------------------------
	// Bottom AS�̃r���h�R�}���h�𔭍s����
	//-------------------------------------------------------------------
	bool BottomAccelerationStructure::Build(Device* pDevice, CommandList* pCmdList, const StructureInputDesc& desc, bool barrier)
	{
		if (!pCmdList)
			return false;
		if (!pDxrBuffer_ || !pDxrBuffer_->GetResourceDep())
			return false;
		if (!pScratchBuffer_)
			return false;

		bool isCompaction = desc.inputDesc.Flags & D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION;

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc{};
		buildDesc.DestAccelerationStructureData = pDxrBuffer_->GetResourceDep()->GetGPUVirtualAddress();
		buildDesc.Inputs = desc.inputDesc;
		buildDesc.ScratchAccelerationStructureData = pScratchBuffer_->GetResourceDep()->GetGPUVirtualAddress();

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC InfoDesc;
		InfoDesc.InfoType = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_COMPACTED_SIZE;

		// Compaction�̏��o�b�t�@����
		Buffer* pInfoBuffer = nullptr;
		if (isCompaction)
		{
			pInfoBuffer = new Buffer();
			if (!pInfoBuffer)
			{
				return false;
			}
			if (!pInfoBuffer->Initialize(pDevice,
				sizeof(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_COMPACTED_SIZE_DESC),
				0,
				BufferUsage::ShaderResource,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				false, true))
			{
				SafeDelete(pInfoBuffer);
				return false;
			}
			InfoDesc.DestBuffer = pInfoBuffer->GetResourceDep()->GetGPUVirtualAddress();

			// ���R�s�[��o�b�t�@�𐶐�����
			pPostBuildReadBuffer_ = new Buffer();
			if (!pPostBuildReadBuffer_)
			{
				SafeDelete(pInfoBuffer);
				return false;
			}
			if (!pPostBuildReadBuffer_->Initialize(pDevice,
				sizeof(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_COMPACTED_SIZE_DESC),
				0,
				BufferUsage::ReadBack,
				D3D12_RESOURCE_STATE_COPY_DEST,
				false, false))
			{
				SafeDelete(pInfoBuffer);
				SafeDelete(pPostBuildReadBuffer_);
				return false;
			}
		}

		// �r���h�R�}���h
		pCmdList->GetDxrCommandList()->BuildRaytracingAccelerationStructure(
			&buildDesc,
			isCompaction ? 1 : 0,
			isCompaction ? &InfoDesc : nullptr);

		// �o���A
		// Compaction�L���ȏꍇ�͕K���o���A�𒣂��ăT�C�Y�擾���m���ɂ���
		if (barrier || isCompaction)
		{
			// TopAS�r���h�O��BottomAS�̃r���h������҂K�v������܂�.
			// ���\�[�X�o���A�𒣂邱�Ƃ�BottomAS�r���h���������Ă��邱�Ƃ�ۏ؂��܂�.
			pCmdList->UAVBarrier(pDxrBuffer_);
			pCmdList->TransitionBarrier(pInfoBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);

			if (isCompaction)
			{
				pCmdList->GetDxrCommandList()->CopyResource(pPostBuildReadBuffer_->GetResourceDep(), pInfoBuffer->GetResourceDep());
				pDevice->KillObject(pInfoBuffer);
			}
		}

		return true;
	}

	//-------------------------------------------------------------------
	// BLAS��Compaction�����s����
	//-------------------------------------------------------------------
	bool BottomAccelerationStructure::CompactAS(Device* pDevice, CommandList* pCmdList, bool barrier)
	{
		if (!pDevice || !pCmdList || !pPostBuildReadBuffer_ || !pDxrBuffer_)
		{
			return false;
		}

		// �T�C�Y�擾
		auto pDesc = (D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_COMPACTED_SIZE_DESC*)pPostBuildReadBuffer_->Map(nullptr);
		u64 size = pDesc->CompactedSizeInBytes;
		pPostBuildReadBuffer_->Unmap();

		// �V����AS�p�o�b�t�@�𐶐�����
		Buffer* pNewASBuffer = new Buffer();
		if (!pNewASBuffer)
		{
			return false;
		}
		if (!pNewASBuffer->Initialize(pDevice, size, 0, sl12::BufferUsage::AccelerationStructure, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, false, true))
		{
			return false;
		}

		// Compaction Copy���s��
		pCmdList->GetDxrCommandList()->CopyRaytracingAccelerationStructure(
			pNewASBuffer->GetResourceDep()->GetGPUVirtualAddress(),
			pDxrBuffer_->GetResourceDep()->GetGPUVirtualAddress(),
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_COMPACT);

		if (barrier)
		{
			pCmdList->UAVBarrier(pNewASBuffer);
		}

		// �o�b�t�@�X���b�v
		pDevice->KillObject(pDxrBuffer_);
		pDevice->KillObject(pPostBuildReadBuffer_);
		pDxrBuffer_ = pNewASBuffer;
		pPostBuildReadBuffer_ = nullptr;

		return true;
	}


	//-------------------------------------------------------------------
	// Top AS�p�̃o�b�t�@�𐶐�����
	//-------------------------------------------------------------------
	bool TopAccelerationStructure::CreateBuffer(
		sl12::Device*			pDevice,
		size_t					size,
		size_t					scratchSize)
	{
		if (!AccelerationStructure::CreateBuffer(pDevice, size, scratchSize))
		{
			return false;
		}

		descInfo_ = pDevice->GetViewDescriptorHeap().Allocate();
		if (!descInfo_.IsValid())
		{
			return false;
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = DXGI_FORMAT_UNKNOWN;
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
		viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		viewDesc.RaytracingAccelerationStructure.Location = GetDxrBuffer().GetResourceDep()->GetGPUVirtualAddress();
		pDevice->GetDeviceDep()->CreateShaderResourceView(nullptr, &viewDesc, descInfo_.cpuHandle);

		return true;
	}

	//-------------------------------------------------------------------
	// Top AS�p�̃C���X�^���X�o�b�t�@�𐶐�����
	//-------------------------------------------------------------------
	bool TopAccelerationStructure::CreateInstanceBuffer(sl12::Device* pDevice, const TopInstanceDesc* pDescs, int descsCount)
	{
		if (!pDevice)
			return false;
		if (descsCount <= 0)
			return false;

		sl12::SafeDelete(pInstanceBuffer_);
		pInstanceBuffer_ = new sl12::Buffer();
		if (!pInstanceBuffer_->Initialize(pDevice, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * descsCount, 0, sl12::BufferUsage::ShaderResource, true, false))
		{
			return false;
		}

		auto p = reinterpret_cast<D3D12_RAYTRACING_INSTANCE_DESC*>(pInstanceBuffer_->Map(nullptr));
		for (int i = 0; i < descsCount; i++)
		{
			p[i] = pDescs[i].dxrDesc;
		}
		pInstanceBuffer_->Unmap();

		return true;
	}

	//-------------------------------------------------------------------
	// Top AS�̃r���h�R�}���h�𔭍s����
	//-------------------------------------------------------------------
	bool TopAccelerationStructure::Build(sl12::CommandList* pCmdList, const StructureInputDesc& desc, bool barrier)
	{
		if (!pCmdList)
			return false;
		if (!pInstanceBuffer_)
			return false;
		if (!pDxrBuffer_ || !pDxrBuffer_->GetResourceDep())
			return false;
		if (!pScratchBuffer_)
			return false;

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc{};
		buildDesc.DestAccelerationStructureData = pDxrBuffer_->GetResourceDep()->GetGPUVirtualAddress();
		buildDesc.Inputs = desc.inputDesc;
		buildDesc.Inputs.InstanceDescs = pInstanceBuffer_->GetResourceDep()->GetGPUVirtualAddress();
		buildDesc.ScratchAccelerationStructureData = pScratchBuffer_->GetResourceDep()->GetGPUVirtualAddress();

		// �r���h�R�}���h
		pCmdList->GetDxrCommandList()->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

		// �o���A
		if (barrier)
		{
			// TopAS�r���h�O��BottomAS�̃r���h������҂K�v������܂�.
			// ���\�[�X�o���A�𒣂邱�Ƃ�BottomAS�r���h���������Ă��邱�Ƃ�ۏ؂��܂�.
			pCmdList->UAVBarrier(pDxrBuffer_);
		}

		return true;
	}

	//-------------------------------------------------------------------
	// �j������
	//-------------------------------------------------------------------
	void TopAccelerationStructure::Destroy()
	{
		AccelerationStructure::Destroy();
		DestroyInstanceBuffer();
		descInfo_.Free();
	}

	//-------------------------------------------------------------------
	// �C���X�^���X�o�b�t�@�̂ݔj������
	//-------------------------------------------------------------------
	void TopAccelerationStructure::DestroyInstanceBuffer()
	{
		sl12::SafeDelete(pInstanceBuffer_);
	}

}	// namespace sl12


//	EOF
