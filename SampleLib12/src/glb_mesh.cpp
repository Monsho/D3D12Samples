#include "sl12/glb_mesh.h"

#include <cstdio>
#include <memory>
#include <fstream>
#include <set>

#include "GLTFSDK/GLTF.h"
#include "GLTFSDK/GLBResourceReader.h"
#include "GLTFSDK/Deserialize.h"

#include "sl12/util.h"

#include "../../External/UVAtlas/UVAtlas/inc/UVAtlas.h"
#include "../../External/DirectXMesh/DirectXMesh/DirectXMesh.h"

#include "../../External/stb/stb_image.h"


using namespace Microsoft::glTF;

namespace sl12
{
	namespace
	{

		class StreamReader : public IStreamReader
		{
		public:
			StreamReader(const std::string& p)
				: path_(p)
			{}

			std::shared_ptr<std::istream> GetInputStream(const std::string& filename) const override
			{
				auto stream = std::make_shared<std::ifstream>(path_ + filename, std::ios_base::binary);
				return stream;
			}

		private:
			std::string		path_;
		};

	}


	//-----------------------------------------------------------------------------
	// サブメッシュの初期化
	//-----------------------------------------------------------------------------
	bool GlbSubmesh::Initialize(Device* pDev, const Microsoft::glTF::Document& doc, const Microsoft::glTF::MeshPrimitive& mesh, Microsoft::glTF::GLTFResourceReader& resReader)
	{
		materialIndex_ = std::stoi(mesh.materialId);

		// インデックス取得
		{
			auto&& index_accessor = doc.accessors.Get(mesh.indicesAccessorId);
			auto index_data = resReader.ReadBinaryData<u32>(doc, index_accessor);
			auto index_count = index_data.size();

			indexBuffer_.source_data_.reset(new u8[index_count * sizeof(u32)]);
			memcpy(indexBuffer_.source_data_.get(), index_data.data(), index_count * sizeof(u32));
			indicesCount_ = (int)index_count;
		}

		// 頂点バッファ作成
		auto CreateVB = [&](BufferBundle<VertexBufferView>& bb, const char* bufferName, int need_elem, int* verticesCount = nullptr)
		{
			std::string accessorId;
			if (mesh.TryGetAttributeAccessorId(bufferName, accessorId))
			{
				auto&& accessor = doc.accessors.Get(accessorId);
				if (accessor.componentType != Microsoft::glTF::COMPONENT_FLOAT)
					return false;

				auto data = resReader.ReadBinaryData<float>(doc, accessor);
				int elem_count = 0;
				if (accessor.type == Microsoft::glTF::AccessorType::TYPE_VEC2)
					elem_count = 2;
				else if (accessor.type == Microsoft::glTF::AccessorType::TYPE_VEC3)
					elem_count = 3;
				else
					return false;

				if (elem_count != need_elem)
					return false;

				auto count = data.size();
				bb.source_data_.reset(new u8[count * sizeof(float)]);
				memcpy(bb.source_data_.get(), data.data(), count * sizeof(float));

				if (verticesCount)
				{
					*verticesCount = (int)(count / elem_count);
				}
			}

			return true;
		};
		{
			if (!CreateVB(positionBuffer_, "POSITION", 3, &verticesCount_))
			{
				return false;
			}
			if (!CreateVB(normalBuffer_, "NORMAL", 3))
			{
				return false;
			}
			if (!CreateVB(texcoordBuffer_, "TEXCOORD_0", 2))
			{
				return false;
			}
		}

		if (!CreateBuffers(pDev))
		{
			return false;
		}

		return true;
	}

