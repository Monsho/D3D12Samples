#include <vector>

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
#include "sl12/glb_mesh.h"

#include "CompiledShaders/test.lib.hlsl.h"


namespace
{
	static const int	kScreenWidth = 1280;
	static const int	kScreenHeight = 720;

	static LPCWSTR		kRayGenName			= L"RayGenerator";
	static LPCWSTR		kClosestHitName		= L"ClosestHitProcessor";
	static LPCWSTR		kMissName			= L"MissProcessor";
	static LPCWSTR		kHitGroupName		= L"HitGroup";
}

class SampleApplication
	: public sl12::Application
{
	struct SceneCB
	{
		DirectX::XMFLOAT4X4	mtxProjToWorld;
		DirectX::XMFLOAT4	camPos;
		DirectX::XMFLOAT4	lightDir;
		DirectX::XMFLOAT4	lightColor;
	};

public:
	SampleApplication(HINSTANCE hInstance, int nCmdShow, int screenWidth, int screenHeight)
		: Application(hInstance, nCmdShow, screenWidth, screenHeight)
	{}

	bool Initialize() override
	{
		// �R�}���h���X�g�̏�����
		auto&& gqueue = device_.GetGraphicsQueue();
		for (auto&& v : cmdLists_)
		{
			if (!v.Initialize(&device_, &gqueue, true))
			{
				return false;
			}
		}

		// �e�N�X�`���ǂݍ���
		{
			sl12::File texFile("data/ConcreteTile_basecolor.tga");

			if (!imageTexture_.InitializeFromTGA(&device_, &cmdLists_[0], texFile.GetData(), texFile.GetSize(), false))
			{
				return false;
			}
			if (!imageTextureView_.Initialize(&device_, &imageTexture_))
			{
				return false;
			}

			D3D12_SAMPLER_DESC samDesc{};
			samDesc.AddressU = samDesc.AddressV = samDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			if (!imageSampler_.Initialize(&device_, samDesc))
			{
				return false;
			}
		}

		// ���[�g�V�O�l�`���̏�����
		{
			D3D12_DESCRIPTOR_RANGE ranges[] = {
				{ D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
				{ D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
			};

			D3D12_ROOT_PARAMETER params[3];
			params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			params[0].DescriptorTable.NumDescriptorRanges = 1;
			params[0].DescriptorTable.pDescriptorRanges = &ranges[0];
			params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			params[1].DescriptorTable.NumDescriptorRanges = 1;
			params[1].DescriptorTable.pDescriptorRanges = &ranges[1];
			params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
			params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			params[2].Descriptor.ShaderRegister = 0;
			params[2].Descriptor.RegisterSpace = 0;

			D3D12_ROOT_SIGNATURE_DESC sigDesc{};
			sigDesc.NumParameters = ARRAYSIZE(params);
			sigDesc.pParameters = params;
			sigDesc.NumStaticSamplers = 0;
			sigDesc.pStaticSamplers = nullptr;
			sigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

			if (!globalRootSig_.Initialize(&device_, sigDesc))
			{
				return false;
			}
		}
		{
			sl12::RootParameter params[] = {
				sl12::RootParameter(sl12::RootParameterType::ShaderResource, sl12::ShaderVisibility::All, 1),
				sl12::RootParameter(sl12::RootParameterType::ShaderResource, sl12::ShaderVisibility::All, 2),
				sl12::RootParameter(sl12::RootParameterType::ShaderResource, sl12::ShaderVisibility::All, 3),
				sl12::RootParameter(sl12::RootParameterType::Sampler, sl12::ShaderVisibility::All, 0),
			};
			sl12::RootSignatureDesc desc;
			desc.pParameters = params;
			desc.numParameters = ARRAYSIZE(params);
			desc.flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
			if (!localRootSig_.Initialize(&device_, desc))
			{
				return false;
			}
		}

		// �p�C�v���C���X�e�[�g�I�u�W�F�N�g�̏�����
		if (!CreatePipelineState())
		{
			return false;
		}

		// �o�͐�̃e�N�X�`���𐶐�
		{
			sl12::TextureDesc desc;
			desc.dimension = sl12::TextureDimension::Texture2D;
			desc.width = kScreenWidth;
			desc.height = kScreenHeight;
			desc.mipLevels = 1;
			desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			desc.sampleCount = 1;
			desc.clearColor[4] = { 0.0f };
			desc.clearDepth = 1.0f;
			desc.clearStencil = 0;
			desc.isRenderTarget = false;
			desc.isDepthBuffer = false;
			desc.isUav = true;
			if (!resultTexture_.Initialize(&device_, desc))
			{
				return false;
			}

			if (!resultTextureView_.Initialize(&device_, &resultTexture_))
			{
				return false;
			}
		}

		// �W�I���g���𐶐�����
		if (!CreateGeometry())
		{
			return false;
		}

		// AS�𐶐�����
		if (!CreateAccelerationStructure())
		{
			return false;
		}

		// �V�[���萔�o�b�t�@�𐶐�����
		if (!CreateSceneCB())
		{
			return false;
		}

		// �V�F�[�_�e�[�u���𐶐�����
		if (!CreateShaderTable())
		{
			return false;
		}

		return true;
	}

	bool Execute() override
	{
		device_.WaitPresent();

		auto frameIndex = (device_.GetSwapchain().GetFrameIndex() + sl12::Swapchain::kMaxBuffer - 1) % sl12::Swapchain::kMaxBuffer;
		auto&& cmdList = cmdLists_[frameIndex];
		auto&& d3dCmdList = cmdList.GetCommandList();
		auto&& dxrCmdList = cmdList.GetDxrCommandList();

		UpdateSceneCB(frameIndex);

		cmdList.Reset();

		auto&& swapchain = device_.GetSwapchain();
		cmdList.TransitionBarrier(swapchain.GetCurrentTexture(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		float color[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
		d3dCmdList->ClearRenderTargetView(swapchain.GetCurrentRenderTargetView()->GetDesc()->GetCpuHandle(), color, 0, nullptr);

		// �O���[�o�����[�g�V�O�l�`����ݒ�
		d3dCmdList->SetComputeRootSignature(globalRootSig_.GetRootSignature());

		// �f�X�N���v�^�q�[�v��ݒ�
		ID3D12DescriptorHeap* pDescHeaps[] = {
			device_.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetHeap(),
			device_.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER).GetHeap()
		};
		d3dCmdList->SetDescriptorHeaps(ARRAYSIZE(pDescHeaps), pDescHeaps);

		// �O���[�o���ݒ�̃V�F�[�_���\�[�X��ݒ肷��
		d3dCmdList->SetComputeRootDescriptorTable(0, resultTextureView_.GetDesc()->GetGpuHandle());
		d3dCmdList->SetComputeRootDescriptorTable(1, sceneCBVs_[frameIndex].GetDesc()->GetGpuHandle());
		d3dCmdList->SetComputeRootShaderResourceView(2, topAS_.GetDxrBuffer().GetResourceDep()->GetGPUVirtualAddress());
		//d3dCmdList->SetComputeRootShaderResourceView(1, topAS_.GetResourceDep()->GetGPUVirtualAddress());

		// ���C�g���[�X�����s
		D3D12_DISPATCH_RAYS_DESC desc{};
		desc.HitGroupTable.StartAddress = hitGroupTable_.GetResourceDep()->GetGPUVirtualAddress();
		desc.HitGroupTable.SizeInBytes = hitGroupTable_.GetSize();
		desc.HitGroupTable.StrideInBytes = desc.HitGroupTable.SizeInBytes;
		desc.MissShaderTable.StartAddress = missTable_.GetResourceDep()->GetGPUVirtualAddress();
		desc.MissShaderTable.SizeInBytes = missTable_.GetSize();
		desc.MissShaderTable.StrideInBytes = desc.MissShaderTable.SizeInBytes;
		desc.RayGenerationShaderRecord.StartAddress = rayGenTable_.GetResourceDep()->GetGPUVirtualAddress();
		desc.RayGenerationShaderRecord.SizeInBytes = rayGenTable_.GetSize();
		desc.Width = kScreenWidth;
		desc.Height = kScreenHeight;
		desc.Depth = 1;
		dxrCmdList->SetPipelineState1(stateObject_.GetPSO());
		dxrCmdList->DispatchRays(&desc);

		cmdList.UAVBarrier(&resultTexture_);

		// ���\�[�X�o���A
		cmdList.TransitionBarrier(swapchain.GetCurrentTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
		cmdList.TransitionBarrier(&resultTexture_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);

		d3dCmdList->CopyResource(swapchain.GetCurrentTexture()->GetResourceDep(), resultTexture_.GetResourceDep());

		cmdList.TransitionBarrier(swapchain.GetCurrentTexture(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
		cmdList.TransitionBarrier(&resultTexture_, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		// �R�}���h�I���Ǝ��s
		cmdList.Close();
		cmdList.Execute();
		device_.WaitDrawDone();

		// ���̃t���[����
		device_.Present(1);

		return true;
	}

	void Finalize() override
	{
		rayGenTable_.Destroy();
		missTable_.Destroy();
		hitGroupTable_.Destroy();

		for (auto&& v : sceneCBVs_) v.Destroy();
		for (auto&& v : sceneCBs_) v.Destroy();

		topAS_.Destroy();
		bottomAS_.Destroy();

		glbMesh_.Destroy();
		geometryIBV_.Destroy();
		geometryIB_.Destroy();
		geometryUVBV_.Destroy();
		geometryUVB_.Destroy();
		geometryVB_.Destroy();

		resultTextureView_.Destroy();
		resultTexture_.Destroy();

		stateObject_.Destroy();

		localRootSig_.Destroy();
		globalRootSig_.Destroy();

		imageSampler_.Destroy();
		imageTextureView_.Destroy();
		imageTexture_.Destroy();

		for (auto&& v : cmdLists_) v.Destroy();
	}

private:
	bool CreateRootSig(const D3D12_ROOT_SIGNATURE_DESC& desc, ID3D12RootSignature** ppSig)
	{
		ID3DBlob* blob = nullptr;
		ID3DBlob* error = nullptr;
		bool ret = true;

		auto hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
		if (FAILED(hr))
		{
			ret = false;
			goto D3D_ERROR;
		}

		hr = device_.GetDeviceDep()->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(ppSig));
		if (FAILED(hr))
		{
			ret = false;
			goto D3D_ERROR;
		}

	D3D_ERROR:
		sl12::SafeRelease(blob);
		sl12::SafeRelease(error);
		return ret;
	}

	bool CreatePipelineState()
	{
		// DXR�p�̃p�C�v���C���X�e�[�g�𐶐����܂�.
		// Graphics�p�ACompute�p�̃p�C�v���C���X�e�[�g�͐������ɌŒ�T�C�Y�̋L�q�q��p�ӂ��܂�.
		// ����ɑ΂��āADXR�p�̃p�C�v���C���X�e�[�g�͉ό̃T�u�I�u�W�F�N�g��K�v�Ƃ��܂�.
		// �܂��A�T�u�I�u�W�F�N�g�̒��ɂ͑��̃T�u�I�u�W�F�N�g�ւ̃|�C���^��K�v�Ƃ�����̂����邽�߁A�T�u�I�u�W�F�N�g�z��̐����ɂ͒��ӂ��K�v�ł�.

		sl12::DxrPipelineStateDesc dxrDesc;

		// DXIL���C�u�����T�u�I�u�W�F�N�g
		// 1�̃V�F�[�_���C�u�����ƁA�����ɓo�^����Ă���V�F�[�_�̃G���g���[�|�C���g���G�N�X�|�[�g���邽�߂̃T�u�I�u�W�F�N�g�ł�.
		D3D12_EXPORT_DESC libExport[] = {
			{ kRayGenName,		nullptr, D3D12_EXPORT_FLAG_NONE },
			{ kClosestHitName,	nullptr, D3D12_EXPORT_FLAG_NONE },
			{ kMissName,		nullptr, D3D12_EXPORT_FLAG_NONE },
		};
		dxrDesc.AddDxilLibrary(g_pTestLib, sizeof(g_pTestLib), libExport, ARRAYSIZE(libExport));

		// �q�b�g�O���[�v�T�u�I�u�W�F�N�g
		// Intersection, AnyHit, ClosestHit�̑g�ݍ��킹���`���A�q�b�g�O���[�v���ł܂Ƃ߂�T�u�I�u�W�F�N�g�ł�.
		// �}�e���A�����Ƃ�p�r����(�}�e���A���A�V���h�E�Ȃ�)�ɃT�u�I�u�W�F�N�g��p�ӂ��܂�.
		dxrDesc.AddHitGroup(kHitGroupName, true, nullptr, kClosestHitName, nullptr);

		// �V�F�[�_�R���t�B�O�T�u�I�u�W�F�N�g
		// �q�b�g�V�F�[�_�A�~�X�V�F�[�_�̈����ƂȂ�Payload, IntersectionAttributes�̍ő�T�C�Y��ݒ肵�܂�.
		dxrDesc.AddShaderConfig(sizeof(float) * 4, sizeof(float) * 2);

		// ���[�J�����[�g�V�O�l�`���T�u�I�u�W�F�N�g
		// �V�F�[�_���R�[�h���Ƃɐݒ肳��郋�[�g�V�O�l�`����ݒ肵�܂�.

		// Exports Assosiation �T�u�I�u�W�F�N�g
		// �V�F�[�_���R�[�h�ƃ��[�J�����[�g�V�O�l�`���̃o�C���h���s���T�u�I�u�W�F�N�g�ł�.
		LPCWSTR kExports[] = {
			kRayGenName,
			kMissName,
			kHitGroupName,
		};
		dxrDesc.AddLocalRootSignatureAndExportAssociation(localRootSig_, kExports, ARRAYSIZE(kExports));

		// �O���[�o�����[�g�V�O�l�`���T�u�I�u�W�F�N�g
		// ���ׂẴV�F�[�_�e�[�u���ŎQ�Ƃ����O���[�o���ȃ��[�g�V�O�l�`����ݒ肵�܂�.
		dxrDesc.AddGlobalRootSignature(globalRootSig_);

		// ���C�g���[�X�R���t�B�O�T�u�I�u�W�F�N�g
		// TraceRay()���s�����Ƃ��ł���ő�[�x���w�肷��T�u�I�u�W�F�N�g�ł�.
		dxrDesc.AddRaytracinConfig(1);

		// PSO����
		if (!stateObject_.Initialize(&device_, dxrDesc))
		{
			return false;
		}

		return true;
	}

	bool CreateGeometry()
	{
		const float size = 2.f;
		float vertices[] = {
			-size,  size, -size,
			 size,  size, -size,
			-size,  size,  size,
			 size,  size,  size,

			 size, -size, -size,
			-size, -size, -size,
			 size, -size,  size,
			-size, -size,  size,

			 size,  size, -size,
			 size,  size,  size,
			 size, -size, -size,
			 size, -size,  size,

			-size,  size, -size,
			-size,  size,  size,
			-size, -size, -size,
			-size, -size,  size,

			-size,  size, -size,
			 size,  size, -size,
			-size, -size, -size,
			 size, -size, -size,

			-size,  size,  size,
			 size,  size,  size,
			-size, -size,  size,
			 size, -size,  size,
		};
		float uv[] = {
			0.0f, 0.0f,
			1.0f, 0.0f,
			0.0f, 1.0f,
			1.0f, 1.0f,

			1.0f, 0.0f,
			0.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 1.0f,

			1.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 0.0f,
			0.0f, 1.0f,

			1.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 0.0f,
			0.0f, 1.0f,

			0.0f, 1.0f,
			1.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,

			0.0f, 1.0f,
			1.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,
		};
		UINT32 indices[] =
		{
			0, 2, 1, 1, 2, 3,
			4, 6, 5, 5, 6, 7,
			8, 9, 10, 9, 11, 10,
			12, 14, 13, 13, 14, 15,
			16, 17, 18, 17, 19, 18,
			20, 22, 21, 21, 22, 23,
		};

		if (!geometryVB_.Initialize(&device_, sizeof(vertices), sizeof(float) * 3, sl12::BufferUsage::ShaderResource, true, false))
		{
			return false;
		}
		if (!geometryUVB_.Initialize(&device_, sizeof(uv), sizeof(float) * 2, sl12::BufferUsage::ShaderResource, true, false))
		{
			return false;
		}
		if (!geometryIB_.Initialize(&device_, sizeof(indices), sizeof(indices[0]), sl12::BufferUsage::ShaderResource, true, false))
		{
			return false;
		}

		void* p = geometryVB_.Map(nullptr);
		memcpy(p, vertices, sizeof(vertices));
		geometryVB_.Unmap();

		p = geometryUVB_.Map(nullptr);
		memcpy(p, uv, sizeof(uv));
		geometryUVB_.Unmap();

		p = geometryIB_.Map(nullptr);
		memcpy(p, indices, sizeof(indices));
		geometryIB_.Unmap();

		if (!geometryIBV_.Initialize(&device_, &geometryIB_, 0, 0))
		{
			return false;
		}
		if (!geometryUVBV_.Initialize(&device_, &geometryUVB_, 0, sizeof(float) * 2))
		{
			return false;
		}

		if (!glbMesh_.Initialize(&device_, &cmdLists_[0], "data/", "PreviewSphere.glb"))
		{
			return false;
		}

		return true;
	}

	bool CreateAccelerationStructure()
	{
		// AS�̐�����GPU�ōs�����߁A�R�}���h��ς�GPU�𓮍삳����K�v������܂�.
		auto&& cmdList = cmdLists_[0];
		cmdList.Reset();

		// Bottom AS�̐�������
		sl12::GeometryStructureDesc geoDesc{};
		auto submesh = glbMesh_.GetSubmesh(0);
		geoDesc.InitializeAsTriangle(
			&submesh->GetPositionB(),
			&submesh->GetIndexB(),
			nullptr,
			submesh->GetPositionB().GetStride(),
			static_cast<UINT>(submesh->GetPositionB().GetSize() / submesh->GetPositionB().GetStride()),
			DXGI_FORMAT_R32G32B32_FLOAT,
			static_cast<UINT>(submesh->GetIndexB().GetSize()) / submesh->GetIndexB().GetStride(),
			DXGI_FORMAT_R32_UINT);
		//geoDesc.InitializeAsTriangle(
		//	&geometryVB_,
		//	&geometryIB_,
		//	nullptr,
		//	geometryVB_.GetStride(),
		//	static_cast<UINT>(geometryVB_.GetSize() / geometryVB_.GetStride()),
		//	DXGI_FORMAT_R32G32B32_FLOAT,
		//	static_cast<UINT>(geometryIB_.GetSize()) / geometryIB_.GetStride(),
		//	DXGI_FORMAT_R32_UINT);

		sl12::StructureInputDesc bottomInput{};
		if (!bottomInput.InitializeAsBottom(&device_, &geoDesc, 1, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE))
		{
			return false;
		}

		if (!bottomAS_.CreateBuffer(&device_, bottomInput.prebuildInfo.ResultDataMaxSizeInBytes, bottomInput.prebuildInfo.ScratchDataSizeInBytes))
		{
			return false;
		}

		// �R�}���h���s
		if (!bottomAS_.Build(&cmdList, bottomInput))
		{
			return false;
		}

		// Top AS�̐�������
		sl12::StructureInputDesc topInput{};
		if (!topInput.InitializeAsTop(&device_, 1, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE))
		{
			return false;
		}

		sl12::TopInstanceDesc topInstance{};
		topInstance.Initialize(&bottomAS_);

		if (!topAS_.CreateBuffer(&device_, topInput.prebuildInfo.ResultDataMaxSizeInBytes, topInput.prebuildInfo.ScratchDataSizeInBytes))
		{
			return false;
		}
		if (!topAS_.CreateInstanceBuffer(&device_, &topInstance, 1))
		{
			return false;
		}

		// �R�}���h���s
		if (!topAS_.Build(&cmdList, topInput, false))
		{
			return false;
		}

		// �R�}���h���s�ƏI���҂�
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
		// ���C�����V�F�[�_�Ŏg�p����萔�o�b�t�@�𐶐�����
		auto mtxWorldToView = DirectX::XMMatrixLookAtLH(
			DirectX::XMLoadFloat4(&camPos_),
			DirectX::XMLoadFloat4(&tgtPos_),
			DirectX::XMLoadFloat4(&upVec_));
		auto mtxViewToClip = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(60.0f), (float)kScreenWidth / (float)kScreenHeight, 0.01f, 100.0f);
		auto mtxWorldToClip = mtxWorldToView * mtxViewToClip;
		auto mtxClipToWorld = DirectX::XMMatrixInverse(nullptr, mtxWorldToClip);

		DirectX::XMFLOAT4 lightDir = { 1.0f, -1.0f, -1.0f, 0.0f };
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
				DirectX::XMStoreFloat4x4(&cb->mtxProjToWorld, mtxClipToWorld);
				cb->camPos = camPos_;
				cb->lightDir = lightDir;
				cb->lightColor = lightColor;
				sceneCBs_[i].Unmap();

				if (!sceneCBVs_[i].Initialize(&device_, &sceneCBs_[i]))
				{
					return false;
				}
			}
		}

		return true;
	}

	bool CreateShaderTable()
	{
		// ���C�����V�F�[�_�A�~�X�V�F�[�_�A�q�b�g�O���[�v��ID���擾���܂�.
		// �e�V�F�[�_��ʂ��ƂɃV�F�[�_�e�[�u�����쐬���܂����A���̃T���v���ł͊e�V�F�[�_��ʂ͂��ꂼ��1�̃V�F�[�_�������ƂɂȂ�܂�.
		void* rayGenShaderIdentifier;
		void* missShaderIdentifier;
		void* hitGroupShaderIdentifier;
		{
			ID3D12StateObjectProperties* prop;
			stateObject_.GetPSO()->QueryInterface(IID_PPV_ARGS(&prop));
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

		// �V�F�[�_���R�[�h�T�C�Y
		// �V�F�[�_���R�[�h�̓V�F�[�_�e�[�u���̗v�f1�ł�.
		// ����̓V�F�[�_ID�ƃ��[�J�����[�g�V�O�l�`���ɐݒ肳���ϐ��̑g�ݍ��킹�ō\������Ă��܂�.
		// �V�F�[�_���R�[�h�̃T�C�Y�̓V�F�[�_�e�[�u�����œ���łȂ���΂Ȃ�Ȃ����߁A����V�F�[�_�e�[�u�����ōő�̃��R�[�h�T�C�Y���w�肷�ׂ��ł�.
		// �{�T���v���ł͂��ׂẴV�F�[�_���R�[�h�ɂ��ăT�C�Y������ƂȂ�܂�.
		UINT descHandleOffset = Align(shaderIdentifierSize, sizeof(D3D12_GPU_DESCRIPTOR_HANDLE));
		UINT shaderRecordSize = Align(descHandleOffset + sizeof(D3D12_GPU_DESCRIPTOR_HANDLE) * 4, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

		auto GenShaderTable = [&](void* shaderId, sl12::Buffer& buffer)
		{
			if (!buffer.Initialize(&device_, shaderRecordSize, 0, sl12::BufferUsage::ShaderResource, D3D12_RESOURCE_STATE_GENERIC_READ, true, false))
			{
				return false;
			}

			auto p = reinterpret_cast<char*>(buffer.Map(nullptr));
			memcpy(p, shaderId, shaderIdentifierSize);
			p += descHandleOffset;

			auto texHandle = imageTextureView_.GetDesc()->GetGpuHandle();
			auto uvHandle = glbMesh_.GetSubmesh(0)->GetTexcoordBV().GetDesc()->GetGpuHandle();
			auto indexHandle = glbMesh_.GetSubmesh(0)->GetIndexBV().GetDesc()->GetGpuHandle();
			//auto uvHandle = geometryUVBV_.GetDesc()->GetGpuHandle();
			//auto indexHandle = geometryIBV_.GetDesc()->GetGpuHandle();
			auto samHandle = imageSampler_.GetDesc()->GetGpuHandle();
			memcpy(p, &texHandle, sizeof(texHandle)); p += sizeof(texHandle);
			memcpy(p, &uvHandle, sizeof(uvHandle)); p += sizeof(uvHandle);
			memcpy(p, &indexHandle, sizeof(indexHandle)); p += sizeof(indexHandle);
			memcpy(p, &samHandle, sizeof(samHandle)); p += sizeof(samHandle);
			buffer.Unmap();

			return true;
		};

		if (!GenShaderTable(rayGenShaderIdentifier, rayGenTable_))
		{
			return false;
		}
		if (!GenShaderTable(missShaderIdentifier, missTable_))
		{
			return false;
		}
		if (!GenShaderTable(hitGroupShaderIdentifier, hitGroupTable_))
		{
			return false;
		}

		return true;
	}

	void UpdateSceneCB(int frameIndex)
	{
		auto mtxRot = DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(1.0f));
		auto cp = DirectX::XMLoadFloat4(&camPos_);
		cp = DirectX::XMVector4Transform(cp, mtxRot);
		DirectX::XMStoreFloat4(&camPos_, cp);

		auto mtxWorldToView = DirectX::XMMatrixLookAtLH(
			cp,
			DirectX::XMLoadFloat4(&tgtPos_),
			DirectX::XMLoadFloat4(&upVec_));
		auto mtxViewToClip = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(60.0f), (float)kScreenWidth / (float)kScreenHeight, 0.01f, 100.0f);
		auto mtxWorldToClip = mtxWorldToView * mtxViewToClip;
		auto mtxClipToWorld = DirectX::XMMatrixInverse(nullptr, mtxWorldToClip);

		DirectX::XMFLOAT4 lightDir = { 1.0f, -1.0f, -1.0f, 0.0f };
		DirectX::XMStoreFloat4(&lightDir, DirectX::XMVector3Normalize(DirectX::XMLoadFloat4(&lightDir)));

		DirectX::XMFLOAT4 lightColor = { 1.0f, 1.0f, 1.0f, 1.0f };

		auto cb = reinterpret_cast<SceneCB*>(sceneCBs_[frameIndex].Map(nullptr));
		DirectX::XMStoreFloat4x4(&cb->mtxProjToWorld, mtxClipToWorld);
		cb->camPos = camPos_;
		cb->lightDir = lightDir;
		cb->lightColor = lightColor;
		sceneCBs_[frameIndex].Unmap();
	}

private:
	static const int kBufferCount = sl12::Swapchain::kMaxBuffer;

	sl12::CommandList		cmdLists_[kBufferCount];
	sl12::RootSignature		globalRootSig_, localRootSig_;

	sl12::DxrPipelineState		stateObject_;
	sl12::Texture				resultTexture_;
	sl12::UnorderedAccessView	resultTextureView_;

	sl12::Texture			imageTexture_;
	sl12::TextureView		imageTextureView_;
	sl12::Sampler			imageSampler_;

	sl12::GlbMesh			glbMesh_;
	sl12::Buffer			geometryVB_, geometryIB_, geometryUVB_;
	sl12::BufferView		geometryIBV_, geometryUVBV_;

	sl12::BottomAccelerationStructure	bottomAS_;
	sl12::TopAccelerationStructure		topAS_;

	sl12::Buffer				sceneCBs_[kBufferCount];
	sl12::ConstantBufferView	sceneCBVs_[kBufferCount];

	sl12::Buffer			rayGenTable_, missTable_, hitGroupTable_;

	DirectX::XMFLOAT4		camPos_ = { 5.0f, 5.0f, -5.0f, 1.0f };
	DirectX::XMFLOAT4		tgtPos_ = { 0.0f, 0.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT4		upVec_ = { 0.0f, 1.0f, 0.0f, 0.0f };

	int		frameIndex_ = 0;
};	// class SampleApplication

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	SampleApplication app(hInstance, nCmdShow, kScreenWidth, kScreenHeight);

	return app.Run();
}

//	EOF