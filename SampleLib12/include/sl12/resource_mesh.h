#pragma once

#include "sl12/types.h"
#include "sl12/resource_loader.h"
#include "sl12/buffer.h"
#include "sl12/buffer_view.h"

#include <DirectXMath.h>
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>


namespace sl12
{

	struct ResourceMeshBoundingSphere
	{
		float	centerX, centerY, centerZ;
		float	radius;

		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(CEREAL_NVP(centerX), CEREAL_NVP(centerY), CEREAL_NVP(centerZ), CEREAL_NVP(radius));
		}
	};	// class ResourceMeshBoundingSphere

	struct ResourceMeshBoundingBox
	{
		float	minX, minY, minZ;
		float	maxX, maxY, maxZ;

		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(CEREAL_NVP(minX), CEREAL_NVP(minY), CEREAL_NVP(minZ), CEREAL_NVP(maxX), CEREAL_NVP(maxY), CEREAL_NVP(maxZ));
		}
	};	// class ResourceMeshBoundingBox

	class ResourceMeshMaterial
	{
		friend class cereal::access;

	public:
		ResourceMeshMaterial()
		{}
		~ResourceMeshMaterial()
		{}

		const std::string& GetName() const
		{
			return name_;
		}
		const std::vector<std::string>& GetTextureNames() const
		{
			return textureNames_;
		}

	private:
		std::string					name_;
		std::vector<std::string>	textureNames_;


		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(CEREAL_NVP(name_), CEREAL_NVP(textureNames_));
		}
	};	// class ResourceMeshMaterial

	class ResourceMeshMeshlet
	{
		friend class cereal::access;

	public:
		ResourceMeshMeshlet()
		{}
		~ResourceMeshMeshlet()
		{}

		u32 GetIndexOffset() const
		{
			return indexOffset_;
		}
		u32 GetIndexCount() const
		{
			return indexCount_;
		}
		const ResourceMeshBoundingSphere& GetBoundingSphere() const
		{
			return boundingSphere_;
		}
		const ResourceMeshBoundingBox& GetBoundingBox() const
		{
			return boundingBox_;
		}

	private:
		u32							indexOffset_;
		u32							indexCount_;
		ResourceMeshBoundingSphere	boundingSphere_;
		ResourceMeshBoundingBox		boundingBox_;


		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(CEREAL_NVP(indexOffset_),
				CEREAL_NVP(indexCount_),
				CEREAL_NVP(boundingSphere_),
				CEREAL_NVP(boundingBox_));
		}
	};	// class ResourceMeshMeshlet

	class ResourceMeshSubmesh
	{
		friend class cereal::access;

	public:
		ResourceMeshSubmesh()
		{}
		~ResourceMeshSubmesh()
		{}

		int GetMaterialIndex() const
		{
			return materialIndex_;
		}
		u32 GetVertexOffset() const
		{
			return vertexOffset_;
		}
		u32 GetIndexOffset() const
		{
			return indexOffset_;
		}
		u32 GetVertexCount() const
		{
			return vertexCount_;
		}
		u32 GetIndexCount() const
		{
			return indexCount_;
		}
		const std::vector<ResourceMeshMeshlet>& GetMeshlets() const
		{
			return meshlets_;
		}
		const ResourceMeshBoundingSphere& GetBoundingSphere() const
		{
			return boundingSphere_;
		}
		const ResourceMeshBoundingBox& GetBoundingBox() const
		{
			return boundingBox_;
		}

	private:
		int									materialIndex_;
		u32									vertexOffset_;
		u32									indexOffset_;
		u32									vertexCount_;
		u32									indexCount_;
		std::vector<ResourceMeshMeshlet>	meshlets_;
		ResourceMeshBoundingSphere			boundingSphere_;
		ResourceMeshBoundingBox				boundingBox_;


		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(CEREAL_NVP(materialIndex_),
				CEREAL_NVP(vertexOffset_),
				CEREAL_NVP(indexOffset_),
				CEREAL_NVP(vertexCount_),
				CEREAL_NVP(indexCount_),
				CEREAL_NVP(meshlets_),
				CEREAL_NVP(boundingSphere_),
				CEREAL_NVP(boundingBox_));
		}
	};	// class ResourceMeshSubmesh

	class ResourceMesh
	{
		friend class cereal::access;

	public:
		ResourceMesh()
		{}
		~ResourceMesh()
		{}

		const std::vector<ResourceMeshMaterial>& GetMaterials() const
		{
			return materials_;
		}
		const std::vector<ResourceMeshSubmesh>& GetSubmeshes() const
		{
			return submeshes_;
		}
		const ResourceMeshBoundingSphere& GetBoundingSphere() const
		{
			return boundingSphere_;
		}
		const ResourceMeshBoundingBox& GetBoundingBox() const
		{
			return boundingBox_;
		}

		const std::vector<u8>& GetVBPosition() const
		{
			return vbPosition_;
		}
		const std::vector<u8>& GetVBNormal() const
		{
			return vbNormal_;
		}
		const std::vector<u8>& GetVBTangent() const
		{
			return vbTangent_;
		}
		const std::vector<u8>& GetVBTexcoord() const
		{
			return vbTexcoord_;
		}
		const std::vector<u8>& GetIndexBuffer() const
		{
			return indexBuffer_;
		}

	private:
		std::vector<ResourceMeshMaterial>	materials_;
		std::vector<ResourceMeshSubmesh>	submeshes_;
		ResourceMeshBoundingSphere			boundingSphere_;
		ResourceMeshBoundingBox				boundingBox_;

		std::vector<u8>						vbPosition_;
		std::vector<u8>						vbNormal_;
		std::vector<u8>						vbTangent_;
		std::vector<u8>						vbTexcoord_;
		std::vector<u8>						indexBuffer_;


		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(CEREAL_NVP(materials_),
				CEREAL_NVP(submeshes_),
				CEREAL_NVP(boundingSphere_),
				CEREAL_NVP(boundingBox_),
				CEREAL_NVP(vbPosition_),
				CEREAL_NVP(vbNormal_),
				CEREAL_NVP(vbTangent_),
				CEREAL_NVP(vbTexcoord_),
				CEREAL_NVP(indexBuffer_));
		}
	};	// class ResourceMesh

	class ResourceItemMesh
		: public ResourceItemBase
	{
	public:
		struct Bounding
		{
			struct
			{
				DirectX::XMFLOAT3	center;
				float				radius;
			} sphere;
			struct
			{
				DirectX::XMFLOAT3	aabbMin, aabbMax;
			} box;
		};	// struct Bounding

		struct Material
		{
			std::string		name;
			ResourceHandle	baseColorTex;
			ResourceHandle	normalTex;
			ResourceHandle	ormTex;
		};	// struct Material

		struct Meshlet
		{
			u32			indexOffset;
			u32			indexCount;
			Bounding	boundingInfo;
		};	// struct Meshlet

		struct Submesh
		{
			int					materialIndex;
			u32					indexCount;
			VertexBufferView	positionVBV;
			VertexBufferView	normalVBV;
			VertexBufferView	tangentVBV;
			VertexBufferView	texcoordVBV;
			IndexBufferView		indexBV;
			BufferView			positionView;
			BufferView			normalView;
			BufferView			tangentView;
			BufferView			texcoordView;
			BufferView			indexView;
			Bounding			boundingInfo;

			std::vector<Meshlet>	meshlets;
		};	// struct Submesh

	public:
		static const u32 kType = TYPE_FOURCC("MESH");

		~ResourceItemMesh();

		const std::vector<Material>& GetMaterials() const
		{
			return mateirals_;
		}
		const std::vector<Submesh>& GetSubmeshes() const
		{
			return Submeshes_;
		}
		const Bounding& GetBoundingInfo() const
		{
			return boundingInfo_;
		}
		Buffer& GetPositionVB()
		{
			return positionVB_;
		}
		Buffer& GetNormalVB()
		{
			return normalVB_;
		}
		Buffer& GetTangentVB()
		{
			return tangentVB_;
		}
		Buffer& GetTexcoordVB()
		{
			return texcoordVB_;
		}
		Buffer& GetIndexBuffer()
		{
			return indexBuffer_;
		}

		static ResourceItemBase* LoadFunction(ResourceLoader* pLoader, const std::string& filepath);

	private:
		ResourceItemMesh()
			: ResourceItemBase(ResourceItemMesh::kType)
		{}

	private:
		std::vector<Material>	mateirals_;
		std::vector<Submesh>	Submeshes_;
		Bounding				boundingInfo_;

		Buffer		positionVB_;
		Buffer		normalVB_;
		Buffer		tangentVB_;
		Buffer		texcoordVB_;
		Buffer		indexBuffer_;
	};	// class ResourceItemMesh

}	// namespace sl12


//	EOF
