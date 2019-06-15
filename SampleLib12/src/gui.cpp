#include <sl12/gui.h>
#include <sl12/device.h>
#include <sl12/command_list.h>
#include <sl12/texture.h>
#include <sl12/texture_view.h>
#include <sl12/buffer.h>
#include <sl12/buffer_view.h>
#include <sl12/sampler.h>
#include <sl12/shader.h>
#include <sl12/descriptor.h>
#include <sl12/descriptor_heap.h>
#include <sl12/swapchain.h>
#include <sl12/root_signature.h>
#include <sl12/pipeline_state.h>
#include <sl12/descriptor_set.h>
#include <sl12/VSGui.h>
#include <sl12/PSGui.h>


namespace sl12
{
	namespace
	{
		static const u32	kMaxFrameCount = Swapchain::kMaxBuffer;

		struct VertexUniform
		{
			float	scale_[2];
			float	translate_[2];
		};	// struct VertexUniform

		struct ImDrawVertex
		{
			float	pos_[2];
			float	uv_[2];
			u32		color_;
		};	// struct ImDrawVertex

	}	// namespace


	Gui* Gui::guiHandle_ = nullptr;

	//----
	// 初期化
	bool Gui::Initialize(Device* pDevice, DXGI_FORMAT rtFormat, DXGI_FORMAT dsFormat)
	{
		Destroy();

		if (guiHandle_)
		{
			return false;
		}

		pOwner_ = pDevice;
		guiHandle_ = this;

		// imguiのコンテキスト作成
		ImGui::CreateContext();

		// コールバックの登録
		ImGuiIO& io = ImGui::GetIO();
		io.RenderDrawListsFn = &Gui::RenderDrawList;

		// シェーダ作成
		pVShader_ = new Shader();
		pPShader_ = new Shader();
		if (!pVShader_ || !pPShader_)
		{
			return false;
		}
		if (!pVShader_->Initialize(pDevice, ShaderType::Vertex, kVSGui, sizeof(kVSGui)))
		{
			return false;
		}
		if (!pPShader_->Initialize(pDevice, ShaderType::Pixel, kPSGui, sizeof(kPSGui)))
		{
			return false;
		}

		// サンプラ作成
		{
			pFontSampler_ = new Sampler();
			if (!pFontSampler_)
			{
				return false;
			}
			D3D12_SAMPLER_DESC desc{};
			desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			if (!pFontSampler_->Initialize(pDevice, desc))
			{
				return false;
			}
		}

		// ルートシグニチャ作成
		{
			pRootSig_ = new RootSignature();
			if (!pRootSig_->Initialize(pDevice, pVShader_, pPShader_, nullptr, nullptr, nullptr))
			{
				return false;
			}

			pDescSet_ = new DescriptorSet();
		}

		// パイプラインステート作成
		{
			D3D12_INPUT_ELEMENT_DESC elementDescs[] = {
				{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 0,                 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, sizeof(float) * 2, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, sizeof(float) * 4, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			};

			GraphicsPipelineStateDesc desc{};

			desc.blend.sampleMask = UINT_MAX;
			desc.blend.rtDesc[0].isBlendEnable = true;
			desc.blend.rtDesc[0].srcBlendColor = D3D12_BLEND_SRC_ALPHA;
			desc.blend.rtDesc[0].dstBlendColor = D3D12_BLEND_INV_SRC_ALPHA;
			desc.blend.rtDesc[0].blendOpColor = D3D12_BLEND_OP_ADD;
			desc.blend.rtDesc[0].srcBlendAlpha = D3D12_BLEND_ONE;
			desc.blend.rtDesc[0].dstBlendAlpha = D3D12_BLEND_ZERO;
			desc.blend.rtDesc[0].blendOpAlpha = D3D12_BLEND_OP_ADD;
			desc.blend.rtDesc[0].writeMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			desc.depthStencil.isDepthEnable = (dsFormat != DXGI_FORMAT_UNKNOWN);
			desc.depthStencil.isDepthWriteEnable = (dsFormat != DXGI_FORMAT_UNKNOWN);
			desc.depthStencil.depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

			desc.rasterizer.cullMode = D3D12_CULL_MODE_NONE;
			desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
			desc.rasterizer.isFrontCCW = false;
			desc.rasterizer.isDepthClipEnable = true;
			desc.multisampleCount = 1;

			desc.inputLayout.numElements = ARRAYSIZE(elementDescs);
			desc.inputLayout.pElements = elementDescs;

			desc.pRootSignature = pRootSig_;
			desc.pVS = pVShader_;
			desc.pPS = pPShader_;
			desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			desc.numRTVs = 0;
			desc.rtvFormats[desc.numRTVs++] = rtFormat;
			desc.dsvFormat = dsFormat;

			pPipelineState_ = new GraphicsPipelineState();
			if (!pPipelineState_->Initialize(pDevice, desc))
			{
				return false;
			}
		}

		// 定数バッファを作成
		pConstantBuffers_ = new Buffer[kMaxFrameCount];
		pConstantBufferViews_ = new ConstantBufferView[kMaxFrameCount];
		if (!pConstantBuffers_ || !pConstantBufferViews_)
		{
			return false;
		}
		for (u32 i = 0; i < kMaxFrameCount; i++)
		{
			if (!pConstantBuffers_[i].Initialize(pDevice, sizeof(VertexUniform), 0, BufferUsage::ConstantBuffer, true, false))
			{
				return false;
			}
			if (!pConstantBufferViews_[i].Initialize(pDevice, &pConstantBuffers_[i]))
			{
				return false;
			}
		}

		// 頂点・インデックスバッファを作成
		pVertexBuffers_ = new Buffer[kMaxFrameCount];
		pVertexBufferViews_ = new VertexBufferView[kMaxFrameCount];
		pIndexBuffers_ = new Buffer[kMaxFrameCount];
		pIndexBufferViews_ = new IndexBufferView[kMaxFrameCount];
		if (!pVertexBuffers_ || !pVertexBufferViews_ || !pIndexBuffers_ || !pIndexBufferViews_)
		{
			return false;
		}

		return true;
	}

	//----
	// 破棄
	void Gui::Destroy()
	{
		if (pOwner_)
		{
			sl12::SafeDelete(pVShader_);
			sl12::SafeDelete(pPShader_);
			sl12::SafeDelete(pFontTextureView_);
			sl12::SafeDelete(pFontTexture_);
			sl12::SafeDelete(pFontSampler_);

			sl12::SafeDeleteArray(pConstantBufferViews_);
			sl12::SafeDeleteArray(pConstantBuffers_);
			sl12::SafeDeleteArray(pVertexBufferViews_);
			sl12::SafeDeleteArray(pVertexBuffers_);
			sl12::SafeDeleteArray(pIndexBufferViews_);
			sl12::SafeDeleteArray(pIndexBuffers_);

			sl12::SafeDelete(pRootSig_);
			sl12::SafeDelete(pPipelineState_);
			sl12::SafeDelete(pDescSet_);

			ImGui::DestroyContext();

			pOwner_ = nullptr;
		}
		guiHandle_ = nullptr;
	}

	//----
	// フォントイメージ生成
	bool Gui::CreateFontImage(Device* pDevice, CommandList& cmdList)
	{
		if (!pOwner_)
		{
			return false;
		}

		ImGuiIO& io = ImGui::GetIO();

		unsigned char* pixels;
		int width, height;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
		size_t upload_size = width * height * 4 * sizeof(char);

		// テクスチャ作成
		TextureDesc desc;
		desc.dimension = TextureDimension::Texture2D;
		desc.width = static_cast<u32>(width);
		desc.height = static_cast<u32>(height);
		desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		
		pFontTexture_ = new Texture();
		if (!pFontTexture_)
		{
			return false;
		}

		if (!pFontTexture_->InitializeFromImageBin(pDevice, &cmdList, desc, pixels))
		{
			return false;
		}

		// テクスチャビュー作成
		pFontTextureView_ = new TextureView();
		if (!pFontTextureView_)
		{
			return false;
		}
		if (!pFontTextureView_->Initialize(pDevice, pFontTexture_))
		{
			return false;
		}

		// 登録
		io.Fonts->SetTexID(pFontTexture_);

		return true;
	}

	//----
	void Gui::RenderDrawList(ImDrawData* draw_data)
	{
		ImGuiIO& io = ImGui::GetIO();

		Gui* pThis = guiHandle_;
		Device* pDevice = pThis->pOwner_;
		CommandList* pCmdList = pThis->pDrawCommandList_;
		u32 frameIndex = pThis->frameIndex_;

		// 頂点バッファ生成
		Buffer& vbuffer = pThis->pVertexBuffers_[frameIndex];
		VertexBufferView& vbView = pThis->pVertexBufferViews_[frameIndex];
		size_t vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
		if (vbuffer.GetSize() < vertex_size)
		{
			vbuffer.Destroy();
			vbuffer.Initialize(pDevice, vertex_size, sizeof(ImDrawVert), BufferUsage::VertexBuffer, true, false);

			vbView.Destroy();
			vbView.Initialize(pDevice, &vbuffer);
		}

		// インデックスバッファ生成
		Buffer& ibuffer = pThis->pIndexBuffers_[frameIndex];
		IndexBufferView& ibView = pThis->pIndexBufferViews_[frameIndex];
		size_t index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
		if (ibuffer.GetSize() < index_size)
		{
			ibuffer.Destroy();
			ibuffer.Initialize(pDevice, index_size, sizeof(ImDrawIdx), BufferUsage::IndexBuffer, true, false);

			ibView.Destroy();
			ibView.Initialize(pDevice, &ibuffer);
		}

		// 頂点・インデックスのメモリを上書き
		{
			ImDrawVert* vtx_dst = static_cast<ImDrawVert*>(vbuffer.Map(pCmdList));
			ImDrawIdx* idx_dst = static_cast<ImDrawIdx*>(ibuffer.Map(pCmdList));

			for (int n = 0; n < draw_data->CmdListsCount; n++)
			{
				const ImDrawList* cmd_list = draw_data->CmdLists[n];
				memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
				memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
				vtx_dst += cmd_list->VtxBuffer.Size;
				idx_dst += cmd_list->IdxBuffer.Size;
			}

			vbuffer.Unmap();
			ibuffer.Unmap();
		}

		// 定数バッファ更新
		{
			Buffer& cb = pThis->pConstantBuffers_[frameIndex];
			float* p = static_cast<float*>(cb.Map(pCmdList));
			p[0] = 2.0f / io.DisplaySize.x;
			p[1] = -2.0f / io.DisplaySize.y;
			p[2] = -1.0f;
			p[3] = 1.0f;
			cb.Unmap();
		}

		// レンダリング開始
		// NOTE: レンダーターゲットは設定済みとする

		// パイプラインステート設定
		ID3D12GraphicsCommandList* pNativeCmdList = pCmdList->GetCommandList();
		pNativeCmdList->SetPipelineState(pThis->pPipelineState_->GetPSO());

		// DescriptorSet設定
		ConstantBufferView& cbView = pThis->pConstantBufferViews_[frameIndex];
		pThis->pDescSet_->Reset();
		pThis->pDescSet_->SetVsCbv(0, cbView.GetDescInfo().cpuHandle);
		pThis->pDescSet_->SetPsSrv(0, pThis->pFontTextureView_->GetDescInfo().cpuHandle);
		pThis->pDescSet_->SetPsSampler(0, pThis->pFontSampler_->GetDescInfo().cpuHandle);

		// RootSigとDescSetを設定
		pCmdList->SetGraphicsRootSignatureAndDescriptorSet(pThis->pRootSig_, pThis->pDescSet_);

		// DrawCall
		D3D12_VERTEX_BUFFER_VIEW views[] = { vbView.GetView() };
		pNativeCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pNativeCmdList->IASetVertexBuffers(0, _countof(views), views);
		pNativeCmdList->IASetIndexBuffer(&ibView.GetView());

		// DrawCall
		int vtx_offset = 0;
		int idx_offset = 0;
		for (int n = 0; n < draw_data->CmdListsCount; n++)
		{
			const ImDrawList* cmd_list = draw_data->CmdLists[n];
			for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
			{
				const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
				if (pcmd->UserCallback)
				{
					pcmd->UserCallback(cmd_list, pcmd);
				}
				else
				{
					D3D12_RECT rect;
					rect.left = static_cast<s32>(pcmd->ClipRect.x);
					rect.top = static_cast<s32>(pcmd->ClipRect.y);
					rect.right = static_cast<s32>(pcmd->ClipRect.z);
					rect.bottom = static_cast<s32>(pcmd->ClipRect.w);
					pNativeCmdList->RSSetScissorRects(1, &rect);

					pNativeCmdList->DrawIndexedInstanced(pcmd->ElemCount, 1, idx_offset, vtx_offset, 0);
				}
				idx_offset += pcmd->ElemCount;
			}
			vtx_offset += cmd_list->VtxBuffer.Size;
		}
	}

	//----
	// 新しいフレームの開始
	void Gui::BeginNewFrame(CommandList* pDrawCmdList, uint32_t frameWidth, uint32_t frameHeight, const InputData& input, float frameScale, float timeStep)
	{
		ImGuiIO& io = ImGui::GetIO();

		// フレームバッファのサイズを指定する
		io.DisplaySize = ImVec2((float)frameWidth, (float)frameHeight);
		io.DisplayFramebufferScale = ImVec2(frameScale, frameScale);

		// 時間進行を指定
		io.DeltaTime = timeStep;

		// マウスによる操作
		io.MousePos = ImVec2((float)input.mouseX, (float)input.mouseY);
		io.MouseDown[0] = (input.mouseButton & MouseButton::Left) != 0;
		io.MouseDown[1] = (input.mouseButton & MouseButton::Right) != 0;
		io.MouseDown[2] = (input.mouseButton & MouseButton::Middle) != 0;

		// 新規フレーム開始
		ImGui::NewFrame();

		frameIndex_ = (frameIndex_ + 1) % kMaxFrameCount;
		pDrawCommandList_ = pDrawCmdList;
	}

}	// namespace sl12


// eof
