#pragma once

#include <sl12/device.h>
#include <sl12/command_list.h>
#include <sl12/buffer.h>
#include <vector>


namespace sl12
{
	/***************************************//**
	 * @brief Geometry description for bottom acceleration structure
	*******************************************/
	struct GeometryStructureDesc
	{
		D3D12_RAYTRACING_GEOMETRY_DESC	dxrDesc;

		void InitializeAsTriangle(
			D3D12_RAYTRACING_GEOMETRY_FLAGS	flags,
			sl12::Buffer*		pVertexBuffer,
			sl12::Buffer*		pIndexBuffer,
			sl12::Buffer*		pTransformBuffer,
			UINT64				vertexStride,
			UINT				vertexCount,
			DXGI_FORMAT			vertexFormat,
			UINT				indexCount,
			DXGI_FORMAT			indexFormat);

		void InitializeAsTriangle(
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
			DXGI_FORMAT			indexFormat);

		void InitializeAsAABB(
			D3D12_RAYTRACING_GEOMETRY_FLAGS	flags,
			sl12::Buffer*		pAABBBuffer,
			UINT64				bufferStride,
			UINT				aabbCount);
	};	// struct GeometryStructureDesc

	/***************************************//**
	 * @brief Acceleration structure input description
	*******************************************/
	struct StructureInputDesc
	{
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS	inputDesc;
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO	prebuildInfo;
		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>				geos;

		bool InitializeAsTop(
			sl12::Device*										pDevice,
			UINT												instanceCount,
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS	flags);

		bool InitializeAsBottom(
			sl12::Device*										pDevice,
			GeometryStructureDesc*								pGeos,
			UINT												geosCount,
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS	flags);
	};	// struct StructureInputDesc

	/***************************************//**
	 * @brief Instance description for top acceleration structure
	*******************************************/
	struct TopInstanceDesc
	{
		D3D12_RAYTRACING_INSTANCE_DESC	dxrDesc;

		void Initialize(
			const DirectX::XMFLOAT4X4&			transform,
			UINT								id,
			UINT								mask,
			UINT								contribution,
			UINT								flags,
			class BottomAccelerationStructure*	pBottomAS);
		void Initialize(
			const DirectX::XMFLOAT4X4&			transform,
			class BottomAccelerationStructure*	pBottomAS);
		void Initialize(
			class BottomAccelerationStructure*	pBottomAS);

	};	// struct InstanceDesc

	/***************************************//**
	 * @brief Acceleration structure
	*******************************************/
	class AccelerationStructure
	{
	public:
		AccelerationStructure()
		{}
		virtual ~AccelerationStructure()
		{
			Destroy();
		}

		virtual bool CreateBuffer(
			Device*		pDevice,
			size_t		size,
			size_t		scratchSize);

		void SetScratchBuffer(Buffer* p);

		virtual void Destroy();
		void DestroyScratchBuffer();

		Buffer& GetDxrBuffer()
		{
			return *pDxrBuffer_;
		}
		const Buffer& GetDxrBuffer() const
		{
			return *pDxrBuffer_;
		}
		Buffer* GetScratchBufferPtr()
		{
			return pScratchBuffer_;
		}

	protected:
		Buffer*		pDxrBuffer_ = nullptr;
		Buffer*		pScratchBuffer_ = nullptr;
		bool		scratchCreated_ = false;
	};	// class AccelerationStructure

	/***************************************//**
	 * @brief Bottom acceleration structure
	*******************************************/
	class BottomAccelerationStructure
		: public AccelerationStructure
	{
	public:
		BottomAccelerationStructure()
		{}
		~BottomAccelerationStructure()
		{
			Destroy();
		}

		bool Build(Device* pDevice, CommandList* pCmdList, const StructureInputDesc& desc, bool barrier = true);

		bool CompactAS(Device* pDevice, CommandList* pCmdList, bool barrier = true);

	private:
		Buffer*		pPostBuildReadBuffer_ = nullptr;
	};	// class BottomAccelerationStructure

	/***************************************//**
	 * @brief Top acceleration structure
	*******************************************/
	class TopAccelerationStructure
		: public AccelerationStructure
	{
	public:
		TopAccelerationStructure()
		{}
		~TopAccelerationStructure()
		{
			Destroy();
		}

		bool CreateBuffer(
			sl12::Device*			pDevice,
			size_t					size,
			size_t					scratchSize) override;

		bool CreateInstanceBuffer(sl12::Device* pDevice, const TopInstanceDesc* pDescs, int descsCount);

		bool Build(sl12::CommandList* pCmdList, const StructureInputDesc& desc, bool barrier = true);

		void Destroy();
		void DestroyInstanceBuffer();

		Buffer* TransferInstanceBuffer()
		{
			auto ret = pInstanceBuffer_;
			pInstanceBuffer_ = nullptr;
			return ret;
		}

		DescriptorInfo& GetDescInfo() { return descInfo_; }

	private:
		Buffer*			pInstanceBuffer_ = nullptr;
		DescriptorInfo	descInfo_;
	};	// class TopAccelerationStructure

}	// namespace sl12


//	EOF