	//-----------------------------------------------------------------------------
	// 取得済みのデータから各種バッファを生成する
	//-----------------------------------------------------------------------------
	bool GlbSubmesh::CreateBuffers(Device* pDev)
	{
		// インデックスバッファ作成
		{
			indexBuffer_.buffer_.Destroy();
			indexBuffer_.buffer_view_.Destroy();
			indexBuffer_.view_.Destroy();

			if (!indexBuffer_.buffer_.Initialize(pDev, indicesCount_ * sizeof(u32), sizeof(u32), BufferUsage::IndexBuffer, true, false))
			{
				return false;
			}
			if (!indexBuffer_.buffer_view_.Initialize(pDev, &indexBuffer_.buffer_, 0, 0, 0))
			{
				return false;
			}
			if (!indexBuffer_.view_.Initialize(pDev, &indexBuffer_.buffer_))
			{
				return false;
			}

			auto p = indexBuffer_.buffer_.Map(nullptr);
			memcpy(p, indexBuffer_.source_data_.get(), indicesCount_ * sizeof(u32));
			indexBuffer_.buffer_.Unmap();
		}

		// 頂点バッファ作成
		auto CreateVB = [&](BufferBundle<VertexBufferView>& bb, int elem_count)
		{
			bb.buffer_.Destroy();
			bb.buffer_view_.Destroy();
			bb.view_.Destroy();

			if (bb.source_data_.get() == nullptr)
				return true;

			auto count = verticesCount_ * elem_count;
			if (!bb.buffer_.Initialize(pDev, count * sizeof(float), sizeof(float) * elem_count, BufferUsage::VertexBuffer, true, false))
			{
				return false;
			}
			if (!bb.buffer_view_.Initialize(pDev, &bb.buffer_, 0, 0, sizeof(float) * elem_count))
			{
				return false;
			}
			if (!bb.view_.Initialize(pDev, &bb.buffer_))
			{
				return false;
			}

			auto p = bb.buffer_.Map(nullptr);
			memcpy(p, bb.source_data_.get(), count * sizeof(float));
			bb.buffer_.Unmap();

			return true;
		};
		{
			if (!CreateVB(positionBuffer_, 3))
			{
				return false;
			}
			if (!CreateVB(normalBuffer_, 3))
			{
				return false;
			}
			if (!CreateVB(texcoordBuffer_, 2))
			{
				return false;
			}
			if (!CreateVB(uniqueUvBuffer_, 2))
			{
				return false;
			}
		}

		return true;
	}

	//-----------------------------------------------------------------------------
	// サブメッシュの破棄
	//-----------------------------------------------------------------------------
	void GlbSubmesh::Destroy()
	{
		materialIndex_ = -1;
	}


	//-----------------------------------------------------------------------------
	// マテリアルの初期化
	//-----------------------------------------------------------------------------
	bool GlbMaterial::Initialize(Device* pDev, const Microsoft::glTF::Document& doc, const Microsoft::glTF::Material& mat, Microsoft::glTF::GLTFResourceReader& resReader)
	{
		name_ = mat.name;
		texBaseColorIndex_ = std::stoi(mat.metallicRoughness.baseColorTexture.textureId);
		texMetalRoughIndex_ = std::stoi(mat.metallicRoughness.metallicRoughnessTexture.textureId);
		texNormalIndex_ = std::stoi(mat.normalTexture.textureId);

		const BlendMode::Value kBlend[] = {
			BlendMode::Opaque,		// ALPHA_UNKNOWN 
			BlendMode::Opaque,		// ALPHA_OPAQUE
			BlendMode::Blend,		// ALPHA_BLEND
			BlendMode::Mask,		// ALPHA_MASK
		};
		blendMode_ = kBlend[mat.alphaMode];

		return true;
	}

	//-----------------------------------------------------------------------------
	// マテリアルの破棄
	//-----------------------------------------------------------------------------
	void GlbMaterial::Destroy()
	{
		texBaseColorIndex_ = texMetalRoughIndex_ = texNormalIndex_ = -1;
	}


	//-----------------------------------------------------------------------------
	// メッシュの初期化
	//-----------------------------------------------------------------------------
	bool GlbMesh::Initialize(Device* pDev, CommandList* pCmdList, const char* pathname, const char* filename)
	{
		auto streamReader = std::make_unique<StreamReader>(pathname);
		auto glbStream = streamReader->GetInputStream(filename);
		auto glbResourceReader = std::make_unique<GLBResourceReader>(std::move(streamReader), std::move(glbStream));
		auto manifest = glbResourceReader->GetJson();

		auto document = Deserialize(manifest);

		// イメージ生成
		textures_.resize(document.images.Size());
		auto texture = textures_.data();
		for (auto&& image : document.images.Elements())
		{
			auto data = glbResourceReader->ReadBinaryData(document, image);

			texture->pTex = new Texture();
			texture->pView = new TextureView();

			if (!texture->pTex->InitializeFromPNG(pDev, pCmdList, data.data(), data.size(), 0, false))
			{
				return false;
			}
			if (!texture->pView->Initialize(pDev, texture->pTex))
			{
				return false;
			}

			texture++;
		}

		// マテリアル生成
		for (auto&& mat : document.materials.Elements())
		{
			auto material = std::make_shared<GlbMaterial>();
			if (!material->Initialize(pDev, document, mat, *glbResourceReader.get()))
			{
				return false;
			}
			materials_.push_back(material);
		}

		// サブメッシュ生成
		for (auto&& mesh : document.meshes.Elements())
		{
			for (auto&& prim : mesh.primitives)
			{
				auto submesh = std::make_shared<GlbSubmesh>();
				if (submesh->Initialize(pDev, document, prim, *glbResourceReader.get()))
				{
					submeshes_.push_back(submesh);
				}
			}
		}

		return true;
	}

