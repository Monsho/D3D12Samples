#include "sl12/resource_texture.h"
#include "sl12/device.h"
#include "sl12/file.h"
#include "sl12/string_util.h"
#include "sl12/command_list.h"
#include <cctype>


namespace sl12
{

	namespace
	{
		struct TexInitRenderCommand
			: public IRenderCommand
		{
			std::unique_ptr<DirectX::ScratchImage>	image;
			Device*									pDevice;
			Texture*								pTexture;

			~TexInitRenderCommand()
			{
			}

			void LoadCommand(CommandList* pCmdlist) override
			{
				ID3D12Resource* pSrcImage = nullptr;
				if (!pTexture->UpdateImage(pDevice, pCmdlist, *image, &pSrcImage))
				{
					return;
				}
				pDevice->PendingKill(new ReleaseObjectItem<ID3D12Resource>(pSrcImage));

				pCmdlist->TransitionBarrier(pTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
			}
		};	// struct TexInitRenderCommand
	}

	//----------------
	ResourceItemTexture::~ResourceItemTexture()
	{
		textureView_.Destroy();
		texture_.Destroy();
	}

	//----------------
	ResourceItemBase* ResourceItemTexture::LoadFunction(ResourceLoader* pLoader, const std::string& filepath)
	{
		auto device = pLoader->GetDevice();

		std::unique_ptr<ResourceItemTexture> ret(new ResourceItemTexture());

		// check ext.
		// NOTE: support .png, .tga, .exr, or .hdr.
		auto ext = GetExtent(filepath);
		if (ext != ".png" && ext != ".dds" && ext != ".tga" && ext != ".exr" && ext != ".hdr")
		{
			return nullptr;
		}

		// load file.
		File texBin;
		if (!texBin.ReadFile(filepath.c_str()))
		{
			return nullptr;
		}

		// check need srgb.
		bool isSrgb = false;
		if ((filepath.find(".bc.") != std::string::npos) || (filepath.find("_baseColor") != std::string::npos))
		{
			isSrgb = true;
		}

		// create texture and view.
		std::unique_ptr<DirectX::ScratchImage> image;
		if (ext == ".png")
		{
			image = ret->texture_.InitializeFromPNGwoLoad(device, texBin.GetData(), texBin.GetSize(), 0);
		}
		else if (ext == ".dds")
		{
			image = ret->texture_.InitializeFromDDSwoLoad(device, texBin.GetData(), texBin.GetSize(), 0);
		}
		else if (ext == ".tga")
		{
			image = ret->texture_.InitializeFromTGAwoLoad(device, texBin.GetData(), texBin.GetSize(), 0);
		}
		else if (ext == ".exr")
		{
			image = ret->texture_.InitializeFromEXRwoLoad(device, texBin.GetData(), texBin.GetSize(), 0);
		}
		else if (ext == ".hdr")
		{
			image = ret->texture_.InitializeFromHDRwoLoad(device, texBin.GetData(), texBin.GetSize(), 0);
		}
		if (image.get() == nullptr)
		{
			return nullptr;
		}
		if (!ret->texture_.InitializeFromDXImage(device, *image, isSrgb))
		{
			return false;
		}

		ret->textureView_.Initialize(device, &ret->texture_);

		TexInitRenderCommand* command = new TexInitRenderCommand();
		command->image = std::move(image);
		command->pDevice = device;
		command->pTexture = &ret->texture_;

		device->AddRenderCommand(std::unique_ptr<IRenderCommand>(command));
		return ret.release();
	}


}	// namespace sl12


//	EOF
