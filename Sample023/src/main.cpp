#include <sl12/device.h>
#include <sl12/swapchain.h>
#include <sl12/command_queue.h>
#include <sl12/command_list.h>
#include <sl12/descriptor_heap.h>
#include <sl12/descriptor.h>
#include <sl12/texture.h>
#include <sl12/texture_view.h>
#include <sl12/sampler.h>
#include <sl12/fence.h>
#include <sl12/buffer.h>
#include <sl12/buffer_view.h>
#include <sl12/shader.h>
#include <sl12/gui.h>
#include <sl12/pipeline_state.h>
#include <sl12/root_signature.h>
#include <sl12/descriptor_set.h>
#include <sl12/resource_loader.h>
#include <sl12/resource_texture.h>
#include <sl12/application.h>
#include <DirectXTex.h>
#include <d3dcompiler.h>
#include <d3d12shader.h>
#include <windowsx.h>

#include "OpenColorIO.h"

#include "CompiledShaders/fullscreen.vv.hlsl.h"
#include "CompiledShaders/texture_view.p.hlsl.h"


namespace OCIO = OCIO_NAMESPACE;

namespace
{
	static const int kScreenWidth = 1920;
	static const int kScreenHeight = 1080;
}

class SampleApplication
	: public sl12::Application
{
public:
	SampleApplication(HINSTANCE hInstance, int nCmdShow, int screenWidth, int screenHeight)
		: Application(hInstance, nCmdShow, screenWidth, screenHeight)
	{}

	bool Initialize() override
	{
		// load texture.
		if (!resLoader_.Initialize(&device_))
		{
			return false;
		}
		resTexHandle_ = resLoader_.LoadRequest<sl12::ResourceItemTexture>("data/MtTamWest.exr");

		// create command lists.
		if (!mainCmdLists_.Initialize(&device_, &device_.GetGraphicsQueue()))
		{
			return false;
		}

		// create sampler.
		{
			D3D12_SAMPLER_DESC desc{};
			desc.AddressU = desc.AddressV = desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			desc.MinLOD = 0.0f;
			desc.MaxLOD = FLT_MAX;

			if (!sampler_.Initialize(&device_, desc))
			{
				return false;
			}
		}

		// create shaders.
		if (!vshader_.Initialize(&device_, sl12::ShaderType::Vertex, g_FullscreenVVShader, sizeof(g_FullscreenVVShader)))
		{
			return false;
		}
		if (!pshader_.Initialize(&device_, sl12::ShaderType::Pixel, g_TextureViewPShader, sizeof(g_TextureViewPShader)))
		{
			return false;
		}

		// create texture view pso.
		{
			if (!rsTextureView_.Initialize(&device_, &vshader_, &pshader_, nullptr, nullptr, nullptr))
			{
				return false;
			}

			sl12::GraphicsPipelineStateDesc	desc{};
			desc.pRootSignature = &rsTextureView_;
			desc.pVS = &vshader_;
			desc.pPS = &pshader_;

			desc.blend.sampleMask = UINT_MAX;
			desc.blend.rtDesc[0].isBlendEnable = false;
			desc.blend.rtDesc[0].blendOpColor = D3D12_BLEND_OP_ADD;
			desc.blend.rtDesc[0].srcBlendColor = D3D12_BLEND_SRC_ALPHA;
			desc.blend.rtDesc[0].dstBlendColor = D3D12_BLEND_INV_SRC_ALPHA;
			desc.blend.rtDesc[0].blendOpAlpha = D3D12_BLEND_OP_ADD;
			desc.blend.rtDesc[0].srcBlendAlpha = D3D12_BLEND_ONE;
			desc.blend.rtDesc[0].dstBlendAlpha = D3D12_BLEND_ZERO;
			desc.blend.rtDesc[0].writeMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			desc.rasterizer.cullMode = D3D12_CULL_MODE_NONE;
			desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
			desc.rasterizer.isDepthClipEnable = false;
			desc.rasterizer.isFrontCCW = false;

			desc.depthStencil.isDepthEnable = false;
			desc.depthStencil.isDepthWriteEnable = false;
			desc.depthStencil.depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

			desc.inputLayout.numElements = 0;
			desc.inputLayout.pElements = nullptr;

			desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			desc.numRTVs = 0;
			desc.rtvFormats[desc.numRTVs++] = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.dsvFormat = DXGI_FORMAT_UNKNOWN;
			desc.multisampleCount = 1;

			if (!psoTextureView_.Initialize(&device_, desc))
			{
				return false;
			}
		}

		// read config.
		if (!ReadOCIOConfig())
		{
			return false;
		}

		// create luts.
		auto&& cmd_list = mainCmdLists_.Reset();
		if (!CreateOCIOLut(&cmd_list))
		{
			return false;
		}

		// create gui.
		if (!gui_.Initialize(&device_, DXGI_FORMAT_R8G8B8A8_UNORM))
		{
			return false;
		}
		if (!gui_.CreateFontImage(&device_, cmd_list))
		{
			return false;
		}

		mainCmdLists_.Close();
		mainCmdLists_.Execute();
		device_.WaitDrawDone();

		// wait loaded.
		while (resLoader_.IsLoading())
		{
			Sleep(1);
		}

		return true;
	}

	bool Execute() override
	{
		const int kSwapchainBufferOffset = 1;
		auto frameIndex = (device_.GetSwapchain().GetFrameIndex() + sl12::Swapchain::kMaxBuffer - 1) % sl12::Swapchain::kMaxBuffer;
		auto prevFrameIndex = (device_.GetSwapchain().GetFrameIndex() + sl12::Swapchain::kMaxBuffer - 2) % sl12::Swapchain::kMaxBuffer;

		// begin new frame.
		device_.WaitPresent();
		device_.SyncKillObjects();

		// load device commands.
		auto&& cmd_list = mainCmdLists_.Reset();
		gui_.BeginNewFrame(&cmd_list, kScreenWidth, kScreenHeight, inputData_);
		device_.LoadRenderCommands(&cmd_list);

		// GUI.
		static bool s_Raw = true;
		if (ImGui::RadioButton("Raw", s_Raw))
		{
			s_Raw = true;
		}
		if (ImGui::RadioButton("ACES 1.03", !s_Raw))
		{
			s_Raw = false;
		}
		std::vector<const char*> viewStrings;
		for (auto&& v : ocioViews_) viewStrings.push_back(v.c_str());
		if (ImGui::Combo("View", &ocioViewIndex_, viewStrings.data(), viewStrings.size()))
		{
			if (!CreateOCIOLut(&cmd_list))
			{
				return false;
			}
		}

		// begin draw.
		auto&& swapchain = device_.GetSwapchain();
		cmd_list.TransitionBarrier(swapchain.GetCurrentTexture(kSwapchainBufferOffset), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		{
			float color[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
			cmd_list.GetCommandList5()->ClearRenderTargetView(swapchain.GetCurrentRenderTargetView(kSwapchainBufferOffset)->GetDescInfo().cpuHandle, color, 0, nullptr);
		}

		// set viewport.
		D3D12_VIEWPORT viewport{ 0.0f, 0.0f, (float)kScreenWidth, (float)kScreenHeight, 0.0f, 1.0f };
		D3D12_RECT scissor{ 0, 0, kScreenWidth, kScreenHeight };
		cmd_list.GetCommandList5()->RSSetViewports(1, &viewport);
		cmd_list.GetCommandList5()->RSSetScissorRects(1, &scissor);

		// set render target.
		cmd_list.GetCommandList5()->OMSetRenderTargets(1, &swapchain.GetCurrentDescHandle(kSwapchainBufferOffset), false, nullptr);

		// draw texture.
		if (s_Raw)
		{
			dset_.Reset();
			dset_.SetPsSrv(0, resTexHandle_.GetItem<sl12::ResourceItemTexture>()->GetTextureView().GetDescInfo().cpuHandle);
			dset_.SetPsSampler(0, sampler_.GetDescInfo().cpuHandle);

			cmd_list.GetCommandList5()->SetPipelineState(psoTextureView_.GetPSO());
			cmd_list.SetGraphicsRootSignatureAndDescriptorSet(&rsTextureView_, &dset_);

			cmd_list.GetCommandList5()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmd_list.GetCommandList5()->DrawInstanced(3, 1, 0, 0);
		}
		else
		{
			dset_.Reset();
			dset_.SetPsSrv(0, resTexHandle_.GetItem<sl12::ResourceItemTexture>()->GetTextureView().GetDescInfo().cpuHandle);
			dset_.SetPsSampler(0, sampler_.GetDescInfo().cpuHandle);
			for (auto&& v : lut3Ds_)
			{
				if (v->textureSlotIndex >= 0)
					dset_.SetPsSrv(v->textureSlotIndex, v->view.GetDescInfo().cpuHandle);
				if (v->samplerSlotIndex >= 0)
					dset_.SetPsSampler(v->samplerSlotIndex, sampler_.GetDescInfo().cpuHandle);
			}
			for (auto&& v : lut2Ds_)
			{
				if (v->textureSlotIndex >= 0)
					dset_.SetPsSrv(v->textureSlotIndex, v->view.GetDescInfo().cpuHandle);
				if (v->samplerSlotIndex >= 0)
					dset_.SetPsSampler(v->samplerSlotIndex, sampler_.GetDescInfo().cpuHandle);
			}

			cmd_list.GetCommandList5()->SetPipelineState(pPsoOCIOView_->GetPSO());
			cmd_list.SetGraphicsRootSignatureAndDescriptorSet(pRsOCIOView_, &dset_);

			cmd_list.GetCommandList5()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmd_list.GetCommandList5()->DrawInstanced(3, 1, 0, 0);
		}

		// draw GUI.
		ImGui::Render();

		// end draw.
		cmd_list.TransitionBarrier(swapchain.GetCurrentTexture(kSwapchainBufferOffset), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		mainCmdLists_.Close();

		// to next frame.
		device_.WaitDrawDone();
		device_.Present(1);

		// execute commands.
		mainCmdLists_.Execute();

		return true;
	}

	void Finalize() override
	{
		// wait draw end.
		device_.WaitDrawDone();
		device_.Present(1);

		sl12::SafeDelete(pOCIOPixelShader_);
		sl12::SafeDelete(pRsOCIOView_);
		sl12::SafeDelete(pPsoOCIOView_);

		sampler_.Destroy();
		for (auto&& v : lut3Ds_) delete v;
		for (auto&& v : lut2Ds_) delete v;
		lut3Ds_.clear();
		lut2Ds_.clear();

		psoTextureView_.Destroy();
		rsTextureView_.Destroy();
		vshader_.Destroy();
		pshader_.Destroy();

		resLoader_.Destroy();
		mainCmdLists_.Destroy();
	}

	int Input(UINT message, WPARAM wParam, LPARAM lParam) override
	{
		switch (message)
		{
		case WM_LBUTTONDOWN:
			inputData_.mouseButton |= sl12::MouseButton::Left;
			return 0;
		case WM_RBUTTONDOWN:
			inputData_.mouseButton |= sl12::MouseButton::Right;
			return 0;
		case WM_MBUTTONDOWN:
			inputData_.mouseButton |= sl12::MouseButton::Middle;
			return 0;
		case WM_LBUTTONUP:
			inputData_.mouseButton &= ~sl12::MouseButton::Left;
			return 0;
		case WM_RBUTTONUP:
			inputData_.mouseButton &= ~sl12::MouseButton::Right;
			return 0;
		case WM_MBUTTONUP:
			inputData_.mouseButton &= ~sl12::MouseButton::Middle;
			return 0;
		case WM_MOUSEMOVE:
			inputData_.mouseX = GET_X_LPARAM(lParam);
			inputData_.mouseY = GET_Y_LPARAM(lParam);
			return 0;
		}

		return 0;
	}

private:
	bool ReadOCIOConfig()
	{
		ocioConfig_ = OCIO::Config::CreateFromFile("data/aces_1.0.3/config.ocio");
		if (ocioConfig_ == nullptr)
		{
			return false;
		}

		int n = ocioConfig_->getNumViews(ocioConfig_->getDisplay(0));
		for (int i = 0; i < n; i++)
		{
			ocioViews_.push_back(std::string(ocioConfig_->getView(ocioConfig_->getDisplay(0), i)));
		}

		FILE* fp;
		if (fopen_s(&fp, "data/ocio_view.p.hlsl", "r") != 0)
		{
			return false;
		}

		char str[4096];
		while (fgets(str, sizeof(str), fp))
		{
			ocioPixelShaderCode_ += str;
		}

		fclose(fp);

		ocioViewIndex_ = 0;

		return true;
	}

	bool CompileOCIOShader(const std::string& inFunc, std::vector<sl12::u8>& outBin)
	{
		class MyInclude : public ID3DInclude
		{
		public:
			MyInclude(const std::string& ocio)
				: ocioCode_(ocio)
			{
				pCode_ = ocioCode_.c_str();
			}

			HRESULT __stdcall Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes) override
			{
				*ppData = pCode_;
				*pBytes = ocioCode_.length();

				return S_OK;
			}
			HRESULT Close(LPCVOID pData) override
			{
				return S_OK;
			}

		private:
			std::string	ocioCode_;
			const char*	pCode_;
		};

		MyInclude incl(inFunc);

		ID3DBlob* pShaderBin = nullptr;
		ID3DBlob* pErrorCode = nullptr;

		D3D_SHADER_MACRO macros[] = { NULL, NULL };
		HRESULT hr = D3DCompile(
			ocioPixelShaderCode_.c_str(),
			ocioPixelShaderCode_.length(),
			"ocio_view.p.hlsl",
			macros,
			&incl,
			"main",
			"ps_5_0",
			D3DCOMPILE_OPTIMIZATION_LEVEL3,
			0,
			&pShaderBin,
			&pErrorCode);
		if (FAILED(hr))
		{
			return false;
		}

		outBin.resize(pShaderBin->GetBufferSize());
		memcpy(outBin.data(), pShaderBin->GetBufferPointer(), pShaderBin->GetBufferSize());

		sl12::SafeRelease(pShaderBin);
		sl12::SafeRelease(pErrorCode);

		return true;
	}

	bool CreateOCIOPipeline(const std::vector<sl12::u8>& shaderBin)
	{
		if (pOCIOPixelShader_)
		{
			device_.KillObject(pOCIOPixelShader_);
			pOCIOPixelShader_ = nullptr;
		}
		if (pRsOCIOView_)
		{
			device_.KillObject(pRsOCIOView_);
			pRsOCIOView_ = nullptr;
		}
		if (pPsoOCIOView_)
		{
			device_.KillObject(pPsoOCIOView_);
			pPsoOCIOView_ = nullptr;
		}

		pOCIOPixelShader_ = new sl12::Shader();
		pRsOCIOView_ = new sl12::RootSignature();
		pPsoOCIOView_ = new sl12::GraphicsPipelineState();

		// initialize pixel shader.
		if (!pOCIOPixelShader_->Initialize(&device_, sl12::ShaderType::Pixel, shaderBin.data(), shaderBin.size()))
		{
			return false;
		}

		// initialize pso.
		{
			if (!pRsOCIOView_->Initialize(&device_, &vshader_, pOCIOPixelShader_, nullptr, nullptr, nullptr))
			{
				return false;
			}

			sl12::GraphicsPipelineStateDesc	desc{};
			desc.pRootSignature = pRsOCIOView_;
			desc.pVS = &vshader_;
			desc.pPS = pOCIOPixelShader_;

			desc.blend.sampleMask = UINT_MAX;
			desc.blend.rtDesc[0].isBlendEnable = false;
			desc.blend.rtDesc[0].blendOpColor = D3D12_BLEND_OP_ADD;
			desc.blend.rtDesc[0].srcBlendColor = D3D12_BLEND_SRC_ALPHA;
			desc.blend.rtDesc[0].dstBlendColor = D3D12_BLEND_INV_SRC_ALPHA;
			desc.blend.rtDesc[0].blendOpAlpha = D3D12_BLEND_OP_ADD;
			desc.blend.rtDesc[0].srcBlendAlpha = D3D12_BLEND_ONE;
			desc.blend.rtDesc[0].dstBlendAlpha = D3D12_BLEND_ZERO;
			desc.blend.rtDesc[0].writeMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			desc.rasterizer.cullMode = D3D12_CULL_MODE_NONE;
			desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
			desc.rasterizer.isDepthClipEnable = false;
			desc.rasterizer.isFrontCCW = false;

			desc.depthStencil.isDepthEnable = false;
			desc.depthStencil.isDepthWriteEnable = false;
			desc.depthStencil.depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

			desc.inputLayout.numElements = 0;
			desc.inputLayout.pElements = nullptr;

			desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			desc.numRTVs = 0;
			desc.rtvFormats[desc.numRTVs++] = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.dsvFormat = DXGI_FORMAT_UNKNOWN;
			desc.multisampleCount = 1;

			if (!pPsoOCIOView_->Initialize(&device_, desc))
			{
				return false;
			}
		}

		return true;
	}

	bool CreateOCIOLut(sl12::CommandList* pCmdList)
	{
		// initialize transform.
		OCIO::DisplayTransformRcPtr transform = OCIO::DisplayTransform::Create();
		transform->setInputColorSpaceName(OCIO::ROLE_SCENE_LINEAR);
		transform->setDisplay(ocioConfig_->getDisplay(0));
		transform->setView(ocioConfig_->getView(ocioConfig_->getDisplay(0), ocioViewIndex_));

		// create gpu resources.
		OCIO::ConstProcessorRcPtr processor = ocioConfig_->getProcessor(transform);
		OCIO::GpuShaderDescRcPtr shaderDesc = OCIO::GpuShaderDesc::CreateShaderDesc();
		shaderDesc->setLanguage(OCIO::GPU_LANGUAGE_HLSL_DX11);
		shaderDesc->setFunctionName("OCIODisplay");

		OCIO::ConstGPUProcessorRcPtr gpuProcessor = processor->getDefaultGPUProcessor();
		gpuProcessor->extractGpuShaderInfo(shaderDesc);
		std::string shader_code = shaderDesc->getShaderText();		// shader code.

		// compile shader.
		std::vector<sl12::u8> shaderBin;
		if (!CompileOCIOShader(shader_code, shaderBin))
		{
			return false;
		}

		// create pipeline.
		if (!CreateOCIOPipeline(shaderBin))
		{
			return false;
		}

		// shader reflection.
		ID3D12ShaderReflection* pReflection;
		D3DReflect(shaderBin.data(), shaderBin.size(), IID_PPV_ARGS(&pReflection));

		// clear prev luts.
		for (auto&& v : lut3Ds_) delete v;
		for (auto&& v : lut2Ds_) delete v;
		lut3Ds_.clear();
		lut2Ds_.clear();

		// create 3d textures.
		auto tex3d_count = shaderDesc->getNum3DTextures();
		for (int i = 0; i < tex3d_count; i++)
		{
			const char* tex_name;
			const char* sam_name;
			const char* uid;
			sl12::u32 edgelen;
			OCIO::Interpolation interp;
			shaderDesc->get3DTexture(i, tex_name, sam_name, uid, edgelen, interp);

			// create shader resource texture.
			TextureSet* tset = new TextureSet();
			sl12::TextureDesc desc{};
			desc.dimension = sl12::TextureDimension::Texture3D;
			desc.format = DXGI_FORMAT_R32G32B32_FLOAT;
			desc.mipLevels = 1;
			desc.sampleCount = 1;
			desc.width = desc.height = desc.depth = edgelen;
			desc.initialState = D3D12_RESOURCE_STATE_COPY_DEST;
			if (!tset->texture.Initialize(&device_, desc))
			{
				return false;
			}
			if (!tset->view.Initialize(&device_, &tset->texture))
			{
				return false;
			}

			D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
			UINT num_rows;
			UINT64 row_size, total_size;
			device_.GetDeviceDep()->GetCopyableFootprints(&tset->texture.GetResourceDesc(), 0, 1, 0, &layout, &num_rows, &row_size, &total_size);

			// create copy source buffer.
			sl12::Buffer* pCopySrc = new sl12::Buffer();
			if (!pCopySrc->Initialize(&device_, total_size, 1, sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
			{
				return false;
			}

			const float* values = nullptr;
			shaderDesc->get3DTextureValues(i, values);
			sl12::u8* p;
			pCopySrc->GetResourceDep()->Map(0, nullptr, (void**)&p);
			for (sl12::u32 d = 0; d < edgelen; d++)
			{
				sl12::u8* page = p + layout.Footprint.RowPitch * num_rows * d;
				for (sl12::u32 h = 0; h < edgelen; h++)
				{
					memcpy(page, values + edgelen * edgelen * 3 * d + edgelen * 3 * h, edgelen * 3 * sizeof(float));
					page += layout.Footprint.RowPitch;
				}
			}
			pCopySrc->GetResourceDep()->Unmap(0, nullptr);

			// copy command.
			D3D12_TEXTURE_COPY_LOCATION src, dst;
			src.pResource = pCopySrc->GetResourceDep();
			src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			src.PlacedFootprint = layout;
			dst.pResource = tset->texture.GetResourceDep();
			dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dst.SubresourceIndex = 0;
			pCmdList->GetCommandList5()->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
			pCmdList->TransitionBarrier(&tset->texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);

			// find slot.
			tset->textureSlotName = tex_name;
			tset->samplerSlotName = sam_name;
			tset->textureSlotIndex = -1;
			tset->samplerSlotIndex = -1;
			D3D12_SHADER_INPUT_BIND_DESC bindDesc;
			if (SUCCEEDED(pReflection->GetResourceBindingDescByName(tex_name, &bindDesc)))
			{
				tset->textureSlotIndex = (int)bindDesc.BindPoint;
			}
			if (SUCCEEDED(pReflection->GetResourceBindingDescByName(sam_name, &bindDesc)))
			{
				tset->samplerSlotIndex = (int)bindDesc.BindPoint;
			}

			lut3Ds_.push_back(tset);

			device_.KillObject(pCopySrc);
		}

		// create 2d textures.
		auto tex_count = shaderDesc->getNumTextures();
		for (int i = 0; i < tex_count; i++)
		{
			const char* tex_name;
			const char* sam_name;
			const char* uid;
			unsigned int width, height;
			OCIO::GpuShaderCreator::TextureType type;
			OCIO::Interpolation interp;
			shaderDesc->getTexture(i, tex_name, sam_name, uid, width, height, type, interp);

			// create shader resource texture.
			TextureSet* tset = new TextureSet();
			sl12::TextureDesc desc{};
			desc.dimension = sl12::TextureDimension::Texture2D;
			desc.format = DXGI_FORMAT_R32G32B32_FLOAT;
			desc.mipLevels = 1;
			desc.sampleCount = 1;
			desc.width = width;
			desc.height = height;
			desc.depth = 1;
			desc.initialState = D3D12_RESOURCE_STATE_COPY_DEST;
			if (!tset->texture.Initialize(&device_, desc))
			{
				return false;
			}
			if (!tset->view.Initialize(&device_, &tset->texture))
			{
				return false;
			}

			D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
			UINT num_rows;
			UINT64 row_size, total_size;
			device_.GetDeviceDep()->GetCopyableFootprints(&tset->texture.GetResourceDesc(), 0, 1, 0, &layout, &num_rows, &row_size, &total_size);

			// create copy source buffer.
			sl12::Buffer* pCopySrc = new sl12::Buffer();
			if (!pCopySrc->Initialize(&device_, total_size, 1, sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
			{
				return false;
			}

			const float* values = nullptr;
			shaderDesc->getTextureValues(i, values);
			sl12::u8* p;
			pCopySrc->GetResourceDep()->Map(0, nullptr, (void**)&p);
			for (sl12::u32 h = 0; h < height; h++)
			{
				sl12::u8* page = p + layout.Footprint.RowPitch * h;
				memcpy(page, values + width * 3 * h, width * 3 * sizeof(float));
			}
			pCopySrc->GetResourceDep()->Unmap(0, nullptr);

			// copy command.
			D3D12_TEXTURE_COPY_LOCATION src, dst;
			src.pResource = pCopySrc->GetResourceDep();
			src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			src.PlacedFootprint = layout;
			dst.pResource = tset->texture.GetResourceDep();
			dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dst.SubresourceIndex = 0;
			pCmdList->GetCommandList5()->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
			pCmdList->TransitionBarrier(&tset->texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);

			// find slot.
			tset->textureSlotName = tex_name;
			tset->samplerSlotName = sam_name;
			tset->textureSlotIndex = -1;
			tset->samplerSlotIndex = -1;
			D3D12_SHADER_INPUT_BIND_DESC bindDesc;
			if (SUCCEEDED(pReflection->GetResourceBindingDescByName(tex_name, &bindDesc)))
			{
				tset->textureSlotIndex = (int)bindDesc.BindPoint;
			}
			if (SUCCEEDED(pReflection->GetResourceBindingDescByName(sam_name, &bindDesc)))
			{
				tset->samplerSlotIndex = (int)bindDesc.BindPoint;
			}

			lut2Ds_.push_back(tset);

			device_.KillObject(pCopySrc);
		}

		return true;
	}

private:
	static const int kBufferCount = sl12::Swapchain::kMaxBuffer;

	struct CommandLists
	{
		sl12::CommandList	cmdLists[kBufferCount];
		int					index = 0;

		bool Initialize(sl12::Device* pDev, sl12::CommandQueue* pQueue)
		{
			for (auto&& v : cmdLists)
			{
				if (!v.Initialize(pDev, pQueue, true))
				{
					return false;
				}
			}
			index = 0;
			return true;
		}

		void Destroy()
		{
			for (auto&& v : cmdLists) v.Destroy();
		}

		sl12::CommandList& Reset()
		{
			index = (index + 1) % kBufferCount;
			auto&& ret = cmdLists[index];
			ret.Reset();
			return ret;
		}

		void Close()
		{
			cmdLists[index].Close();
		}

		void Execute()
		{
			cmdLists[index].Execute();
		}

		sl12::CommandQueue* GetParentQueue()
		{
			return cmdLists[index].GetParentQueue();
		}
	};	// struct CommandLists

	struct TextureSet
	{
		sl12::Texture		texture;
		sl12::TextureView	view;
		std::string			textureSlotName;
		std::string			samplerSlotName;
		int					textureSlotIndex;
		int					samplerSlotIndex;

		~TextureSet()
		{
			view.Destroy();
			texture.Destroy();
		}
	};	// struct TextureSet

	sl12::ResourceLoader	resLoader_;
	sl12::ResourceHandle	resTexHandle_;

	std::vector<TextureSet*>	lut3Ds_;
	std::vector<TextureSet*>	lut2Ds_;
	sl12::Sampler				sampler_;

	sl12::Shader			vshader_, pshader_;

	sl12::RootSignature			rsTextureView_;
	sl12::GraphicsPipelineState	psoTextureView_;

	sl12::Shader*					pOCIOPixelShader_ = nullptr;
	sl12::RootSignature*			pRsOCIOView_ = nullptr;
	sl12::GraphicsPipelineState*	pPsoOCIOView_ = nullptr;

	sl12::DescriptorSet			dset_;

	sl12::Gui				gui_;
	sl12::InputData			inputData_{};

	OCIO::ConstConfigRcPtr		ocioConfig_;
	std::vector<std::string>	ocioViews_;
	std::string					ocioPixelShaderCode_;
	int							ocioViewIndex_ = 0;

	CommandLists			mainCmdLists_;
};	// class SampleApplication

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	SampleApplication app(hInstance, nCmdShow, kScreenWidth, kScreenHeight);

	return app.Run();
}

//	EOF
