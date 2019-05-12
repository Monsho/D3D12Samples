#include "sl12/glb_mesh.h"

#include <cstdio>
#include <memory>
#include <fstream>

#include "GLTFSDK/GLTF.h"
#include "GLTFSDK/GLBResourceReader.h"
#include "GLTFSDK/Deserialize.h"

#include "sl12/util.h"

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

		// インデックスバッファ作成
		{
			auto&& index_accessor = doc.accessors.Get(mesh.indicesAccessorId);
			auto index_data = resReader.ReadBinaryData<u32>(doc, index_accessor);
			auto index_count = index_data.size();

			if (!indexBuffer_.buffer_.Initialize(pDev, index_count * sizeof(u32), sizeof(u32), BufferUsage::IndexBuffer, true, false))
			{
				return false;
			}
			if (!indexBuffer_.buffer_view_.Initialize(pDev, &indexBuffer_.buffer_, 0, 0))
			{
				return false;
			}
			if (!indexBuffer_.view_.Initialize(pDev, &indexBuffer_.buffer_))
			{
				return false;
			}

			auto p = indexBuffer_.buffer_.Map(nullptr);
			memcpy(p, index_data.data(), index_count * sizeof(u32));
			indexBuffer_.buffer_.Unmap();

			indicesCount_ = index_count;
		}

		// 頂点バッファ作成
		auto CreateVB = [&](BufferBundle<VertexBufferView>& bb, const char* bufferName, int* verticesCount = nullptr)
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

				auto count = data.size();
				if (!bb.buffer_.Initialize(pDev, count * sizeof(float), sizeof(float) * elem_count, BufferUsage::VertexBuffer, true, false))
				{
					return false;
				}
				if (!bb.buffer_view_.Initialize(pDev, &bb.buffer_, 0, sizeof(float) * elem_count))
				{
					return false;
				}
				if (!bb.view_.Initialize(pDev, &bb.buffer_))
				{
					return false;
				}

				auto p = bb.buffer_.Map(nullptr);
				memcpy(p, data.data(), count * sizeof(float));
				bb.buffer_.Unmap();

				if (verticesCount)
				{
					*verticesCount = count / elem_count;
				}
			}

			return true;
		};
		{
			if (!CreateVB(positionBuffer_, "POSITION", &verticesCount_))
			{
				return false;
			}
			if (!CreateVB(normalBuffer_, "NORMAL"))
			{
				return false;
			}
			if (!CreateVB(texcoordBuffer_, "TEXCOORD_0"))
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

			if (!texture->pTex->InitializeFromPNG(pDev, pCmdList, data.data(), data.size(), false))
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

}


//	EOF
