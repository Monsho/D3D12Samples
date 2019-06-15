#pragma once

#include <imgui.h>
#include <sl12/util.h>


namespace sl12
{
	class Device;
	class Texture;
	class TextureView;
	class Buffer;
	class ConstantBufferView;
	class VertexBufferView;
	class IndexBufferView;
	class Shader;
	class Sampler;
	class CommandList;
	class RootSignature;
	class GraphicsPipelineState;
	class DescriptorSet;

	struct MouseButton
	{
		enum Type
		{
			Left			= 0x1 << 0,
			Right			= 0x1 << 1,
			Middle			= 0x1 << 2,

			Max
		};
	};	// struct MouseButton

	struct InputData
	{
		int	mouseX{ 0 }, mouseY{ 0 };
		u32	mouseButton{ 0 };
	};	// struct InputData

	class Gui
	{
	public:
		Gui()
		{}
		~Gui()
		{
			Destroy();
		}

		// 初期化
		bool Initialize(Device* pDevice, DXGI_FORMAT rtFormat, DXGI_FORMAT dsFormat = DXGI_FORMAT_UNKNOWN);
		// 破棄
		void Destroy();

		// フォントイメージ生成
		bool CreateFontImage(Device* pDevice, CommandList& cmdList);

		// 新しいフレームの開始
		void BeginNewFrame(CommandList* pDrawCmdList, u32 frameWidth, u32 frameHeight, const InputData& input, float frameScale = 1.0f, float timeStep = 1.0f / 60.0f);

	private:
		Device*			pOwner_{ nullptr };
		CommandList*	pDrawCommandList_{ nullptr };

		Shader*			pVShader_{ nullptr };
		Shader*			pPShader_{ nullptr };
		Texture*		pFontTexture_{ nullptr };
		TextureView*	pFontTextureView_{ nullptr };
		Sampler*		pFontSampler_{ nullptr };

		Buffer*					pConstantBuffers_{ nullptr };
		ConstantBufferView*		pConstantBufferViews_{ nullptr };
		Buffer*					pVertexBuffers_{ nullptr };
		VertexBufferView*		pVertexBufferViews_{ nullptr };
		Buffer*					pIndexBuffers_{ nullptr };
		IndexBufferView*		pIndexBufferViews_{ nullptr };

		DescriptorSet*			pDescSet_{ nullptr };
		RootSignature*			pRootSig_{ nullptr };
		GraphicsPipelineState*	pPipelineState_{ nullptr };

		u32	frameIndex_{ 0 };

	public:
		// 描画命令
		static void RenderDrawList(ImDrawData* draw_data);

	private:
		static Gui* guiHandle_;
	};	// class Gui

}	// namespace sl12


// eof
