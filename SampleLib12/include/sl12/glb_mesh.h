#pragma once

#include <vector>
#include <memory>

#include "sl12/buffer.h"
#include "sl12/buffer_view.h"
#include "sl12/texture.h"
#include "sl12/texture_view.h"


namespace Microsoft
{
	namespace glTF
	{
		class Document;
		struct MeshPrimitive;
		struct Material;
		class GLTFResourceReader;
	}
}

namespace sl12
{
	class GlbSubmesh
	{
		friend class GlbMesh;

		template <typename View>
		struct BufferBundle
		{
			Buffer			buffer_;
			BufferView		buffer_view_;
			View			view_;

			~BufferBundle()
			{
				view_.Destroy();
				buffer_view_.Destroy();
				buffer_.Destroy();
			}
		};	// struct BufferBundle

	public:
		GlbSubmesh()
		{}
		~GlbSubmesh()
		{
			Destroy();
		}

		// getter
		Buffer& GetPositionB()
		{
			return positionBuffer_.buffer_;
		}
		BufferView& GetPositionBV()
		{
			return positionBuffer_.buffer_view_;
		}
		VertexBufferView& GetPositionVBV()
		{
			return positionBuffer_.view_;
		}

		Buffer& GetNormalB()
		{
			return normalBuffer_.buffer_;
		}
		BufferView& GetNormalBV()
		{
			return normalBuffer_.buffer_view_;
		}
		VertexBufferView& GetNormalVBV()
		{
			return normalBuffer_.view_;
		}

		Buffer& GetTexcoordB()
		{
			return texcoordBuffer_.buffer_;
		}
		BufferView& GetTexcoordBV()
		{
			return texcoordBuffer_.buffer_view_;
		}
		VertexBufferView& GetTexcoordVBV()
		{
			return texcoordBuffer_.view_;
		}

		Buffer& GetIndexB()
		{
			return indexBuffer_.buffer_;
		}
		BufferView& GetIndexBV()
		{
			return indexBuffer_.buffer_view_;
		}
		IndexBufferView& GetIndexIBV()
		{
			return indexBuffer_.view_;
		}

		int GetMaterialIndex() const
		{
			return materialIndex_;
		}

		int GetIndicesCount() const
		{
			return indicesCount_;
		}

	private:
		bool Initialize(Device* pDev, const Microsoft::glTF::Document& doc, const Microsoft::glTF::MeshPrimitive& mesh, Microsoft::glTF::GLTFResourceReader& resReader);
		void Destroy();

	private:
		BufferBundle<VertexBufferView>	positionBuffer_;
		BufferBundle<VertexBufferView>	normalBuffer_;
		BufferBundle<VertexBufferView>	texcoordBuffer_;
		BufferBundle<IndexBufferView>	indexBuffer_;

		int								materialIndex_ = -1;
		int								indicesCount_ = 0;
	};	// class GlbSubmesh

	class GlbMaterial
	{
		friend class GlbMesh;

	public:
		GlbMaterial()
		{}
		~GlbMaterial()
		{
			Destroy();
		}

		// getter
		const std::string& GetName() const
		{
			return name_;
		}
		int GetTexBaseColorIndex() const
		{
			return texBaseColorIndex_;
		}
		int GetTexMetalRoughIndex() const
		{
			return texMetalRoughIndex_;
		}
		int GetTexNormalIndex() const
		{
			return texNormalIndex_;
		}

	private:
		bool Initialize(Device* pDev, const Microsoft::glTF::Document& doc, const Microsoft::glTF::Material& mat, Microsoft::glTF::GLTFResourceReader& resReader);
		void Destroy();

	private:
		std::string			name_;
		int					texBaseColorIndex_ = -1;
		int					texMetalRoughIndex_ = -1;
		int					texNormalIndex_ = -1;
	};	// class GlbMaterial

	class GlbMesh
	{
		struct TextureSet
		{
			Texture*			pTex = nullptr;
			TextureView*		pView = nullptr;

			~TextureSet()
			{
				delete pView;
				delete pTex;
			}
		};	// struct TextureSet

	public:
		GlbMesh()
		{}
		~GlbMesh()
		{
			Destroy();
		}

		bool Initialize(Device* pDev, CommandList* pCmdList, const char* pathname, const char* filename);
		void Destroy();

		// getter
		int GetSubmeshCount() const
		{
			return (int)submeshes_.size();
		}
		std::shared_ptr<GlbSubmesh> GetSubmesh(int index)
		{
			return submeshes_[index];
		}
		std::shared_ptr<GlbMaterial> GetMaterial(int index)
		{
			return materials_[index];
		}
		TextureView* GetTextureView(int index)
		{
			return textures_[index].pView;
		}

	private:
		std::vector<std::shared_ptr<GlbSubmesh>>	submeshes_;
		std::vector<std::shared_ptr<GlbMaterial>>	materials_;
		std::vector<TextureSet>						textures_;
	};	// class GlbMesh

}	// namespace sl12


//	EOF
