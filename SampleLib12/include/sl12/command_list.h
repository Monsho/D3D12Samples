#pragma once

#include <sl12/util.h>


namespace sl12
{
	class Device;
	class CommandQueue;
	class Texture;
	class Buffer;
	class DescriptorStackList;
	class SamplerDescriptorCache;
	class RootSignature;
	class DescriptorSet;

	class CommandList
	{
	public:
		CommandList()
		{}
		~CommandList()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, CommandQueue* pQueue, bool forDxr = false);
		void Destroy();

		void Reset();

		void Close();

		void Execute();

		void TransitionBarrier(Texture* p, D3D12_RESOURCE_STATES nextState);
		void TransitionBarrier(Buffer* p, D3D12_RESOURCE_STATES nextState);

		void TransitionBarrier(Texture* p, D3D12_RESOURCE_STATES prevState, D3D12_RESOURCE_STATES nextState);
		void TransitionBarrier(Buffer* p, D3D12_RESOURCE_STATES prevState, D3D12_RESOURCE_STATES nextState);

		void UAVBarrier(Texture* p);
		void UAVBarrier(Buffer* p);

		void SetDescriptorHeapDirty()
		{
			changeHeap_ = true;
		}
		void SetGraphicsRootSignatureAndDescriptorSet(RootSignature* pRS, DescriptorSet* pDSet);
		void SetComputeRootSignatureAndDescriptorSet(RootSignature* pRS, DescriptorSet* pDSet);

		// getter
		CommandQueue* GetParentQueue() { return pParentQueue_; }
		DescriptorStackList* GetViewDescriptorStack() { return pViewDescStack_; };
		SamplerDescriptorCache* GetSamplerDescriptorCache() { return pSamplerDescCache_; };
		ID3D12CommandAllocator* GetCommandAllocator() { return pCmdAllocator_; }
		ID3D12GraphicsCommandList* GetCommandList() { return pCmdList_; };
		ID3D12GraphicsCommandList4* GetDxrCommandList() { return pDxrCmdList_; };

	private:
		Device*						pParentDevice_{ nullptr };
		CommandQueue*				pParentQueue_{ nullptr };
		DescriptorStackList*		pViewDescStack_{ nullptr };
		SamplerDescriptorCache*		pSamplerDescCache_{ nullptr };
		ID3D12CommandAllocator*		pCmdAllocator_{ nullptr };
		ID3D12GraphicsCommandList*	pCmdList_{ nullptr };
		ID3D12GraphicsCommandList4*	pDxrCmdList_{ nullptr };

		ID3D12DescriptorHeap*		pCurrentSamplerHeap_{ nullptr };
		ID3D12DescriptorHeap*		pPrevSamplerHeap_{ nullptr };
		bool						changeHeap_{ true };
	};	// class CommandList

}	// namespace sl12

//	EOF
