#pragma once

#include <sl12/util.h>
#include <DirectXTex.h>


namespace sl12
{
	class Device;
	class Descriptor;
	class Texture;

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

		bool Initialize(Device* pDev, Texture* pTex);
		void Destroy();

		// getter
		Descriptor* GetDesc() { return pDesc_; }

	private:
		Descriptor*	pDesc_{ nullptr };
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
		Descriptor* GetDesc() { return pDesc_; }

	private:
		Descriptor*	pDesc_{ nullptr };
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
		Descriptor* GetDesc() { return pDesc_; }

	private:
		Descriptor*	pDesc_{ nullptr };
	};	// class DepthStencilView

}	// namespace sl12

//	EOF
