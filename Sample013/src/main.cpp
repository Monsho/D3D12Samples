#include <vector>
#include <random>

#include "sl12/application.h"
#include "sl12/command_list.h"
#include "sl12/root_signature.h"
#include "sl12/texture.h"
#include "sl12/texture_view.h"
#include "sl12/buffer.h"
#include "sl12/buffer_view.h"
#include "sl12/command_queue.h"
#include "sl12/descriptor.h"
#include "sl12/descriptor_heap.h"
#include "sl12/swapchain.h"
#include "sl12/pipeline_state.h"
#include "sl12/acceleration_structure.h"
#include "sl12/file.h"
#include "sl12/shader.h"
#include "sl12/gui.h"
#include "sl12/glb_mesh.h"
#include "sl12/timestamp.h"
#include "sl12/fence.h"

#include "CompiledShaders/hybrid.lib.hlsl.h"
#include "CompiledShaders/vertex_bake.lib.hlsl.h"
#include "CompiledShaders/zpre.vv.hlsl.h"
#include "CompiledShaders/zpre.p.hlsl.h"
#include "CompiledShaders/copy.vv.hlsl.h"
#include "CompiledShaders/copy.p.hlsl.h"
#include "CompiledShaders/lighting.vv.hlsl.h"
#include "CompiledShaders/lighting.p.hlsl.h"

#include <windowsx.h>


namespace
{
	static const int	kScreenWidth = 1280;
	static const int	kScreenHeight = 720;
	static const int	MaxSample = 512;

	static LPCWSTR		kRayGenName = L"RayGenerator";
	static LPCWSTR		kClosestHitName = L"ClosestHitProcessor";
	static LPCWSTR		kAnyHitName = L"AnyHitProcessor";
	static LPCWSTR		kMissName = L"MissProcessor";
	static LPCWSTR		kMissShadowName = L"MissShadowProcessor";
	static LPCWSTR		kHitGroupName = L"HitGroup";
	static LPCWSTR		kHitGroupShadowName = L"HitGroupShadow";
}

