#include "sl12/resource_mesh.h"
#include "sl12/string_util.h"
#include "sl12/resource_texture.h"

#include <fstream>


namespace sl12
{

	//---------------
	ResourceItemMesh::~ResourceItemMesh()
	{
		positionVB_.Destroy();
		normalVB_.Destroy();
		tangentVB_.Destroy();
		texcoordVB_.Destroy();
		indexBuffer_.Destroy();
	}

	//---------------
	ResourceItemBase* ResourceItemMesh::LoadFunction(ResourceLoader* pLoader, const std::string& filepath)
	{
		ResourceMesh mesh_bin;
		std::fstream ifs(filepath, std::ios::in | std::ios::binary);
		cereal::BinaryInputArchive ar(ifs);
		ar(cereal::make_nvp("mesh", mesh_bin));

		if (mesh_bin.GetIndexBuffer().empty())
		{
			return nullptr;
		}

		std::unique_ptr<ResourceItemMesh> ret(new ResourceItemMesh());

		// set bounding sphere.
		ret->boundingInfo_.sphere.center.x = mesh_bin.GetBoundingSphere().centerX;
		ret->boundingInfo_.sphere.center.y = mesh_bin.GetBoundingSphere().centerY;
		ret->boundingInfo_.sphere.center.z = mesh_bin.GetBoundingSphere().centerZ;
		ret->boundingInfo_.sphere.radius = mesh_bin.GetBoundingSphere().radius;
		ret->boundingInfo_.box.aabbMin.x = mesh_bin.GetBoundingBox().minX;
		ret->boundingInfo_.box.aabbMin.y = mesh_bin.GetBoundingBox().minY;
		ret->boundingInfo_.box.aabbMin.z = mesh_bin.GetBoundingBox().minZ;
		ret->boundingInfo_.box.aabbMax.x = mesh_bin.GetBoundingBox().maxX;
		ret->boundingInfo_.box.aabbMax.y = mesh_bin.GetBoundingBox().maxY;
		ret->boundingInfo_.box.aabbMax.z = mesh_bin.GetBoundingBox().maxZ;

		// create buffers.
		auto pDev = pLoader->GetDevice();
		auto CreateBuffer = [&](Buffer& buff, const std::vector<u8>& data, size_t stride, BufferUsage::Type usage)
		{
			if (!buff.Initialize(pDev, data.size(), stride, usage, true, false))
			{
				return false;
			}
			auto p = buff.Map(nullptr);
			memcpy(p, data.data(), data.size());
			buff.Unmap();
			return true;
		};
		if (!CreateBuffer(ret->positionVB_, mesh_bin.GetVBPosition(), sizeof(DirectX::XMFLOAT3), BufferUsage::VertexBuffer))
		{
			return nullptr;
		}
		if (!CreateBuffer(ret->normalVB_, mesh_bin.GetVBNormal(), sizeof(DirectX::XMFLOAT3), BufferUsage::VertexBuffer))
		{
			return nullptr;
		}
		if (!CreateBuffer(ret->tangentVB_, mesh_bin.GetVBTangent(), sizeof(DirectX::XMFLOAT4), BufferUsage::VertexBuffer))
		{
			return nullptr;
		}
		if (!CreateBuffer(ret->texcoordVB_, mesh_bin.GetVBTexcoord(), sizeof(DirectX::XMFLOAT2), BufferUsage::VertexBuffer))
		{
			return nullptr;
		}
		if (!CreateBuffer(ret->indexBuffer_, mesh_bin.GetIndexBuffer(), sizeof(u32), BufferUsage::IndexBuffer))
		{
			return nullptr;
		}

		auto path = sl12::GetFilePath(filepath);

		// create materials.
		auto&& src_materials = mesh_bin.GetMaterials();
		auto mat_count = src_materials.size();
		ret->mateirals_.resize(mat_count);
		for (size_t i = 0; i < mat_count; i++)
		{
			ret->mateirals_[i].name = src_materials[i].GetName();
			if (!src_materials[i].GetTextureNames()[0].empty())
			{
				std::string f = path + src_materials[i].GetTextureNames()[0];
				ret->mateirals_[i].baseColorTex = pLoader->LoadRequest<ResourceItemTexture>(f);
			}
			if (!src_materials[i].GetTextureNames()[1].empty())
			{
				std::string f = path + src_materials[i].GetTextureNames()[1];
				ret->mateirals_[i].normalTex = pLoader->LoadRequest<ResourceItemTexture>(f);
			}
			if (!src_materials[i].GetTextureNames()[2].empty())
			{
				std::string f = path + src_materials[i].GetTextureNames()[2];
				ret->mateirals_[i].ormTex = pLoader->LoadRequest<ResourceItemTexture>(f);
			}
			ret->mateirals_[i].isOpaque = src_materials[i].IsOpaque();
		}

		// create submeshes.
		auto&& src_submeshes = mesh_bin.GetSubmeshes();
		auto sub_count = src_submeshes.size();
		ret->Submeshes_.resize(sub_count);
		for (size_t i = 0; i < sub_count; i++)
		{
			auto&& src = src_submeshes[i];
			auto&& dst = ret->Submeshes_[i];

			dst.materialIndex = src.GetMaterialIndex();
			dst.vertexCount = src.GetVertexCount();
			dst.indexCount = src.GetIndexCount();

			dst.positionVBV.Initialize(pDev, &ret->positionVB_, sizeof(DirectX::XMFLOAT3) * src.GetVertexOffset(), sizeof(DirectX::XMFLOAT3) * src.GetVertexCount());
			dst.normalVBV.Initialize(pDev, &ret->normalVB_, sizeof(DirectX::XMFLOAT3) * src.GetVertexOffset(), sizeof(DirectX::XMFLOAT3) * src.GetVertexCount());
			dst.tangentVBV.Initialize(pDev, &ret->tangentVB_, sizeof(DirectX::XMFLOAT4) * src.GetVertexOffset(), sizeof(DirectX::XMFLOAT4) * src.GetVertexCount());
			dst.texcoordVBV.Initialize(pDev, &ret->texcoordVB_, sizeof(DirectX::XMFLOAT2) * src.GetVertexOffset(), sizeof(DirectX::XMFLOAT2) * src.GetVertexCount());
			dst.indexBV.Initialize(pDev, &ret->indexBuffer_, sizeof(u32) * src.GetIndexOffset(), sizeof(u32) * src.GetIndexCount());

			dst.positionView.Initialize(pDev, &ret->positionVB_, src.GetVertexOffset(), src.GetVertexCount(), sizeof(DirectX::XMFLOAT3));
			dst.normalView.Initialize(pDev, &ret->normalVB_, src.GetVertexOffset(), src.GetVertexCount(), sizeof(DirectX::XMFLOAT3));
			dst.tangentView.Initialize(pDev, &ret->tangentVB_, src.GetVertexOffset(), src.GetVertexCount(), sizeof(DirectX::XMFLOAT4));
			dst.texcoordView.Initialize(pDev, &ret->texcoordVB_, src.GetVertexOffset(), src.GetVertexCount(), sizeof(DirectX::XMFLOAT2));
			dst.indexView.Initialize(pDev, &ret->indexBuffer_, src.GetIndexOffset(), src.GetIndexCount(), sizeof(u32));

			dst.boundingInfo.sphere.center.x = src.GetBoundingSphere().centerX;
			dst.boundingInfo.sphere.center.y = src.GetBoundingSphere().centerY;
			dst.boundingInfo.sphere.center.z = src.GetBoundingSphere().centerZ;
			dst.boundingInfo.sphere.radius = src.GetBoundingSphere().radius;
			dst.boundingInfo.box.aabbMin.x = src.GetBoundingBox().minX;
			dst.boundingInfo.box.aabbMin.y = src.GetBoundingBox().minY;
			dst.boundingInfo.box.aabbMin.z = src.GetBoundingBox().minZ;
			dst.boundingInfo.box.aabbMax.x = src.GetBoundingBox().maxX;
			dst.boundingInfo.box.aabbMax.y = src.GetBoundingBox().maxY;
			dst.boundingInfo.box.aabbMax.z = src.GetBoundingBox().maxZ;

			// create meshlets.
			auto&& src_meshlets = src.GetMeshlets();
			if (!src_meshlets.empty())
			{
				auto let_count = src_meshlets.size();
				dst.meshlets.resize(let_count);
				for (size_t j = 0; j < let_count; j++)
				{
					dst.meshlets[j].indexCount = src_meshlets[j].GetIndexCount();
					dst.meshlets[j].indexOffset = src_meshlets[j].GetIndexOffset();

					dst.meshlets[j].boundingInfo.sphere.center.x = src_meshlets[j].GetBoundingSphere().centerX;
					dst.meshlets[j].boundingInfo.sphere.center.y = src_meshlets[j].GetBoundingSphere().centerY;
					dst.meshlets[j].boundingInfo.sphere.center.z = src_meshlets[j].GetBoundingSphere().centerZ;
					dst.meshlets[j].boundingInfo.sphere.radius = src_meshlets[j].GetBoundingSphere().radius;
					dst.meshlets[j].boundingInfo.box.aabbMin.x = src_meshlets[j].GetBoundingBox().minX;
					dst.meshlets[j].boundingInfo.box.aabbMin.y = src_meshlets[j].GetBoundingBox().minY;
					dst.meshlets[j].boundingInfo.box.aabbMin.z = src_meshlets[j].GetBoundingBox().minZ;
					dst.meshlets[j].boundingInfo.box.aabbMax.x = src_meshlets[j].GetBoundingBox().maxX;
					dst.meshlets[j].boundingInfo.box.aabbMax.y = src_meshlets[j].GetBoundingBox().maxY;
					dst.meshlets[j].boundingInfo.box.aabbMax.z = src_meshlets[j].GetBoundingBox().maxZ;
				}
			}
		}

		return ret.release();
	}

}	// namespace sl12


//	EOF
