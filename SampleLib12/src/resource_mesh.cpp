#include "sl12/resource_mesh.h"

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

		return ret.release();
	}

}	// namespace sl12


//	EOF
