#pragma once

#include <sl12/util.h>


namespace sl12
{
	class Device;
	class CommandList;

	struct BufferUsage
	{
		enum Type
		{
			ConstantBuffer,
			VertexBuffer,
			IndexBuffer,
			ShaderResource,
			AccelerationStructure,
			ReadBack,

			Max
		};
	};	// struct BufferUsage

	class Buffer
	{
		friend class CommandList;

	public:
		Buffer()
		{}
		~Buffer()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, size_t size, size_t stride, BufferUsage::Type type, bool isDynamic, bool isUAV);
		bool Initialize(Device* pDev, size_t size, size_t stride, BufferUsage::Type type, D3D12_RESOURCE_STATES initialState, bool isDynamic, bool isUAV);
		void Destroy();

		void UpdateBuffer(Device* pDev, CommandList* pCmdList, const void* pData, size_t size, size_t offset = 0);

		void* Map(CommandList*);
		void* Map();
		void* Map(const D3D12_RANGE& range);
		void Unmap();
		void Unmap(const D3D12_RANGE& range);

		// getter
		ID3D12Resource* GetResourceDep() { return pResource_; }
		const D3D12_RESOURCE_DESC& GetResourceDesc() const { return resourceDesc_; }
		size_t GetSize() const { return size_; }
		size_t GetStride() const { return stride_; }
		BufferUsage::Type GetBufferUsage() const { return bufferUsage_; }
		bool IsUAV() const { return isUAV_; }

	private:
		ID3D12Resource*			pResource_ = nullptr;
		D3D12_HEAP_PROPERTIES	heapProp_ = {};
		D3D12_RESOURCE_DESC		resourceDesc_ = {};
		size_t					size_ = 0;
		size_t					stride_ = 0;
		BufferUsage::Type		bufferUsage_ = BufferUsage::Max;
		bool					isUAV_ = false;
		D3D12_RESOURCE_STATES	currentState_ = D3D12_RESOURCE_STATE_COMMON;
	};	// class Buffer

}	// namespace sl12

//	EOF