class SampleApplication
	: public sl12::Application
{
	struct SceneCB
	{
		DirectX::XMFLOAT4X4	mtxWorld;
		DirectX::XMFLOAT4X4	mtxWorldToProj;
		DirectX::XMFLOAT4X4	mtxProjToWorld;
		DirectX::XMFLOAT4	camPos;
		DirectX::XMFLOAT4	lightDir;
		DirectX::XMFLOAT4	lightColor;
		float				skyPower;
		uint32_t			maxBounces;
	};

	struct TimeCB
	{
		uint32_t			loopCount;
	};

	struct Sphere
	{
		DirectX::XMFLOAT3	center;
		float				radius;
		DirectX::XMFLOAT4	color;
		uint32_t			material;

		Sphere()
			: center(0.0f, 0.0f, 0.0f), radius(1.0f), color(1.0f, 1.0f, 1.0f, 1.0f), material(0)
		{}
		Sphere(const DirectX::XMFLOAT3& c, float r, const DirectX::XMFLOAT4& col, uint32_t mat)
			: center(c), radius(r), color(col), material(mat)
		{}
		Sphere(float cx, float cy, float cz, float r, const DirectX::XMFLOAT4& col, uint32_t mat)
			: center(cx, cy, cz), radius(r), color(col), material(mat)
		{}
	};

	struct Instance
	{
		DirectX::XMFLOAT4X4	mtxLocalToWorld;
		DirectX::XMFLOAT4	color;
		uint32_t			material;
	};

	struct VertexLightColor
	{
		sl12::Buffer				colorB;
		sl12::VertexBufferView		colorVB;
		sl12::UnorderedAccessView	colorUAV;

		sl12::Buffer				workB;
		sl12::UnorderedAccessView	workUAV;

		VertexLightColor()
		{}
		~VertexLightColor()
		{
			colorUAV.Destroy();
			colorVB.Destroy();
			colorB.Destroy();

			workUAV.Destroy();
			workB.Destroy();
		}
		bool CreateObjects(sl12::Device* pDevice, size_t vtxCnt)
		{
			const size_t kStride = sizeof(float) * 3;
			if (!colorB.Initialize(pDevice, vtxCnt * kStride, kStride, sl12::BufferUsage::VertexBuffer, false, true))
			{
				return false;
			}
			if (!colorVB.Initialize(pDevice, &colorB))
			{
				return false;
			}
			if (!colorUAV.Initialize(pDevice, &colorB))
			{
				return false;
			}

			if (!workB.Initialize(pDevice, vtxCnt * kStride, kStride, sl12::BufferUsage::VertexBuffer, false, true))
			{
				return false;
			}
			if (!workUAV.Initialize(pDevice, &workB))
			{
				return false;
			}

			return true;
		}
	};

	struct DxrRenderSystem
	{
		sl12::RootSignature		globalRootSig, localRootSig;
		sl12::DxrPipelineState	stateObject;
		sl12::Buffer			rayGenTable, missTable, hitGroupTable;
		sl12::u64				shaderRecordSize;

		void Destroy()
		{
			rayGenTable.Destroy();
			missTable.Destroy();
			hitGroupTable.Destroy();

			stateObject.Destroy();

			globalRootSig.Destroy();
			localRootSig.Destroy();
		}
	};

public:
	SampleApplication(HINSTANCE hInstance, int nCmdShow, int screenWidth, int screenHeight)
		: Application(hInstance, nCmdShow, screenWidth, screenHeight)
	{}

	bool Initialize() override
	{
		// コマンドリストの初期化
		auto&& gqueue = device_.GetGraphicsQueue();
		for (auto&& v : cmdLists_)
		{
			if (!v.Initialize(&device_, &gqueue, true))
			{
				return false;
			}
		}

		// サンプラー作成
		{
			D3D12_SAMPLER_DESC samDesc{};
			samDesc.AddressU = samDesc.AddressV = samDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			if (!imageSampler_.Initialize(&device_, samDesc))
			{
				return false;
			}
		}

		// ルートシグネチャの初期化
		{
			D3D12_DESCRIPTOR_RANGE ranges[] = {
				{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
				{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
				{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
				{ D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
				{ D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
				{ D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
				{ D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
			};

			D3D12_ROOT_PARAMETER params[_countof(ranges) + 1];
			for (int i = 0; i < _countof(ranges); i++)
			{
				params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				params[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				params[i].DescriptorTable.NumDescriptorRanges = 1;
				params[i].DescriptorTable.pDescriptorRanges = &ranges[i];
			}
			params[_countof(ranges)].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
			params[_countof(ranges)].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			params[_countof(ranges)].Descriptor.ShaderRegister = 0;
			params[_countof(ranges)].Descriptor.RegisterSpace = 0;

			D3D12_ROOT_SIGNATURE_DESC sigDesc{};
			sigDesc.NumParameters = ARRAYSIZE(params);
			sigDesc.pParameters = params;
			sigDesc.NumStaticSamplers = 0;
			sigDesc.pStaticSamplers = nullptr;
			sigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

			if (!shadowRaySystem_.globalRootSig.Initialize(&device_, sigDesc))
			{
				return false;
			}
		}
		{
			sl12::RootParameter params[] = {
				sl12::RootParameter(sl12::RootParameterType::ShaderResource, sl12::ShaderVisibility::All, 4),
				sl12::RootParameter(sl12::RootParameterType::Sampler, sl12::ShaderVisibility::All, 4),
			};
			sl12::RootSignatureDesc desc;
			desc.pParameters = params;
			desc.numParameters = ARRAYSIZE(params);
			desc.flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
			if (!shadowRaySystem_.localRootSig.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			D3D12_DESCRIPTOR_RANGE ranges[] = {
				{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },		// RandomTable
				{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },		// SourcePosition
				{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },		// SourceNormal
				{ D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },		// DestColor
				{ D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },		// RandomSeed
				{ D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },		// cbScene
				{ D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },		// cbTime
			};

			D3D12_ROOT_PARAMETER params[_countof(ranges) + 1];
			for (int i = 0; i < _countof(ranges); i++)
			{
				params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				params[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				params[i].DescriptorTable.NumDescriptorRanges = 1;
				params[i].DescriptorTable.pDescriptorRanges = &ranges[i];
			}
			// Scene
			params[_countof(ranges)].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
			params[_countof(ranges)].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			params[_countof(ranges)].Descriptor.ShaderRegister = 0;
			params[_countof(ranges)].Descriptor.RegisterSpace = 0;

			D3D12_ROOT_SIGNATURE_DESC sigDesc{};
			sigDesc.NumParameters = ARRAYSIZE(params);
			sigDesc.pParameters = params;
			sigDesc.NumStaticSamplers = 0;
			sigDesc.pStaticSamplers = nullptr;
			sigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

			if (!vertexBakeSystem_.globalRootSig.Initialize(&device_, sigDesc))
			{
				return false;
			}
		}
		{
			sl12::RootParameter params[] = {
				sl12::RootParameter(sl12::RootParameterType::ShaderResource, sl12::ShaderVisibility::All, 4),
				sl12::RootParameter(sl12::RootParameterType::ShaderResource, sl12::ShaderVisibility::All, 5),
				sl12::RootParameter(sl12::RootParameterType::ShaderResource, sl12::ShaderVisibility::All, 6),
				sl12::RootParameter(sl12::RootParameterType::ShaderResource, sl12::ShaderVisibility::All, 7),
				sl12::RootParameter(sl12::RootParameterType::ShaderResource, sl12::ShaderVisibility::All, 8),
				sl12::RootParameter(sl12::RootParameterType::Sampler, sl12::ShaderVisibility::All, 0),
			};
			sl12::RootSignatureDesc desc;
			desc.pParameters = params;
			desc.numParameters = ARRAYSIZE(params);
			desc.flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
			if (!vertexBakeSystem_.localRootSig.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			sl12::RootParameter params[] = {
				sl12::RootParameter(sl12::RootParameterType::ConstantBuffer, sl12::ShaderVisibility::Vertex, 0),
				sl12::RootParameter(sl12::RootParameterType::ShaderResource, sl12::ShaderVisibility::Pixel, 0),
				sl12::RootParameter(sl12::RootParameterType::Sampler, sl12::ShaderVisibility::Pixel, 0),
			};
			sl12::RootSignatureDesc desc;
			desc.pParameters = params;
			desc.numParameters = ARRAYSIZE(params);
			desc.flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			if (!zpreRootSig_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			sl12::RootParameter params[] = {
				sl12::RootParameter(sl12::RootParameterType::ConstantBuffer, sl12::ShaderVisibility::All, 0),
				sl12::RootParameter(sl12::RootParameterType::ShaderResource, sl12::ShaderVisibility::Pixel, 0),
				sl12::RootParameter(sl12::RootParameterType::Sampler, sl12::ShaderVisibility::Pixel, 0),
				sl12::RootParameter(sl12::RootParameterType::ShaderResource, sl12::ShaderVisibility::Pixel, 1),
			};
			sl12::RootSignatureDesc desc;
			desc.pParameters = params;
			desc.numParameters = ARRAYSIZE(params);
			desc.flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			if (!lightingRootSig_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			D3D12_DESCRIPTOR_RANGE ranges[]{
				{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
				{ D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
			};
			D3D12_ROOT_PARAMETER rootParameters[]{
				{ D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL },
				{ D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, 1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL },
			};

			D3D12_ROOT_SIGNATURE_DESC sigDesc;
			sigDesc.NumParameters = _countof(rootParameters);
			sigDesc.pParameters = rootParameters;
			sigDesc.NumStaticSamplers = 0;
			sigDesc.pStaticSamplers = nullptr;
			sigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			if (!copyRootSig_.Initialize(&device_, sigDesc))
			{
				return false;
			}
		}

		// パイプラインステートオブジェクトの初期化
		if (!CreatePipelineState())
		{
			return false;
		}
		{
			if (!zpreVS_.Initialize(&device_, sl12::ShaderType::Vertex, g_pZpreVS, sizeof(g_pZpreVS)))
			{
				return false;
			}
			if (!zprePS_.Initialize(&device_, sl12::ShaderType::Pixel, g_pZprePS, sizeof(g_pZprePS)))
			{
				return false;
			}

			sl12::GraphicsPipelineStateDesc desc;
			desc.pRootSignature = &zpreRootSig_;
			desc.pVS = &zpreVS_;
			desc.pPS = &zprePS_;

			desc.blend.sampleMask = UINT_MAX;
			desc.blend.rtDesc[0].isBlendEnable = false;
			desc.blend.rtDesc[0].writeMask = 0xf;

			desc.rasterizer.cullMode = D3D12_CULL_MODE_NONE;
			desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
			desc.rasterizer.isDepthClipEnable = true;
			desc.rasterizer.isFrontCCW = false;

			desc.depthStencil.isDepthEnable = true;
			desc.depthStencil.isDepthWriteEnable = true;
			desc.depthStencil.depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

			D3D12_INPUT_ELEMENT_DESC input_elems[] = {
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			};
			desc.inputLayout.numElements = ARRAYSIZE(input_elems);
			desc.inputLayout.pElements = input_elems;

			desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			desc.numRTVs = 0;
			desc.rtvFormats[desc.numRTVs++] = DXGI_FORMAT_R10G10B10A2_UNORM;
			desc.dsvFormat = DXGI_FORMAT_D32_FLOAT;
			desc.multisampleCount = 1;

			if (!zprePso_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			if (!lightingVS_.Initialize(&device_, sl12::ShaderType::Vertex, g_pLightingVS, sizeof(g_pLightingVS)))
			{
				return false;
			}
			if (!lightingPS_.Initialize(&device_, sl12::ShaderType::Pixel, g_pLightingPS, sizeof(g_pLightingPS)))
			{
				return false;
			}

			sl12::GraphicsPipelineStateDesc desc;
			desc.pRootSignature = &lightingRootSig_;
			desc.pVS = &lightingVS_;
			desc.pPS = &lightingPS_;

			desc.blend.sampleMask = UINT_MAX;
			desc.blend.rtDesc[0].isBlendEnable = false;
			desc.blend.rtDesc[0].writeMask = 0xf;

			desc.rasterizer.cullMode = D3D12_CULL_MODE_NONE;
			desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
			desc.rasterizer.isDepthClipEnable = true;
			desc.rasterizer.isFrontCCW = false;

			desc.depthStencil.isDepthEnable = true;
			desc.depthStencil.isDepthWriteEnable = false;
			desc.depthStencil.depthFunc = D3D12_COMPARISON_FUNC_EQUAL;

			D3D12_INPUT_ELEMENT_DESC input_elems[] = {
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 3, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			};
			desc.inputLayout.numElements = ARRAYSIZE(input_elems);
			desc.inputLayout.pElements = input_elems;

			desc.primTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			desc.numRTVs = 0;
			desc.rtvFormats[desc.numRTVs++] = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.dsvFormat = DXGI_FORMAT_D32_FLOAT;
			desc.multisampleCount = 1;

			if (!lightingPso_.Initialize(&device_, desc))
			{
				return false;
			}
		}
		{
			if (!copyVS_.Initialize(&device_, sl12::ShaderType::Vertex, g_pCopyVS, sizeof(g_pCopyVS)))
			{
				return false;
			}
			if (!copyPS_.Initialize(&device_, sl12::ShaderType::Pixel, g_pCopyPS, sizeof(g_pCopyPS)))
			{
				return false;
			}

			sl12::GraphicsPipelineStateDesc desc;
			desc.pRootSignature = &copyRootSig_;
			desc.pVS = &copyVS_;
			desc.pPS = &copyPS_;

			desc.blend.sampleMask = UINT_MAX;
			desc.blend.rtDesc[0].isBlendEnable = false;
			desc.blend.rtDesc[0].writeMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			desc.rasterizer.cullMode = D3D12_CULL_MODE_NONE;
			desc.rasterizer.fillMode = D3D12_FILL_MODE_SOLID;
			desc.rasterizer.isDepthClipEnable = false;
			desc.rasterizer.isFrontCCW = true;

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

			if (!copyPso_.Initialize(&device_, desc))
			{
				return false;
			}
		}

		// 出力先のテクスチャを生成
		{
			sl12::TextureDesc desc;
			desc.dimension = sl12::TextureDimension::Texture2D;
			desc.width = kScreenWidth;
			desc.height = kScreenHeight;
			desc.mipLevels = 1;
			desc.format = DXGI_FORMAT_R32_FLOAT;
			desc.initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			desc.sampleCount = 1;
			desc.clearColor[4] = { 0.0f };
			desc.clearDepth = 1.0f;
			desc.clearStencil = 0;
			desc.isRenderTarget = true;
			desc.isDepthBuffer = false;
			desc.isUav = true;
			if (!resultTexture_.Initialize(&device_, desc))
			{
				return false;
			}

			if (!resultTextureSRV_.Initialize(&device_, &resultTexture_))
			{
				return false;
			}

			if (!resultTextureRTV_.Initialize(&device_, &resultTexture_))
			{
				return false;
			}

			if (!resultTextureUAV_.Initialize(&device_, &resultTexture_))
			{
				return false;
			}
		}

		// Gバッファを生成
		{
			sl12::TextureDesc desc;
			desc.dimension = sl12::TextureDimension::Texture2D;
			desc.width = kScreenWidth;
			desc.height = kScreenHeight;
			desc.mipLevels = 1;
			desc.format = DXGI_FORMAT_R10G10B10A2_UNORM;
			desc.initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
			desc.sampleCount = 1;
			desc.clearColor[4] = { 0.0f };
			desc.clearDepth = 1.0f;
			desc.clearStencil = 0;
			desc.isRenderTarget = true;
			desc.isDepthBuffer = false;
			desc.isUav = false;
			if (!gbufferTexture_.Initialize(&device_, desc))
			{
				return false;
			}

			if (!gbufferTextureSRV_.Initialize(&device_, &gbufferTexture_))
			{
				return false;
			}

			if (!gbufferTextureRTV_.Initialize(&device_, &gbufferTexture_))
			{
				return false;
			}
		}

		// 深度バッファを生成
		{
			sl12::TextureDesc desc;
			desc.dimension = sl12::TextureDimension::Texture2D;
			desc.width = kScreenWidth;
			desc.height = kScreenHeight;
			desc.mipLevels = 1;
			desc.format = DXGI_FORMAT_D32_FLOAT;
			desc.initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
			desc.sampleCount = 1;
			desc.clearColor[4] = { 0.0f };
			desc.clearDepth = 1.0f;
			desc.clearStencil = 0;
			desc.isRenderTarget = false;
			desc.isDepthBuffer = true;
			desc.isUav = false;
			if (!depthTexture_.Initialize(&device_, desc))
			{
				return false;
			}

			if (!depthTextureSRV_.Initialize(&device_, &depthTexture_))
			{
				return false;
			}

			if (!depthTextureDSV_.Initialize(&device_, &depthTexture_))
			{
				return false;
			}
		}

		// 乱数用のバッファを生成
		{
			if (!randomBuffer_.Initialize(&device_, sizeof(float) * 65536, sizeof(float), sl12::BufferUsage::ShaderResource, true, false))
			{
				return false;
			}
			if (!randomBufferSRV_.Initialize(&device_, &randomBuffer_, 0, sizeof(float)))
			{
				return false;
			}
			if (!seedBuffer_.Initialize(&device_, sizeof(uint32_t), 1, sl12::BufferUsage::ShaderResource, false, true))
			{
				return false;
			}
			if (!seedBufferUAV_.Initialize(&device_, &seedBuffer_))
			{
				return false;
			}

			auto p = reinterpret_cast<float*>(randomBuffer_.Map(nullptr));
			std::random_device rd;
			std::mt19937 mt(rd());
			std::uniform_real_distribution<float> randGen01(0.0f, 1.0f);
			for (int i = 0; i < 65536; i++)
			{
				p[i] = randGen01(mt);
			}
			randomBuffer_.Unmap();
		}

		// GUIの初期化
		if (!gui_.Initialize(&device_, DXGI_FORMAT_R8G8B8A8_UNORM))
		{
			return false;
		}
		if (!gui_.CreateFontImage(&device_, cmdLists_[0]))
		{
			return false;
		}

		// ジオメトリを生成する
		if (!CreateGeometry())
		{
			return false;
		}

		// ASを生成する
		if (!CreateAccelerationStructure())
		{
			return false;
		}

		// シーン定数バッファを生成する
		if (!CreateSceneCB())
		{
			return false;
		}
		if (!CreateTimeCB())
		{
			return false;
		}

		// シェーダテーブルを生成する
		if (!CreateShaderTableShadow())
		{
			return false;
		}
		if (!CreateShaderTableVertexBake())
		{
			return false;
		}

		// タイムスタンプクエリとバッファ
		for (int i = 0; i < ARRAYSIZE(gpuTimestamp_); ++i)
		{
			if (!gpuTimestamp_[i].Initialize(&device_, 5))
			{
				return false;
			}
		}

		return true;
	}

	bool Execute() override
	{
		device_.WaitPresent();

		auto frameIndex = (device_.GetSwapchain().GetFrameIndex() + sl12::Swapchain::kMaxBuffer - 1) % sl12::Swapchain::kMaxBuffer;
		auto prevFrameIndex = (device_.GetSwapchain().GetFrameIndex() + sl12::Swapchain::kMaxBuffer - 2) % sl12::Swapchain::kMaxBuffer;
		auto&& cmdList = cmdLists_[frameIndex];
		auto&& d3dCmdList = cmdList.GetCommandList();
		auto&& dxrCmdList = cmdList.GetDxrCommandList();

		UpdateSceneCB(frameIndex);

		cmdList.Reset();

		gui_.BeginNewFrame(&cmdList, kScreenWidth, kScreenHeight, inputData_);

		// GUI
		{
			if (ImGui::SliderAngle("Camera Angle", &camRotAngle_))
			{
				isClearTarget_ = true;
				shadowLoopCount_ = 0;
			}
			if (ImGui::SliderFloat("Sky Power", &skyPower_, 0.0f, 10.0f))
			{
				bakeLoopCount_ = 0;
			}
			if (ImGui::SliderFloat("Light Intensity", &lightPower_, 0.0f, 10.0f))
			{
				bakeLoopCount_ = 0;
			}
			if (ImGui::ColorEdit3("Light Color", lightColor_))
			{
				bakeLoopCount_ = 0;
			}
			if (ImGui::SliderInt("Max Bounces", &maxBounces_, 1, 8))
			{
				bakeLoopCount_ = 0;
			}

			uint64_t timestamp[5];
			gpuTimestamp_[prevFrameIndex].GetTimestamp(0, 5, timestamp);
			uint64_t all_time = timestamp[4] - timestamp[0];
			uint64_t ray_time = timestamp[3] - timestamp[2];
			uint64_t freq = device_.GetGraphicsQueue().GetTimestampFrequency();
			float all_ms = (float)all_time / ((float)freq / 1000.0f);
			float ray_ms = (float)ray_time / ((float)freq / 1000.0f);

			ImGui::Text("All GPU: %f (ms)", all_ms);
			ImGui::Text("RayTracing: %f (ms)", ray_ms);
		}

		gpuTimestamp_[frameIndex].Reset();
		gpuTimestamp_[frameIndex].Query(&cmdList);

		auto&& swapchain = device_.GetSwapchain();
		cmdList.TransitionBarrier(swapchain.GetCurrentTexture(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		{
			float color[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
			d3dCmdList->ClearRenderTargetView(swapchain.GetCurrentRenderTargetView()->GetDesc()->GetCpuHandle(), color, 0, nullptr);
		}

		if (isClearTarget_)
		{
			isClearTarget_ = false;
			float color[4] = { 0.0f };
			cmdList.TransitionBarrier(&resultTexture_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);
			d3dCmdList->ClearRenderTargetView(resultTextureRTV_.GetDesc()->GetCpuHandle(), color, 0, nullptr);
			cmdList.TransitionBarrier(&resultTexture_, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}
		d3dCmdList->ClearDepthStencilView(depthTextureDSV_.GetDesc()->GetCpuHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		// デスクリプタヒープを設定
		ID3D12DescriptorHeap* pDescHeaps[] = {
			device_.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetHeap(),
			device_.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER).GetHeap()
		};
		d3dCmdList->SetDescriptorHeaps(ARRAYSIZE(pDescHeaps), pDescHeaps);

		cmdList.TransitionBarrier(&gbufferTexture_, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);

		gpuTimestamp_[frameIndex].Query(&cmdList);

		// Z pre pass
		{
			// レンダーターゲット設定
			auto&& rtv = gbufferTextureRTV_.GetDesc()->GetCpuHandle();
			auto&& dsv = depthTextureDSV_.GetDesc()->GetCpuHandle();
			d3dCmdList->OMSetRenderTargets(1, &rtv, false, &dsv);

			D3D12_VIEWPORT vp;
			vp.TopLeftX = vp.TopLeftY = 0.0f;
			vp.Width = kScreenWidth;
			vp.Height = kScreenHeight;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			d3dCmdList->RSSetViewports(1, &vp);

			D3D12_RECT rect;
			rect.left = rect.top = 0;
			rect.right = kScreenWidth;
			rect.bottom = kScreenHeight;
			d3dCmdList->RSSetScissorRects(1, &rect);

			// PSOとルートシグネチャを設定
			d3dCmdList->SetPipelineState(zprePso_.GetPSO());
			d3dCmdList->SetGraphicsRootSignature(zpreRootSig_.GetRootSignature());

			// デスクリプタテーブル設定
			d3dCmdList->SetGraphicsRootDescriptorTable(0, sceneCBVs_[frameIndex].GetDesc()->GetGpuHandle());
			d3dCmdList->SetGraphicsRootDescriptorTable(2, linearWrapSampler_.GetDesc()->GetGpuHandle());

			// DrawCall
			d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			auto submesh_count = glbMesh_.GetSubmeshCount();
			for (int i = 0; i < submesh_count; i++)
			{
				auto&& submesh = glbMesh_.GetSubmesh(i);
				auto&& material = glbMesh_.GetMaterial(submesh->GetMaterialIndex());
				auto&& base_color_srv = glbMesh_.GetTextureView(material->GetTexBaseColorIndex());

				d3dCmdList->SetGraphicsRootDescriptorTable(1, base_color_srv->GetDesc()->GetGpuHandle());

				const D3D12_VERTEX_BUFFER_VIEW vbvs[] = {
					submesh->GetPositionVBV().GetView(),
					submesh->GetTexcoordVBV().GetView(),
				};
				d3dCmdList->IASetVertexBuffers(0, ARRAYSIZE(vbvs), vbvs);

				auto&& ibv = submesh->GetIndexIBV().GetView();
				d3dCmdList->IASetIndexBuffer(&ibv);

				d3dCmdList->DrawIndexedInstanced(submesh->GetIndicesCount(), 1, 0, 0, 0);
			}
		}

		// リソースバリア
		cmdList.TransitionBarrier(&gbufferTexture_, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
		cmdList.TransitionBarrier(&depthTexture_, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);

		gpuTimestamp_[frameIndex].Query(&cmdList);

		if (shadowLoopCount_ < MaxSample)
		{
			// シャドウ生成用のレイトレース
			// グローバルルートシグネチャを設定
			d3dCmdList->SetComputeRootSignature(shadowRaySystem_.globalRootSig.GetRootSignature());

			// グローバル設定のシェーダリソースを設定する
			d3dCmdList->SetComputeRootDescriptorTable(0, randomBufferSRV_.GetDesc()->GetGpuHandle());
			d3dCmdList->SetComputeRootDescriptorTable(1, gbufferTextureSRV_.GetDesc()->GetGpuHandle());
			d3dCmdList->SetComputeRootDescriptorTable(2, depthTextureSRV_.GetDesc()->GetGpuHandle());
			d3dCmdList->SetComputeRootDescriptorTable(3, resultTextureUAV_.GetDesc()->GetGpuHandle());
			d3dCmdList->SetComputeRootDescriptorTable(4, seedBufferUAV_.GetDesc()->GetGpuHandle());
			d3dCmdList->SetComputeRootDescriptorTable(5, sceneCBVs_[frameIndex].GetDesc()->GetGpuHandle());
			d3dCmdList->SetComputeRootDescriptorTable(6, timeCBVs_[0][frameIndex].GetDesc()->GetGpuHandle());
			d3dCmdList->SetComputeRootShaderResourceView(7, topAS_.GetDxrBuffer().GetResourceDep()->GetGPUVirtualAddress());

			// レイトレースを実行
			D3D12_DISPATCH_RAYS_DESC desc{};
			desc.HitGroupTable.StartAddress = shadowRaySystem_.hitGroupTable.GetResourceDep()->GetGPUVirtualAddress();
			desc.HitGroupTable.SizeInBytes = shadowRaySystem_.hitGroupTable.GetSize();
			desc.HitGroupTable.StrideInBytes = shadowRaySystem_.shaderRecordSize;
			desc.MissShaderTable.StartAddress = shadowRaySystem_.missTable.GetResourceDep()->GetGPUVirtualAddress();
			desc.MissShaderTable.SizeInBytes = shadowRaySystem_.missTable.GetSize();
			desc.MissShaderTable.StrideInBytes = shadowRaySystem_.shaderRecordSize;
			desc.RayGenerationShaderRecord.StartAddress = shadowRaySystem_.rayGenTable.GetResourceDep()->GetGPUVirtualAddress();
			desc.RayGenerationShaderRecord.SizeInBytes = shadowRaySystem_.rayGenTable.GetSize();
			desc.Width = kScreenWidth;
			desc.Height = kScreenHeight;
			desc.Depth = 1;
			dxrCmdList->SetPipelineState1(shadowRaySystem_.stateObject.GetPSO());
			dxrCmdList->DispatchRays(&desc);

			cmdList.UAVBarrier(&resultTexture_);
		}

		if (bakeLoopCount_ < MaxSample)
		{
			// 頂点ライトベイク用のレイトレース
			// グローバルルートシグネチャを設定
			d3dCmdList->SetComputeRootSignature(vertexBakeSystem_.globalRootSig.GetRootSignature());

			// グローバル設定のシェーダリソースを設定する
			d3dCmdList->SetComputeRootDescriptorTable(0, randomBufferSRV_.GetDesc()->GetGpuHandle());
			d3dCmdList->SetComputeRootDescriptorTable(4, seedBufferUAV_.GetDesc()->GetGpuHandle());
			d3dCmdList->SetComputeRootDescriptorTable(5, sceneCBVs_[frameIndex].GetDesc()->GetGpuHandle());
			d3dCmdList->SetComputeRootDescriptorTable(6, timeCBVs_[1][frameIndex].GetDesc()->GetGpuHandle());
			d3dCmdList->SetComputeRootShaderResourceView(7, topAS_.GetDxrBuffer().GetResourceDep()->GetGPUVirtualAddress());

			dxrCmdList->SetPipelineState1(vertexBakeSystem_.stateObject.GetPSO());

			// レイトレースを実行
			D3D12_DISPATCH_RAYS_DESC desc{};
			desc.HitGroupTable.StartAddress = vertexBakeSystem_.hitGroupTable.GetResourceDep()->GetGPUVirtualAddress();
			desc.HitGroupTable.SizeInBytes = vertexBakeSystem_.hitGroupTable.GetSize();
			desc.HitGroupTable.StrideInBytes = vertexBakeSystem_.shaderRecordSize;
			desc.MissShaderTable.StartAddress = vertexBakeSystem_.missTable.GetResourceDep()->GetGPUVirtualAddress();
			desc.MissShaderTable.SizeInBytes = vertexBakeSystem_.missTable.GetSize();
			desc.MissShaderTable.StrideInBytes = vertexBakeSystem_.shaderRecordSize;
			desc.RayGenerationShaderRecord.StartAddress = vertexBakeSystem_.rayGenTable.GetResourceDep()->GetGPUVirtualAddress();
			desc.RayGenerationShaderRecord.SizeInBytes = vertexBakeSystem_.rayGenTable.GetSize();
			desc.Height = 1;
			desc.Depth = 1;

			for (int i = 0; i < glbMesh_.GetSubmeshCount(); i++)
			{
				auto&& submesh = glbMesh_.GetSubmesh(i);
				auto&& vcolor = vertexColors_[i];

				// サブメッシュごとのShaderResource
				d3dCmdList->SetComputeRootDescriptorTable(1, submesh->GetPositionBV().GetDesc()->GetGpuHandle());
				d3dCmdList->SetComputeRootDescriptorTable(2, submesh->GetNormalBV().GetDesc()->GetGpuHandle());
				d3dCmdList->SetComputeRootDescriptorTable(3, vcolor.colorUAV.GetDesc()->GetGpuHandle());

				desc.Width = submesh->GetVerticesCount();
				dxrCmdList->DispatchRays(&desc);
			}
			for (int i = 0; i < glbMesh_.GetSubmeshCount(); i++)
			{
				cmdList.UAVBarrier(&vertexColors_[i].colorB);
			}
		}

		// リソースバリア
		cmdList.TransitionBarrier(&resultTexture_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmdList.TransitionBarrier(&depthTexture_, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		gpuTimestamp_[frameIndex].Query(&cmdList);

		// lighting pass
		{
			// レンダーターゲット設定
			auto&& rtv = swapchain.GetCurrentRenderTargetView()->GetDesc()->GetCpuHandle();
			auto&& dsv = depthTextureDSV_.GetDesc()->GetCpuHandle();
			d3dCmdList->OMSetRenderTargets(1, &rtv, false, &dsv);

			D3D12_VIEWPORT vp;
			vp.TopLeftX = vp.TopLeftY = 0.0f;
			vp.Width = kScreenWidth;
			vp.Height = kScreenHeight;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			d3dCmdList->RSSetViewports(1, &vp);

			D3D12_RECT rect;
			rect.left = rect.top = 0;
			rect.right = kScreenWidth;
			rect.bottom = kScreenHeight;
			d3dCmdList->RSSetScissorRects(1, &rect);

			// PSOとルートシグネチャを設定
			d3dCmdList->SetPipelineState(lightingPso_.GetPSO());
			d3dCmdList->SetGraphicsRootSignature(lightingRootSig_.GetRootSignature());

			// デスクリプタテーブル設定
			d3dCmdList->SetGraphicsRootDescriptorTable(0, sceneCBVs_[frameIndex].GetDesc()->GetGpuHandle());
			d3dCmdList->SetGraphicsRootDescriptorTable(2, linearWrapSampler_.GetDesc()->GetGpuHandle());
			d3dCmdList->SetGraphicsRootDescriptorTable(3, resultTextureSRV_.GetDesc()->GetGpuHandle());

			// DrawCall
			d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			auto submesh_count = glbMesh_.GetSubmeshCount();
			for (int i = 0; i < submesh_count; i++)
			{
				auto&& submesh = glbMesh_.GetSubmesh(i);
				auto&& material = glbMesh_.GetMaterial(submesh->GetMaterialIndex());
				auto&& base_color_srv = glbMesh_.GetTextureView(material->GetTexBaseColorIndex());
				auto&& vcolor = vertexColors_[i].colorVB;

				d3dCmdList->SetGraphicsRootDescriptorTable(1, base_color_srv->GetDesc()->GetGpuHandle());

				const D3D12_VERTEX_BUFFER_VIEW vbvs[] = {
					submesh->GetPositionVBV().GetView(),
					submesh->GetNormalVBV().GetView(),
					submesh->GetTexcoordVBV().GetView(),
					vcolor.GetView(),
				};
				d3dCmdList->IASetVertexBuffers(0, ARRAYSIZE(vbvs), vbvs);

				auto&& ibv = submesh->GetIndexIBV().GetView();
				d3dCmdList->IASetIndexBuffer(&ibv);

				d3dCmdList->DrawIndexedInstanced(submesh->GetIndicesCount(), 1, 0, 0, 0);
			}
		}

		ImGui::Render();

		cmdList.TransitionBarrier(swapchain.GetCurrentTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		cmdList.TransitionBarrier(&resultTexture_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		gpuTimestamp_[frameIndex].Query(&cmdList);
		gpuTimestamp_[frameIndex].Resolve(&cmdList);

		// コマンド終了と実行
		cmdList.Close();
		cmdList.Execute();
		device_.WaitDrawDone();

		// 次のフレームへ
		device_.Present(1);

		return true;
	}

	void Finalize() override
	{
		for (auto&& v : sceneCBVs_) v.Destroy();
		for (auto&& v : sceneCBs_) v.Destroy();

		for (auto&& v : gpuTimestamp_) v.Destroy();

		gui_.Destroy();

		topAS_.Destroy();
		bottomAS_.Destroy();

		vertexColors_.clear();
		glbMesh_.Destroy();

		instanceSBV_.Destroy();
		instanceSB_.Destroy();
		spheresAABB_.Destroy();

		randomBufferSRV_.Destroy();
		randomBuffer_.Destroy();
		seedBufferUAV_.Destroy();
		seedBuffer_.Destroy();

		depthTextureDSV_.Destroy();
		depthTextureSRV_.Destroy();
		depthTexture_.Destroy();

		gbufferTextureRTV_.Destroy();
		gbufferTextureSRV_.Destroy();
		gbufferTexture_.Destroy();

		resultTextureUAV_.Destroy();
		resultTextureRTV_.Destroy();
		resultTextureSRV_.Destroy();
		resultTexture_.Destroy();

		lightingPso_.Destroy();
		lightingVS_.Destroy();
		lightingPS_.Destroy();
		zprePso_.Destroy();
		zpreVS_.Destroy();
		zprePS_.Destroy();
		copyPso_.Destroy();
		copyVS_.Destroy();
		copyPS_.Destroy();

		copyRootSig_.Destroy();
		lightingRootSig_.Destroy();
		zpreRootSig_.Destroy();

		shadowRaySystem_.Destroy();
		vertexBakeSystem_.Destroy();

		for (auto&& v : cmdLists_) v.Destroy();
	}

	int Input(UINT message, WPARAM wParam, LPARAM lParam)
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
	bool CreatePipelineState()
	{
		// シャドウ用
		{
			// DXR用のパイプラインステートを生成します.
			// Graphics用、Compute用のパイプラインステートは生成時に固定サイズの記述子を用意します.
			// これに対して、DXR用のパイプラインステートは可変個のサブオブジェクトを必要とします.
			// また、サブオブジェクトの中には他のサブオブジェクトへのポインタを必要とするものもあるため、サブオブジェクト配列の生成には注意が必要です.

			sl12::DxrPipelineStateDesc dxrDesc;

			// DXILライブラリサブオブジェクト
			// 1つのシェーダライブラリと、そこに登録されているシェーダのエントリーポイントをエクスポートするためのサブオブジェクトです.
			D3D12_EXPORT_DESC libExport[] = {
				{ kRayGenName,				nullptr, D3D12_EXPORT_FLAG_NONE },
				{ kClosestHitName,			nullptr, D3D12_EXPORT_FLAG_NONE },
				{ kMissName,				nullptr, D3D12_EXPORT_FLAG_NONE },
			};
			dxrDesc.AddDxilLibrary(g_pHybridLib, sizeof(g_pHybridLib), libExport, ARRAYSIZE(libExport));

			// ヒットグループサブオブジェクト
			// Intersection, AnyHit, ClosestHitの組み合わせを定義し、ヒットグループ名でまとめるサブオブジェクトです.
			// マテリアルごとや用途ごと(マテリアル、シャドウなど)にサブオブジェクトを用意します.
			dxrDesc.AddHitGroup(kHitGroupName, true, nullptr, kClosestHitName, nullptr);

			// シェーダコンフィグサブオブジェクト
			// ヒットシェーダ、ミスシェーダの引数となるPayload, IntersectionAttributesの最大サイズを設定します.
			dxrDesc.AddShaderConfig(sizeof(float) * 4 + sizeof(sl12::u32), sizeof(float) * 2);

			// ローカルルートシグネチャサブオブジェクト
			// シェーダレコードごとに設定されるルートシグネチャを設定します.

			// Exports Assosiation サブオブジェクト
			// シェーダレコードとローカルルートシグネチャのバインドを行うサブオブジェクトです.
			LPCWSTR kExports[] = {
				kRayGenName,
				kMissName,
				kHitGroupName,
			};
			dxrDesc.AddLocalRootSignatureAndExportAssociation(shadowRaySystem_.localRootSig, kExports, ARRAYSIZE(kExports));

			// グローバルルートシグネチャサブオブジェクト
			// すべてのシェーダテーブルで参照されるグローバルなルートシグネチャを設定します.
			dxrDesc.AddGlobalRootSignature(shadowRaySystem_.globalRootSig);

			// レイトレースコンフィグサブオブジェクト
			// TraceRay()を行うことができる最大深度を指定するサブオブジェクトです.
			dxrDesc.AddRaytracinConfig(1);

			// PSO生成
			if (!shadowRaySystem_.stateObject.Initialize(&device_, dxrDesc))
			{
				return false;
			}
		}

		// 頂点ベイク用
		{
			sl12::DxrPipelineStateDesc dxrDesc;

			D3D12_EXPORT_DESC libExport[] = {
				{ kRayGenName,			nullptr, D3D12_EXPORT_FLAG_NONE },
				{ kClosestHitName,		nullptr, D3D12_EXPORT_FLAG_NONE },
				{ kAnyHitName,			nullptr, D3D12_EXPORT_FLAG_NONE },
				{ kMissName,			nullptr, D3D12_EXPORT_FLAG_NONE },
				{ kMissShadowName,		nullptr, D3D12_EXPORT_FLAG_NONE },
			};
			dxrDesc.AddDxilLibrary(g_pVertexBaseLib, sizeof(g_pVertexBaseLib), libExport, ARRAYSIZE(libExport));

			dxrDesc.AddHitGroup(kHitGroupName, true, kAnyHitName, kClosestHitName, nullptr);
			dxrDesc.AddHitGroup(kHitGroupShadowName, true, kAnyHitName, kClosestHitName, nullptr);

			dxrDesc.AddShaderConfig(sizeof(float) * 4 * 1 + sizeof(float) * 3 * 3 + sizeof(sl12::u32), sizeof(float) * 2);

			LPCWSTR kExports[] = {
				kRayGenName,
				kMissName,
				kMissShadowName,
				kHitGroupName,
				kHitGroupShadowName,
			};
			dxrDesc.AddLocalRootSignatureAndExportAssociation(vertexBakeSystem_.localRootSig, kExports, ARRAYSIZE(kExports));

			dxrDesc.AddGlobalRootSignature(vertexBakeSystem_.globalRootSig);

			dxrDesc.AddRaytracinConfig(2);

			// PSO生成
			if (!vertexBakeSystem_.stateObject.Initialize(&device_, dxrDesc))
			{
				return false;
			}
		}

		return true;
	}

	bool CreateGeometry()
	{
		// メッシュをロード
		if (!glbMesh_.Initialize(&device_, &cmdLists_[0], "data/", "sponza.glb"))
		{
			return false;
		}

		// サブメッシュ数分の頂点カラーを生成する
		cmdLists_[0].Reset();
		vertexColors_.resize(glbMesh_.GetSubmeshCount());
		for (int i = 0; i < glbMesh_.GetSubmeshCount(); i++)
		{
			if (!vertexColors_[i].CreateObjects(&device_, glbMesh_.GetSubmesh(i)->GetVerticesCount()))
			{
				return false;
			}
			cmdLists_[0].TransitionBarrier(&vertexColors_[i].colorB, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}
		cmdLists_[0].Close();
		cmdLists_[0].Execute();

		sl12::Fence fence;
		fence.Initialize(&device_);
		fence.Signal(cmdLists_[0].GetParentQueue());
		fence.WaitSignal();

		fence.Destroy();

		return true;
	}

	bool CreateAccelerationStructure()
	{
		// ASの生成はGPUで行うため、コマンドを積みGPUを動作させる必要があります.
		auto&& cmdList = cmdLists_[0];
		cmdList.Reset();

		// Bottom ASの生成準備
		std::vector< sl12::GeometryStructureDesc> geoDescs(glbMesh_.GetSubmeshCount());
		for (int i = 0; i < glbMesh_.GetSubmeshCount(); i++)
		{
			auto submesh = glbMesh_.GetSubmesh(i);
			geoDescs[i].InitializeAsTriangle(
				&submesh->GetPositionB(),
				&submesh->GetIndexB(),
				nullptr,
				submesh->GetPositionB().GetStride(),
				static_cast<UINT>(submesh->GetPositionB().GetSize() / submesh->GetPositionB().GetStride()),
				DXGI_FORMAT_R32G32B32_FLOAT,
				static_cast<UINT>(submesh->GetIndexB().GetSize()) / submesh->GetIndexB().GetStride(),
				DXGI_FORMAT_R32_UINT);
		}

		sl12::StructureInputDesc bottomInput{};
		if (!bottomInput.InitializeAsBottom(&device_, geoDescs.data(), glbMesh_.GetSubmeshCount(), D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE))
		{
			return false;
		}

		if (!bottomAS_.CreateBuffer(&device_, bottomInput.prebuildInfo.ResultDataMaxSizeInBytes, bottomInput.prebuildInfo.ScratchDataSizeInBytes))
		{
			return false;
		}

		// コマンド発行
		if (!bottomAS_.Build(&cmdList, bottomInput))
		{
			return false;
		}

		// Top ASの生成準備
		sl12::StructureInputDesc topInput{};
		if (!topInput.InitializeAsTop(&device_, 1, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE))
		{
			return false;
		}

		sl12::TopInstanceDesc topInstance{};
		DirectX::XMFLOAT4X4 mtx;
		DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(meshScale_, meshScale_, meshScale_);
		DirectX::XMStoreFloat4x4(&mtx, scale);
		topInstance.Initialize(mtx, &bottomAS_);

		if (!topAS_.CreateBuffer(&device_, topInput.prebuildInfo.ResultDataMaxSizeInBytes, topInput.prebuildInfo.ScratchDataSizeInBytes))
		{
			return false;
		}
		if (!topAS_.CreateInstanceBuffer(&device_, &topInstance, 1))
		{
			return false;
		}

		// コマンド発行
		if (!topAS_.Build(&cmdList, topInput, false))
		{
			return false;
		}

		// コマンド実行と終了待ち
		cmdList.Close();
		cmdList.Execute();
		device_.WaitDrawDone();

		bottomAS_.DestroyScratchBuffer();
		topAS_.DestroyScratchBuffer();
		topAS_.DestroyInstanceBuffer();

		return true;
	}

	bool CreateSceneCB()
	{
		// レイ生成シェーダで使用する定数バッファを生成する
		auto mtxWorldToView = DirectX::XMMatrixLookAtLH(
			DirectX::XMLoadFloat4(&camPos_),
			DirectX::XMLoadFloat4(&tgtPos_),
			DirectX::XMLoadFloat4(&upVec_));
		auto mtxViewToClip = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(60.0f), (float)kScreenWidth / (float)kScreenHeight, 0.01f, 100.0f);
		auto mtxWorldToClip = mtxWorldToView * mtxViewToClip;
		auto mtxClipToWorld = DirectX::XMMatrixInverse(nullptr, mtxWorldToClip);

		DirectX::XMFLOAT4 lightDir = { 0.0f, -1.0f, 0.0f, 0.0f };
		DirectX::XMStoreFloat4(&lightDir, DirectX::XMVector3Normalize(DirectX::XMLoadFloat4(&lightDir)));

		DirectX::XMFLOAT4 lightColor = { 1.0f, 1.0f, 1.0f, 1.0f };

		for (int i = 0; i < kBufferCount; i++)
		{
			if (!sceneCBs_[i].Initialize(&device_, sizeof(SceneCB), 0, sl12::BufferUsage::ConstantBuffer, true, false))
			{
				return false;
			}
			else
			{
				auto cb = reinterpret_cast<SceneCB*>(sceneCBs_[i].Map(nullptr));
				DirectX::XMStoreFloat4x4(&cb->mtxWorldToProj, mtxWorldToClip);
				DirectX::XMStoreFloat4x4(&cb->mtxProjToWorld, mtxClipToWorld);
				cb->camPos = camPos_;
				cb->lightDir = lightDir;
				cb->lightColor = lightColor;
				cb->skyPower = skyPower_;
				sceneCBs_[i].Unmap();

				if (!sceneCBVs_[i].Initialize(&device_, &sceneCBs_[i]))
				{
					return false;
				}
			}
		}

		return true;
	}

	bool CreateTimeCB()
	{
		// レイ生成シェーダで使用する定数バッファを生成する
		auto mtxWorldToView = DirectX::XMMatrixLookAtLH(
			DirectX::XMLoadFloat4(&camPos_),
			DirectX::XMLoadFloat4(&tgtPos_),
			DirectX::XMLoadFloat4(&upVec_));
		auto mtxViewToClip = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(60.0f), (float)kScreenWidth / (float)kScreenHeight, 0.01f, 100.0f);
		auto mtxWorldToClip = mtxWorldToView * mtxViewToClip;
		auto mtxClipToWorld = DirectX::XMMatrixInverse(nullptr, mtxWorldToClip);

		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < kBufferCount; j++)
			{
				if (!timeCBs_[i][j].Initialize(&device_, sizeof(TimeCB), 0, sl12::BufferUsage::ConstantBuffer, true, false))
				{
					return false;
				}
				else
				{
					auto cb = reinterpret_cast<TimeCB*>(timeCBs_[i][j].Map(nullptr));
					cb->loopCount = 0;
					timeCBs_[i][j].Unmap();

					if (!timeCBVs_[i][j].Initialize(&device_, &timeCBs_[i][j]))
					{
						return false;
					}
				}
			}
		}

		return true;
	}

	bool CreateShaderTableShadow()
	{
		// レイ生成シェーダ、ミスシェーダ、ヒットグループのIDを取得します.
		// 各シェーダ種別ごとにシェーダテーブルを作成しますが、このサンプルでは各シェーダ種別はそれぞれ1つのシェーダを持つことになります.
		void* rayGenShaderIdentifier;
		void* missShaderIdentifier;
		void* hitGroupShaderIdentifier;
		{
			ID3D12StateObjectProperties* prop;
			shadowRaySystem_.stateObject.GetPSO()->QueryInterface(IID_PPV_ARGS(&prop));
			rayGenShaderIdentifier = prop->GetShaderIdentifier(kRayGenName);
			missShaderIdentifier = prop->GetShaderIdentifier(kMissName);
			hitGroupShaderIdentifier = prop->GetShaderIdentifier(kHitGroupName);
			prop->Release();
		}

		UINT shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

		auto Align = [](UINT size, UINT align)
		{
			return ((size + align - 1) / align) * align;
		};

		// シェーダレコードサイズ
		// シェーダレコードはシェーダテーブルの要素1つです.
		// これはシェーダIDとローカルルートシグネチャに設定される変数の組み合わせで構成されています.
		// シェーダレコードのサイズはシェーダテーブル内で同一でなければならないため、同一シェーダテーブル内で最大のレコードサイズを指定すべきです.
		// 本サンプルではすべてのシェーダレコードについてサイズが同一となります.
		UINT descHandleOffset = Align(shaderIdentifierSize, sizeof(D3D12_GPU_DESCRIPTOR_HANDLE));
		UINT shaderRecordSize = Align(descHandleOffset + sizeof(D3D12_GPU_DESCRIPTOR_HANDLE) * 2, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
		shadowRaySystem_.shaderRecordSize = shaderRecordSize;

		auto GenShaderTable = [&](void** shaderIds, int shaderIdsCount, sl12::Buffer& buffer, int count = 1)
		{
			if (!buffer.Initialize(&device_, shaderRecordSize * count * shaderIdsCount, 0, sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
			{
				return false;
			}

			auto p = reinterpret_cast<char*>(buffer.Map(nullptr));

			for (int i = 0; i < count; ++i)
			{
				for (int id = 0; id < shaderIdsCount; ++id)
				{
					auto start = p;

					memcpy(p, shaderIds[id], shaderIdentifierSize);
					p += descHandleOffset;

					auto submesh = glbMesh_.GetSubmesh(i);
					auto material = glbMesh_.GetMaterial(submesh->GetMaterialIndex());
					auto texView = glbMesh_.GetTextureView(material->GetTexBaseColorIndex());

					auto texHandle = texView->GetDesc()->GetGpuHandle();
					auto samHandle = imageSampler_.GetDesc()->GetGpuHandle();
					memcpy(p, &texHandle, sizeof(texHandle)); p += sizeof(texHandle);
					memcpy(p, &samHandle, sizeof(samHandle)); p += sizeof(samHandle);

					p = start + shaderRecordSize;
				}
			}
			buffer.Unmap();

			return true;
		};

		if (!GenShaderTable(&rayGenShaderIdentifier, 1, shadowRaySystem_.rayGenTable))
		{
			return false;
		}
		if (!GenShaderTable(&missShaderIdentifier, 1, shadowRaySystem_.missTable))
		{
			return false;
		}
		if (!GenShaderTable(&hitGroupShaderIdentifier, 1, shadowRaySystem_.hitGroupTable, glbMesh_.GetSubmeshCount()))
		{
			return false;
		}

		return true;
	}

	bool CreateShaderTableVertexBake()
	{
		// レイ生成シェーダ、ミスシェーダ、ヒットグループのIDを取得します.
		// 各シェーダ種別ごとにシェーダテーブルを作成しますが、このサンプルでは各シェーダ種別はそれぞれ1つのシェーダを持つことになります.
		void* rayGenShaderIdentifier;
		void* missShaderIdentifier[2];
		void* hitGroupShaderIdentifier[2];
		{
			ID3D12StateObjectProperties* prop;
			vertexBakeSystem_.stateObject.GetPSO()->QueryInterface(IID_PPV_ARGS(&prop));
			rayGenShaderIdentifier = prop->GetShaderIdentifier(kRayGenName);
			missShaderIdentifier[0] = prop->GetShaderIdentifier(kMissName);
			missShaderIdentifier[1] = prop->GetShaderIdentifier(kMissShadowName);
			hitGroupShaderIdentifier[0] = prop->GetShaderIdentifier(kHitGroupName);
			hitGroupShaderIdentifier[1] = prop->GetShaderIdentifier(kHitGroupShadowName);
			prop->Release();
		}

		UINT shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

		auto Align = [](UINT size, UINT align)
		{
			return ((size + align - 1) / align) * align;
		};

		// シェーダレコードサイズ
		// シェーダレコードはシェーダテーブルの要素1つです.
		// これはシェーダIDとローカルルートシグネチャに設定される変数の組み合わせで構成されています.
		// シェーダレコードのサイズはシェーダテーブル内で同一でなければならないため、同一シェーダテーブル内で最大のレコードサイズを指定すべきです.
		// 本サンプルではすべてのシェーダレコードについてサイズが同一となります.
		UINT descHandleOffset = Align(shaderIdentifierSize, sizeof(D3D12_GPU_DESCRIPTOR_HANDLE));
		UINT shaderRecordSize = Align(descHandleOffset + sizeof(D3D12_GPU_DESCRIPTOR_HANDLE) * 6, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
		vertexBakeSystem_.shaderRecordSize = shaderRecordSize;

		auto GenShaderTable = [&](void** shaderIds, int shaderIdsCount, sl12::Buffer& buffer, int count = 1)
		{
			if (!buffer.Initialize(&device_, shaderRecordSize * count * shaderIdsCount, 0, sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
			{
				return false;
			}

			auto p = reinterpret_cast<char*>(buffer.Map(nullptr));

			for (int i = 0; i < count; ++i)
			{
				for (int id = 0; id < shaderIdsCount; ++id)
				{
					auto start = p;

					memcpy(p, shaderIds[id], shaderIdentifierSize);
					p += descHandleOffset;

					auto submesh = glbMesh_.GetSubmesh(i);
					auto material = glbMesh_.GetMaterial(submesh->GetMaterialIndex());
					auto texView = glbMesh_.GetTextureView(material->GetTexBaseColorIndex());

					auto indexHandle = submesh->GetIndexBV().GetDesc()->GetGpuHandle();
					auto posHandle = submesh->GetPositionBV().GetDesc()->GetGpuHandle();
					auto normalHandle = submesh->GetNormalBV().GetDesc()->GetGpuHandle();
					auto uvHandle = submesh->GetTexcoordBV().GetDesc()->GetGpuHandle();
					auto texHandle = texView->GetDesc()->GetGpuHandle();
					auto samHandle = imageSampler_.GetDesc()->GetGpuHandle();
					memcpy(p, &indexHandle, sizeof(indexHandle)); p += sizeof(indexHandle);
					memcpy(p, &posHandle, sizeof(posHandle)); p += sizeof(posHandle);
					memcpy(p, &normalHandle, sizeof(normalHandle)); p += sizeof(normalHandle);
					memcpy(p, &uvHandle, sizeof(uvHandle)); p += sizeof(uvHandle);
					memcpy(p, &texHandle, sizeof(texHandle)); p += sizeof(texHandle);
					memcpy(p, &samHandle, sizeof(samHandle)); p += sizeof(samHandle);

					p = start + shaderRecordSize;
				}
			}
			buffer.Unmap();

			return true;
		};

		if (!GenShaderTable(&rayGenShaderIdentifier, 1, vertexBakeSystem_.rayGenTable))
		{
			return false;
		}
		if (!GenShaderTable(missShaderIdentifier, 2, vertexBakeSystem_.missTable))
		{
			return false;
		}
		if (!GenShaderTable(hitGroupShaderIdentifier, 2, vertexBakeSystem_.hitGroupTable, glbMesh_.GetSubmeshCount()))
		{
			return false;
		}

		return true;
	}


	void UpdateSceneCB(int frameIndex)
	{
		auto mtxRot = DirectX::XMMatrixRotationY(camRotAngle_);
		auto cp = DirectX::XMLoadFloat4(&camPos_);
		cp = DirectX::XMVector4Transform(cp, mtxRot);

		auto mtxWorldToView = DirectX::XMMatrixLookAtLH(
			cp,
			DirectX::XMLoadFloat4(&tgtPos_),
			DirectX::XMLoadFloat4(&upVec_));
		auto mtxViewToClip = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(60.0f), (float)kScreenWidth / (float)kScreenHeight, 0.01f, 10000.0f);
		auto mtxWorldToClip = mtxWorldToView * mtxViewToClip;
		auto mtxClipToWorld = DirectX::XMMatrixInverse(nullptr, mtxWorldToClip);

		DirectX::XMFLOAT4 lightDir = { 0.1f, -1.0f, 0.1f, 0.0f };
		DirectX::XMStoreFloat4(&lightDir, DirectX::XMVector3Normalize(DirectX::XMLoadFloat4(&lightDir)));

		DirectX::XMFLOAT4 lightColor = { lightColor_[0] * lightPower_, lightColor_[1] * lightPower_, lightColor_[2] * lightPower_, 1.0f };

		DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(meshScale_, meshScale_, meshScale_);
		mtxWorldToClip = scale * mtxWorldToClip;

		{
			auto cb = reinterpret_cast<SceneCB*>(sceneCBs_[frameIndex].Map(nullptr));
			DirectX::XMStoreFloat4x4(&cb->mtxWorld, scale);
			DirectX::XMStoreFloat4x4(&cb->mtxWorldToProj, mtxWorldToClip);
			DirectX::XMStoreFloat4x4(&cb->mtxProjToWorld, mtxClipToWorld);
			DirectX::XMStoreFloat4(&cb->camPos, cp);
			cb->lightDir = lightDir;
			cb->lightColor = lightColor;
			cb->skyPower = skyPower_;
			cb->maxBounces = maxBounces_;
			sceneCBs_[frameIndex].Unmap();
		}
		{
			auto cb = reinterpret_cast<TimeCB*>(timeCBs_[0][frameIndex].Map(nullptr));
			cb->loopCount = shadowLoopCount_++;
			timeCBs_[0][frameIndex].Unmap();
		}
		{
			auto cb = reinterpret_cast<TimeCB*>(timeCBs_[1][frameIndex].Map(nullptr));
			cb->loopCount = bakeLoopCount_++;
			timeCBs_[1][frameIndex].Unmap();
		}
	}

private:
	static const int kBufferCount = sl12::Swapchain::kMaxBuffer;

	sl12::CommandList		cmdLists_[kBufferCount];

	DxrRenderSystem			shadowRaySystem_;
	DxrRenderSystem			vertexBakeSystem_;

	sl12::Texture				resultTexture_;
	sl12::TextureView			resultTextureSRV_;
	sl12::RenderTargetView		resultTextureRTV_;
	sl12::UnorderedAccessView	resultTextureUAV_;

	sl12::Buffer				randomBuffer_;
	sl12::BufferView			randomBufferSRV_;
	sl12::Buffer				seedBuffer_;
	sl12::UnorderedAccessView	seedBufferUAV_;

	float					meshScale_ = 20.0f;
	sl12::GlbMesh			glbMesh_;
	sl12::Sampler			imageSampler_;

	sl12::BottomAccelerationStructure	bottomAS_;
	sl12::TopAccelerationStructure		topAS_;

	sl12::Buffer				sceneCBs_[kBufferCount];
	sl12::ConstantBufferView	sceneCBVs_[kBufferCount];
	sl12::Buffer				timeCBs_[2][kBufferCount];
	sl12::ConstantBufferView	timeCBVs_[2][kBufferCount];

	std::vector<Sphere>		spheres_;
	sl12::Buffer			spheresAABB_;
	sl12::Buffer			instanceSB_;
	sl12::BufferView		instanceSBV_;

	sl12::Texture				gbufferTexture_;
	sl12::TextureView			gbufferTextureSRV_;
	sl12::RenderTargetView		gbufferTextureRTV_;

	sl12::Texture				depthTexture_;
	sl12::TextureView			depthTextureSRV_;
	sl12::DepthStencilView		depthTextureDSV_;

	sl12::Shader				zpreVS_, zprePS_;
	sl12::Shader				lightingVS_, lightingPS_;
	sl12::RootSignature			zpreRootSig_, lightingRootSig_;
	sl12::GraphicsPipelineState	zprePso_, lightingPso_;

	sl12::Shader				copyVS_, copyPS_;
	sl12::RootSignature			copyRootSig_;
	sl12::GraphicsPipelineState	copyPso_;

	std::vector<VertexLightColor>	vertexColors_;

	sl12::Gui				gui_;
	sl12::InputData			inputData_{};

	sl12::Timestamp			gpuTimestamp_[sl12::Swapchain::kMaxBuffer];

	DirectX::XMFLOAT4		camPos_ = { -5.0f, -5.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT4		tgtPos_ = { 0.0f, -5.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT4		upVec_ = { 0.0f, 1.0f, 0.0f, 0.0f };
	float					skyPower_ = 1.0f;
	float					lightColor_[3] = { 1.0f, 1.0f, 1.0f };
	float					lightPower_ = 1.0f;
	int						maxBounces_ = 4;
	uint32_t				shadowLoopCount_ = 0;
	uint32_t				bakeLoopCount_ = 0;
	float					camRotAngle_ = 0.0f;
	bool					isClearTarget_ = true;

	int		frameIndex_ = 0;
};	// class SampleApplication

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	SampleApplication app(hInstance, nCmdShow, kScreenWidth, kScreenHeight);

	return app.Run();
}

//	EOF
