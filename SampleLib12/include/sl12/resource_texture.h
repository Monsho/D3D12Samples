#pragma once

#include "sl12/types.h"
#include "sl12/resource_loader.h"
#include "sl12/texture.h"
#include "sl12/texture_view.h"


namespace sl12
{

	class ResourceItemTexture
		: public ResourceItemBase
	{
	public:
		static const u32 kType = TYPE_FOURCC("TEX_");

		~ResourceItemTexture();

		Texture& GetTexture()
		{
			return texture_;
		}
		const Texture& GetTexture() const
		{
			return texture_;
		}
		TextureView& GetTextureView()
		{
			return textureView_;
		}
		const TextureView& GetTextureView() const
		{
			return textureView_;
		}


		static ResourceItemBase* LoadFunction(ResourceLoader* pLoader, const std::string& filepath);

	private:
		ResourceItemTexture()
			: ResourceItemBase(ResourceItemTexture::kType)
		{}

	private:
		Texture			texture_;
		TextureView		textureView_;
	};	// class ResourceItemMesh

}	// namespace sl12


//	EOF