	//-----------------------------------------------------------------------------
	// メッシュの破棄
	//-----------------------------------------------------------------------------
	void GlbMesh::Destroy()
	{
		submeshes_.clear();
		materials_.clear();
		textures_.clear();
	}

	//-----------------------------------------------------------------------------
	// アトラスUVを生成する
	//-----------------------------------------------------------------------------
	bool GlbMesh::GenerateAtlas(Device* pDev, size_t texWidth, size_t texHeight, bool use_uv_for_imt, int options)
	{
		const DWORD kOptions[] = {
			DirectX::UVATLAS_DEFAULT,
			DirectX::UVATLAS_GEODESIC_FAST,
			DirectX::UVATLAS_GEODESIC_QUALITY,
		};
		if (options < 0 || options >= ARRAYSIZE(kOptions))
		{
			return false;
		}

		const size_t maxCharts = 0;
		const float maxStretch = 0.1667f;
		const size_t width = 4096;
		const size_t height = 4096;
		const float gutter = 2.0f;

		auto submesh_count = GetSubmeshCount();

		// サブメッシュすべての頂点を一旦マージ
		std::unique_ptr<uint32_t[]> submesh_vcount(new uint32_t[submesh_count]);
		std::unique_ptr<uint32_t[]> submesh_icount(new uint32_t[submesh_count]);
		uint32_t vcount = 0, icount = 0;
		for (int i = 0; i < submesh_count; ++i)
		{
			auto&& submesh = GetSubmesh(i);
			vcount += submesh->GetVerticesCount();
			submesh_vcount[i] = submesh->GetVerticesCount();
			icount += submesh->GetIndicesCount();
			submesh_icount[i] = submesh->GetIndicesCount();
		}

		std::unique_ptr<DirectX::XMFLOAT3[]> mesh_pos_data(new DirectX::XMFLOAT3[vcount]);
		std::unique_ptr<DirectX::XMFLOAT3[]> mesh_nml_data(new DirectX::XMFLOAT3[vcount]);
		std::unique_ptr<DirectX::XMFLOAT2[]> mesh_uv_data(new DirectX::XMFLOAT2[vcount]);
		std::unique_ptr<sl12::u32[]> mesh_idx_data(new sl12::u32[icount]);
		vcount = 0;  icount = 0;
		for (int i = 0; i < submesh_count; i++)
		{
			auto&& submesh = GetSubmesh(i);

			memcpy(mesh_pos_data.get() + vcount, submesh->GetPositionSourceData(), sizeof(DirectX::XMFLOAT3) * submesh->GetVerticesCount());
			memcpy(mesh_nml_data.get() + vcount, submesh->GetNormalSourceData(), sizeof(DirectX::XMFLOAT3) * submesh->GetVerticesCount());
			memcpy(mesh_uv_data.get() + vcount, submesh->GetTexcoordSourceData(), sizeof(DirectX::XMFLOAT2) * submesh->GetVerticesCount());

			auto idx_source = submesh->GetIndexSourceData();
			for (int idx = 0; idx < submesh->GetIndicesCount(); idx++, idx_source++)
			{
				mesh_idx_data[icount + idx] = *idx_source + vcount;
			}

			vcount += submesh->GetVerticesCount();
			icount += submesh->GetIndicesCount();
		}

		// メッシュのエッジ情報などのバッファを生成する
		// NOTE: DirectXMeshの機能を利用する
		std::unique_ptr<uint32_t[]> adjacency(new uint32_t[icount]);
		HRESULT hr = DirectX::GenerateAdjacencyAndPointReps(
			mesh_idx_data.get(), icount / 3,
			mesh_pos_data.get(), vcount,
			1e-5f, nullptr, adjacency.get());
		if (FAILED(hr))
		{
			return false;
		}

		// IMTデータ生成
		// NOTE: IMTってなんぞや？
		//       Signalは今回は法線を利用するが、UVやカラーなんかも使えるらしい
		std::unique_ptr<float[]> IMTData(new float[icount]);
		if (!use_uv_for_imt)
		{
			hr = DirectX::UVAtlasComputeIMTFromPerVertexSignal(
				mesh_pos_data.get(), vcount,
				mesh_idx_data.get(), DXGI_FORMAT_R32_UINT, icount / 3,
				reinterpret_cast<const float*>(mesh_nml_data.get()), 3, sizeof(DirectX::XMFLOAT3),
				nullptr, IMTData.get());
		}
		else
		{
			hr = DirectX::UVAtlasComputeIMTFromPerVertexSignal(
				mesh_pos_data.get(), vcount,
				mesh_idx_data.get(), DXGI_FORMAT_R32_UINT, icount / 3,
				reinterpret_cast<const float*>(mesh_uv_data.get()), 2, sizeof(DirectX::XMFLOAT2),
				nullptr, IMTData.get());
		}
		if (FAILED(hr))
		{
			return false;
		}

		// UVアトラスの生成
		std::vector<DirectX::UVAtlasVertex> vb;
		std::vector<uint8_t> ib;
		float outStretch = 0.f;
		size_t outCharts = 0;
		std::vector<uint32_t> facePartitioning;
		std::vector<uint32_t> vertexRemapArray;
		hr = DirectX::UVAtlasCreate(
			mesh_pos_data.get(), vcount,
			mesh_idx_data.get(), DXGI_FORMAT_R32_UINT, icount / 3,
			maxCharts, maxStretch, width, height, gutter,
			adjacency.get(), nullptr,
			IMTData.get(),
			nullptr, DirectX::UVATLAS_DEFAULT_CALLBACK_FREQUENCY,
			kOptions[options], vb, ib,
			&facePartitioning,
			&vertexRemapArray,
			&outStretch, &outCharts);
		if (FAILED(hr))
		{
			return false;
		}

		// サブメッシュごとに新しい頂点とインデックスを割り当てる
		int start_index = 0;
		const uint32_t* new_idx = reinterpret_cast<const uint32_t*>(ib.data());
		for (int i = 0; i < submesh_count; i++)
		{
			auto&& submesh = GetSubmesh(i);
			auto idx_count = submesh->GetIndicesCount();
			int end_index = start_index + idx_count;

			// このサブメッシュで使用されるインデックスを列挙する
			std::set<uint32_t> used_indices;
			for (int s = start_index; s < end_index; s++)
			{
				used_indices.insert(new_idx[s]);
			}
			int new_vtx_count = (int)used_indices.size();

			// UVAtlasが生成したインデックス番号をサブメッシュ用インデックス番号に変換するリストを作成
			std::vector<uint32_t> submesh_index_list;
			submesh_index_list.resize(vb.size());
			icount = 0;
			for (auto aidx : used_indices)
			{
				submesh_index_list[aidx] = icount++;
			}

			// 新しいインデックス番号を設定する
			uint32_t* pidx = reinterpret_cast<uint32_t*>(submesh->indexBuffer_.source_data_.get());
			for (int s = start_index; s < end_index; s++, pidx++)
			{
				*pidx = submesh_index_list[new_idx[s]];
			}

			// 新しい頂点情報を生成する
			submesh->positionBuffer_.source_data_.reset(new u8[sizeof(DirectX::XMFLOAT3) * new_vtx_count]);
			submesh->normalBuffer_.source_data_.reset(new u8[sizeof(DirectX::XMFLOAT3) * new_vtx_count]);
			submesh->texcoordBuffer_.source_data_.reset(new u8[sizeof(DirectX::XMFLOAT2) * new_vtx_count]);
			submesh->uniqueUvBuffer_.source_data_.reset(new u8[sizeof(DirectX::XMFLOAT2) * new_vtx_count]);
			auto p_pos = reinterpret_cast<DirectX::XMFLOAT3*>(submesh->positionBuffer_.source_data_.get());
			auto p_nml = reinterpret_cast<DirectX::XMFLOAT3*>(submesh->normalBuffer_.source_data_.get());
			auto p_uv0 = reinterpret_cast<DirectX::XMFLOAT2*>(submesh->texcoordBuffer_.source_data_.get());
			auto p_uv1 = reinterpret_cast<DirectX::XMFLOAT2*>(submesh->uniqueUvBuffer_.source_data_.get());
			for (auto aidx : used_indices)
			{
				*p_pos = mesh_pos_data[vertexRemapArray[aidx]];
				*p_nml = mesh_nml_data[vertexRemapArray[aidx]];
				*p_uv0 = mesh_uv_data[vertexRemapArray[aidx]];
				*p_uv1 = vb[aidx].uv;

				p_pos++; p_nml++; p_uv0++; p_uv1++;
			}
			submesh->verticesCount_ = new_vtx_count;

			// バッファの再生性
			submesh->CreateBuffers(pDev);

			start_index = end_index;
		}

		return true;
	}

}


//	EOF
