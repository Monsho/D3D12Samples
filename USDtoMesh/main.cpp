#include "pxr/pxr.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usd/primRange.h"
#include "pxr/usd/usdGeom/mesh.h"
#include "pxr/usd/usdGeom/sphere.h"
#include "pxr/usd/usdGeom/xform.h"
#include "pxr/usd/usdGeom/xformCommonAPI.h"
#include "pxr/usd/usdGeom/scope.h"
#include "pxr/usd/usdShade/material.h"
#include "pxr/usd/usdShade/shader.h"
#include "pxr/usd/usdRi/risBxdf.h"
#include "pxr/usd/usdRi/materialAPI.h"
#include "pxr/usd/usdUtils/pipeline.h"

#include "../SampleLib12/include/sl12/mesh_format.h"

/**********************************************//**
 * @brief ヘルプを表示
**************************************************/
void DisplayHelp()
{
	fprintf(stdout, "USDtoMesh ver 0.1.0\n");
	fprintf(stdout, "	.usd形式のメッシュデータをサンプル用の.meshバイナリに変換します.\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "	使用例)\n");
	fprintf(stdout, "		USDtoMesh <input_file (.usd)> <output_file (.mesh)>\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "	オプション\n");
	fprintf(stdout, "		-h		: ヘルプを表示\n");
}

/**********************************************//**
 * @brief 浮動小数点ベクトル
**************************************************/
struct Vec2
{
	float	x, y;
};
struct Vec3
{
	float	x, y, z;
};

/**********************************************//**
 * @brief 頂点データ
**************************************************/
struct Vertex
{
	Vec3	position;
	Vec3	normal;
	Vec2	texcoord;

	bool operator==(const Vertex& v) const
	{
		return memcmp(this, &v, sizeof(*this)) == 0;
	}
	bool operator!=(const Vertex& v) const
	{
		return !operator==(v);
	}
	bool operator<(const Vertex& v) const
	{
		return memcmp(this, &v, sizeof(*this)) < 0;
	}
	bool operator>(const Vertex& v) const
	{
		return memcmp(this, &v, sizeof(*this)) > 0;
	}
};	// struct Vertex

/**********************************************//**
 * @brief バイナリデータ
**************************************************/
class BinData
{
public:
	BinData(size_t init_capa = 1024)
	{
		pData_ = (char*)malloc(init_capa);
		assert(pData_ != nullptr);
		size_ = 0;
		capacity_ = init_capa;
	}
	~BinData()
	{
		if (pData_)
		{
			free(pData_);
		}
	}

	const char* GetData() const
	{
		return pData_;
	}
	size_t GetSize() const
	{
		return size_;
	}

	size_t PushBack(void* p, size_t s)
	{
		if (size_ + s > capacity_)
		{
			// メモリの確保し直し
			capacity_ = std::max<size_t>(size_ + s, capacity_ * 2);
			char* pNew = (char*)malloc(capacity_);
			assert(pNew != nullptr);
			memcpy(pNew, pData_, size_);
			free(pData_);
			pData_ = pNew;
		}

		char* p_write = pData_ + size_;
		memcpy(p_write, p, s);
		size_t ret = size_;
		size_ += s;
		return ret;
	}

private:
	char*		pData_;
	size_t		size_;
	size_t		capacity_;
};	// class BinData

/**********************************************//**
 * @brief メッシュノード
**************************************************/
struct MeshNode
{
	pxr::UsdGeomMesh	src_mesh_;

	std::string			name_;

	std::vector<Vec3>	positions_;
	std::vector<Vec3>	normals_;
	std::vector<Vec2>	texcoords_;
	std::vector<int>	poly_vertex_counts_;
	std::vector<int>	poly_vertex_indices_;

	std::vector<Vertex>	vertices_;
	std::vector<int>	triangle_indices_;
	std::vector<int>	triangle_material_indices_;
	std::map<int, std::vector<sl12::u32>>	sub_mesh_indices_;
};	// struct MeshNode

/**********************************************//**
 * @brief マテリアルノード
**************************************************/
struct MaterialNode
{
	pxr::UsdShadeMaterial	src_material_;

	std::string				name_;
};	// struct MaterialNode

/**********************************************//**
 * @brief マテリアルインデックスを検索する
**************************************************/
int FindMaterialIndex(const std::vector<MaterialNode*>& materials, pxr::SdfPath path)
{
	int ret = 0;

	for (auto&& mat : materials)
	{
		if (mat->src_material_.GetPath() == path)
		{
			return ret;
		}
		++ret;
	}
	return -1;
}

/**********************************************//**
 * @brief メッシュノードをインポートする
**************************************************/
bool ImportMesh(MeshNode& out_mesh, pxr::UsdGeomMesh& in_mesh, const std::vector<MaterialNode*>& materials)
{
	out_mesh.src_mesh_ = in_mesh;

	// 名前の取得
	{
		std::string name = in_mesh.GetPath().GetString();
		auto pos = name.rfind('/');
		if (pos != std::string::npos)
		{
			name.erase(0, pos + 1);
		}
		out_mesh.name_ = name;
	}

	// 座標の取得
	{
		pxr::VtVec3fArray usd_points;
		in_mesh.GetPointsAttr().Get(&usd_points);

		auto size = usd_points.size();
		out_mesh.positions_.resize(size);
		for (size_t index = 0; index < size; ++index)
		{
			out_mesh.positions_[index].x = usd_points[index][0];
			out_mesh.positions_[index].y = usd_points[index][1];
			out_mesh.positions_[index].z = usd_points[index][2];
		}
	}

	// ポリゴンインデックスを取得
	{
		pxr::VtIntArray usd_vcounts;
		in_mesh.GetFaceVertexCountsAttr().Get(&usd_vcounts);
		auto size = usd_vcounts.size();
		out_mesh.poly_vertex_counts_.resize(size);
		for (size_t index = 0; index < size; ++index)
		{
			out_mesh.poly_vertex_counts_[index] = usd_vcounts[index];
		}

		pxr::VtIntArray usd_indices;
		in_mesh.GetFaceVertexIndicesAttr().Get(&usd_indices);
		size = usd_indices.size();
		out_mesh.poly_vertex_indices_.resize(size);
		for (size_t index = 0; index < size; ++index)
		{
			out_mesh.poly_vertex_indices_[index] = usd_indices[index];
		}
	}

	// 法線の取得
	{
		pxr::VtVec3fArray usd_normals;
		in_mesh.GetNormalsAttr().Get(&usd_normals);

		if (in_mesh.GetNormalsInterpolation() == pxr::UsdGeomTokens->faceVarying)
		{
			auto size = usd_normals.size();
			out_mesh.normals_.resize(size);
			for (size_t index = 0; index < size; ++index)
			{
				out_mesh.normals_[index].x = usd_normals[index][0];
				out_mesh.normals_[index].y = usd_normals[index][1];
				out_mesh.normals_[index].z = usd_normals[index][2];
			}
		}
		else
		{
			fprintf(stderr, "[ERROR] この法線の補間方法はサポートされていません. (%s)\n", in_mesh.GetNormalsInterpolation().GetString().c_str());
			return false;
		}
	}

	// UVの取得
	{
		auto st_token = pxr::UsdUtilsGetPrimaryUVSetName();
		auto st_prim_var = in_mesh.GetPrimvar(st_token);
		if (st_prim_var.IsDefined())
		{
			pxr::VtVec2fArray usd_sts;
			st_prim_var.Get(&usd_sts);

			if (st_prim_var.GetInterpolation() == pxr::UsdGeomTokens->faceVarying)
			{
				auto size = usd_sts.size();
				out_mesh.texcoords_.resize(size);
				for (size_t index = 0; index < size; ++index)
				{
					out_mesh.texcoords_[index].x = usd_sts[index][0];
					out_mesh.texcoords_[index].y = usd_sts[index][1];
				}
			}
			else if (st_prim_var.GetInterpolation() == pxr::UsdGeomTokens->constant)
			{
				auto size = out_mesh.poly_vertex_indices_.size();
				out_mesh.texcoords_.resize(size);
				Vec2 st = { usd_sts[0][0], usd_sts[0][1] };
				for (size_t index = 0; index < size; ++index)
				{
					out_mesh.texcoords_[index] = st;
				}
			}
			else
			{
				fprintf(stderr, "[ERROR] このUV座標の補間方法はサポートされていません. (%s)\n", st_prim_var.GetInterpolation().GetString().c_str());
				return false;
			}
		}
	}

	// FaceSetから適用されるマテリアルインデックスを取得する
	auto face_set_api = pxr::UsdShadeMaterial::GetMaterialFaceSet(in_mesh.GetPrim());
	pxr::SdfPathVector targets;
	face_set_api.GetBindingTargets(&targets);
	pxr::VtIntArray counts, indices;
	face_set_api.GetFaceCounts(&counts);
	face_set_api.GetFaceIndices(&indices);
	if (targets.size() != counts.size())
	{
		fprintf(stderr, "[ERROR] アサインされているマテリアル数と対応するポリカウント数が一致しません. (Material : %lld, Poly : %lld)\n", targets.size(), counts.size());
		return false;
	}
	if (indices.size() != out_mesh.poly_vertex_counts_.size())
	{
		fprintf(stderr, "[ERROR] マテリアルがアサインされていないポリゴンが存在します.\n");
		return false;
	}

	// 各ポリゴンにアサインされているマテリアルを登録する
	std::vector<int> mat_assign_index(indices.size());
	for (size_t mat_no = 0, findex = 0; mat_no < targets.size(); ++mat_no)
	{
		auto&& mat_path = targets[mat_no];
		auto&& face_count = counts[mat_no];

		int mat_index = FindMaterialIndex(materials, mat_path);
		if (mat_index < 0)
		{
			fprintf(stderr, "[ERROR] 存在しないマテリアルがアサインされています. (%s)\n", mat_path.GetString().c_str());
			return false;
		}

		for (int face_no = 0; face_no < face_count; ++face_no, ++findex)
		{
			mat_assign_index[indices[findex]] = mat_index;
		}
	}

	// 頂点データをまとめる
	std::vector<int> new_indices;
	std::map<Vertex, int> vertex_dic;
	int count = 0;
	for (auto&& index : out_mesh.poly_vertex_indices_)
	{
		Vertex v;
		v.position = out_mesh.positions_[index];
		v.normal = out_mesh.normals_[count];
		v.texcoord = out_mesh.texcoords_[count];
		++count;

		auto&& it = vertex_dic.find(v);
		if (it == vertex_dic.end())
		{
			int new_index = (int)out_mesh.vertices_.size();
			out_mesh.vertices_.push_back(v);
			new_indices.push_back(new_index);
			vertex_dic[v] = new_index;
		}
		else
		{
			new_indices.push_back(it->second);
		}
	}
	out_mesh.poly_vertex_indices_ = new_indices;

	// ポリゴンをトライアングル化して展開する
	for (size_t findex = 0, vindex = 0; findex < out_mesh.poly_vertex_counts_.size(); ++findex)
	{
		int vertex_count = out_mesh.poly_vertex_counts_[findex];
		int mat_index = mat_assign_index[findex];
		auto p_index = out_mesh.poly_vertex_indices_.data();

		int s = 0;
		int e = vertex_count - 1;
		for (int i = 0; i < (vertex_count - 2); ++i)
		{
			if (i & 0x01)
			{
				// odd
				out_mesh.triangle_indices_.push_back(p_index[vindex + s + 1]);
				out_mesh.triangle_indices_.push_back(p_index[vindex + e - 1]);
				out_mesh.triangle_indices_.push_back(p_index[vindex + e]);
				s++;
				e--;
			}
			else
			{
				// even
				out_mesh.triangle_indices_.push_back(p_index[vindex + s]);
				out_mesh.triangle_indices_.push_back(p_index[vindex + s + 1]);
				out_mesh.triangle_indices_.push_back(p_index[vindex + e]);
			}
			out_mesh.triangle_material_indices_.push_back(mat_index);
		}

		vindex += vertex_count;
	}

	// アサインされてるマテリアルごとにグループ化し、サブメッシュとして登録する
	int triangle_index = 0;
	for (auto&& mat_index : out_mesh.triangle_material_indices_)
	{
		auto&& it = out_mesh.sub_mesh_indices_.find(mat_index);
		if (it == out_mesh.sub_mesh_indices_.end())
		{
			std::vector<sl12::u32> indices;
			indices.push_back(out_mesh.triangle_indices_[triangle_index * 3 + 0]);
			indices.push_back(out_mesh.triangle_indices_[triangle_index * 3 + 1]);
			indices.push_back(out_mesh.triangle_indices_[triangle_index * 3 + 2]);
			out_mesh.sub_mesh_indices_[mat_index] = indices;
		}
		else
		{
			out_mesh.sub_mesh_indices_[mat_index].push_back(out_mesh.triangle_indices_[triangle_index * 3 + 0]);
			out_mesh.sub_mesh_indices_[mat_index].push_back(out_mesh.triangle_indices_[triangle_index * 3 + 1]);
			out_mesh.sub_mesh_indices_[mat_index].push_back(out_mesh.triangle_indices_[triangle_index * 3 + 2]);
		}

		++triangle_index;
	}

	return true;
}

/**********************************************//**
 * @brief マテリアルノードをインポートする
**************************************************/
bool ImportMaterial(MaterialNode& out_mat, pxr::UsdShadeMaterial& in_mat)
{
	out_mat.src_material_ = in_mat;

	// 名前
	{
		std::string name = in_mat.GetPath().GetString();
		auto pos = name.rfind('/');
		if (pos != std::string::npos)
		{
			name.erase(0, pos + 1);
		}
		out_mat.name_ = name;
	}

	return true;
}

/**********************************************//**
 * @brief .meshバイナリをエクスポートする
**************************************************/
bool ExportMeshBinary(const std::vector<MeshNode*>& meshes, const std::vector<MaterialNode*>& materials, const std::string& out_name)
{
	// ヘッダ
	sl12::MeshHead mesh_head;
	mesh_head.fourCC[0] = 'M';
	mesh_head.fourCC[1] = 'E';
	mesh_head.fourCC[2] = 'S';
	mesh_head.fourCC[3] = 'H';
	mesh_head.numShapes = (sl12::s32)meshes.size();
	mesh_head.numMaterials = (sl12::s32)materials.size();
	mesh_head.numSubmeshes = 0;

	// シェイプ
	std::vector<sl12::MeshShape> mesh_shapes;
	mesh_shapes.resize(meshes.size());
	BinData vertexBuffer(4 * 1024 * 1024);
	for (size_t i = 0; i < meshes.size(); ++i)
	{
		auto&& in_mesh = meshes[i];
		auto&& out_mesh = mesh_shapes[i];

		strcpy_s(out_mesh.name, in_mesh->name_.c_str());
		out_mesh.numVertices = (sl12::u32)in_mesh->vertices_.size();
		out_mesh.numIndices = (sl12::u32)in_mesh->triangle_indices_.size();

		out_mesh.positionOffset = vertexBuffer.GetSize();
		for (auto&& v : in_mesh->vertices_)
		{
			vertexBuffer.PushBack(&v.position, sizeof(v.position));
		}

		out_mesh.normalOffset = vertexBuffer.GetSize();
		for (auto&& v : in_mesh->vertices_)
		{
			vertexBuffer.PushBack(&v.normal, sizeof(v.normal));
		}

		out_mesh.texcoordOffset = vertexBuffer.GetSize();
		for (auto&& v : in_mesh->vertices_)
		{
			vertexBuffer.PushBack(&v.texcoord, sizeof(v.texcoord));
		}
	}

	// マテリアル
	std::vector<sl12::MeshMaterial> mesh_materials;
	mesh_materials.resize(materials.size());
	for (size_t i = 0; i < materials.size(); ++i)
	{
		auto&& in_mat = materials[i];
		auto&& out_mat = mesh_materials[i];

		strcpy_s(out_mat.name, in_mat->name_.c_str());
	}

	// サブメッシュ
	std::vector<sl12::MeshSubmesh> mesh_submeshes;
	BinData indexBuffer(1024 * 1024);
	for (size_t i = 0; i < meshes.size(); ++i)
	{
		auto&& mesh = meshes[i];

		sl12::MeshSubmesh submesh;
		submesh.shapeIndex = (sl12::s32)i;

		for (auto&& sm : mesh->sub_mesh_indices_)
		{
			submesh.materialIndex = sm.first;
			submesh.numSubmeshIndices = (sl12::u32)sm.second.size();
			submesh.indexBufferOffset = vertexBuffer.GetSize() + indexBuffer.PushBack(sm.second.data(), sm.second.size() * sizeof(sl12::u32));
			mesh_submeshes.push_back(submesh);

			++mesh_head.numSubmeshes;
		}
	}

	// バイナリに保存する
	FILE* fp = nullptr;
	if (fopen_s(&fp, out_name.c_str(), "wb") != 0)
	{
		return false;
	}
	fwrite(&mesh_head, sizeof(mesh_head), 1, fp);
	fwrite(mesh_shapes.data(), sizeof(sl12::MeshShape), mesh_shapes.size(), fp);
	fwrite(mesh_materials.data(), sizeof(sl12::MeshMaterial), mesh_materials.size(), fp);
	fwrite(mesh_submeshes.data(), sizeof(sl12::MeshSubmesh), mesh_submeshes.size(), fp);
	fwrite(vertexBuffer.GetData(), vertexBuffer.GetSize(), 1, fp);
	fwrite(indexBuffer.GetData(), indexBuffer.GetSize(), 1, fp);
	fclose(fp);

	return true;
}

int main(int argc, char* argv[])
{
	if (argc <= 2)
	{
		DisplayHelp();
		return 0;
	}

	std::string input_filepath, output_filepath;
	for (int i = 1; i < argc; ++i)
	{
		std::string arg(argv[i]);
		if (arg[0] == '-')
		{
			// オプションチェック
			if (arg == "-h")
			{
				DisplayHelp();
				return 0;
			}
			else
			{
				fprintf(stderr, "[ERROR] 無効なオプションです. (%s)\n", arg.c_str());
				return -1;
			}
		}
		else if (input_filepath.empty())
		{
			input_filepath = arg;
		}
		else if (output_filepath.empty())
		{
			output_filepath = arg;
		}
		else
		{
			fprintf(stderr, "[ERROR] 無効な引数です. (%s)\n", arg.c_str());
			return -1;
		}
	}

	if (input_filepath.empty() || output_filepath.empty())
	{
		fprintf(stderr, "[ERROR] 入力ファイルと出力ファイルを指定してください.\n");
		return -1;
	}

	auto stage = pxr::UsdStage::Open(input_filepath);
	if (stage == nullptr)
	{
		fprintf(stderr, "[ERROR] 無効なUSDファイルです. (%s)\n", input_filepath.c_str());
		return -1;
	}

	auto root = stage->GetRootLayer();
	auto root_prim = stage->GetDefaultPrim();
	if (!root_prim)
	{
		root_prim = stage->GetPseudoRoot();
	}
	pxr::UsdPrimRange range(root_prim);

	// マテリアルをインポート
	std::vector<MaterialNode*> materials;
	for (auto&& prim : range)
	{
		if (prim.GetTypeName() == "Material")
		{
			pxr::UsdShadeMaterial mat(prim);
			MaterialNode* mat_node = new MaterialNode;
			if (ImportMaterial(*mat_node, mat))
			{
				materials.push_back(mat_node);
			}
		}
	}

	// メッシュをインポート
	std::vector<MeshNode*> meshes;
	for (auto&& prim : range)
	{
		if (prim.GetTypeName() == "Mesh")
		{
			pxr::UsdGeomMesh mesh(prim);
			MeshNode* mesh_node = new MeshNode;
			if (ImportMesh(*mesh_node, mesh, materials))
			{
				meshes.push_back(mesh_node);
			}
		}
	}

	// バイナリを生成して保存する
	if (!ExportMeshBinary(meshes, materials, output_filepath))
	{
		return false;
	}

	// 終了処理
	for (auto&& v : materials) delete v;
	materials.clear();
	for (auto&& v : meshes) delete v;
	meshes.clear();
	stage->Save();
	return 0;
}




// EOF
