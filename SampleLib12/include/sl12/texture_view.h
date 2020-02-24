#pragma once

#include <sl12/util.h>
#include <sl12/descriptor_heap.h>
#include <DirectXTex.h>


namespace sl12
{
	class Device;
	class Texture;
	class Buffer;

	//----------------------------------------------------------------------------
	class TextureView
	{
	public:
		TextureView()
		{}
		~TextureView()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, Texture* pTex, u32 firstMip = 0, u32 mipCount = 0, u32 firstArray = 0, u32 arraySize = 0);
		void Destroy();

		// getter
		DescriptorInfo& GetDescInfo() { return descInfo_; }
		const DescriptorInfo& GetDescInfo() const { return descInfo_; }

	private:
		DescriptorInfo	descInfo_;
	};	// class TextureView


	//----------------------------------------------------------------------------
	class RenderTargetView
	{
	public:
		RenderTargetView()
		{}
		~RenderTargetView()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, Texture* pTex, u32 mipSlice = 0, u32 firstArray = 0, u32 arraySize = 1);
		void Destroy();

		// getter
		DescriptorInfo& GetDescInfo() { return descInfo_; }
		const DescriptorInfo& GetDescInfo() const { return descInfo_; }
		DXGI_FORMAT	GetFormat() { return format_; }

	private:
		DescriptorInfo	descInfo_;
		DXGI_FORMAT		format_{ DXGI_FORMAT_UNKNOWN };
	};	// class RenderTargetView
	

	//----------------------------------------------------------------------------
	class DepthStencilView
	{
	public:
		DepthStencilView()
		{}
		~DepthStencilView()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, Texture* pTex, u32 mipSlice = 0, u32 firstArray = 0, u32 arraySize = 1);
		void Destroy();

		// getter
		DescriptorInfo& GetDescInfo() { return descInfo_; }
		const DescriptorInfo& GetDescInfo() const { return descInfo_; }
		DXGI_FORMAT	GetFormat() { return format_; }

	private:
		DescriptorInfo	descInfo_;
		DXGI_FORMAT		format_{ DXGI_FORMAT_UNKNOWN };
	};	// class DepthStencilView


	//----------------------------------------------------------------------------
	class UnorderedAccessView
	{
	public:
		UnorderedAccessView()
		{}
		~UnorderedAccessView()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, Texture* pTex, u32 mipSlice = 0, u32 firstArray = 0, u32 arraySize = 1);
		bool Initialize(Device* pDev, Buffer* pBuff, u32 firstElement, u32 numElement, u32 stride, u64 offset);
		void Destroy();

		// getter
		DescriptorInfo& GetDescInfo() { return descInfo_; }
		const DescriptorInfo& GetDescInfo() const { return descInfo_; }

	private:
		DescriptorInfo	descInfo_;
	};	// class UnorderdAccessView

}	// namespace sl12

//	EOF
