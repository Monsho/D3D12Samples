#include <sl12/shader_manager.h>

#include <codecvt>
#include <locale>
#include <sstream>
#include <Shlwapi.h>
#include "dxcapi.h"
#include <sl12/string_util.h>

#include <windows.h>
#include <system_error>


namespace sl12
{
	namespace
	{
		std::wstring ToWString(const std::string& str)
		{
			return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(str);
		}

		std::wstring GetTargetProfile(ShaderType::Type type, int majorVersion, int minorVersion)
		{
			const wchar_t* kProfileType[] =
			{
				L"vs_",
				L"ps_",
				L"gs_",
				L"ds_",
				L"hs_",
				L"cs_",
				L"ms_",
				L"as_",
				L"lib_",
			};
			std::wostringstream stream;
			stream << kProfileType[type] << majorVersion << L"_" << minorVersion;
			return stream.str();
		}

		class DefaultIncludeHandler
			: public IDxcIncludeHandler
		{
		public:
			// DefaultIncludeHandler(IDxcLibrary* pLib, const std::vector<std::string>& dirs)
			// 	: dwRef_(1), pDxcLib_(pLib)
			DefaultIncludeHandler(IDxcUtils* pUtils, const std::vector<std::string>& dirs)
				: dwRef_(1), pDxcUtils_(pUtils)
			{
				for (auto&& s : dirs)
				{
					auto dir = ConvertYenToSlash(s);
					if (dir[dir.length() - 1] != '/')
					{
						dir += "/";
					}
					includeDirs_.push_back(ToWString(dir));
				}
			}
			virtual ~DefaultIncludeHandler()
			{
			}

			ULONG STDMETHODCALLTYPE AddRef()
			{
				return ++dwRef_;
			}

			ULONG STDMETHODCALLTYPE Release()
			{
				--dwRef_;
				if (!dwRef_)
				{
					delete this;
				}
				return dwRef_;
			}

			HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject)
			{
				*ppvObject = nullptr;
				if (riid == IID_IUnknown)
				{
					*ppvObject = static_cast<IDxcIncludeHandler*>(this);
					AddRef();
					return S_OK;
				}
				return E_NOINTERFACE;
			}

			virtual HRESULT STDMETHODCALLTYPE LoadSource(
				_In_ LPCWSTR pFilename,
				_COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource) override
			{
				std::wstring path;
				bool bFileExists = false;
				for (auto&& dir : includeDirs_)
				{
					path = dir + pFilename;
					if (PathFileExists(path.c_str()) && !PathIsDirectory(path.c_str()))
					{
						bFileExists = true;
						break;
					}
				}

				if (!bFileExists)
				{
					*ppIncludeSource = nullptr;
					return E_FAIL;
				}

				std::unique_lock<std::mutex> lock(mutex_);

				auto it = includeCodes_.find(path);
				if (it != includeCodes_.end())
				{
					*ppIncludeSource = it->second;
					it->second->AddRef();
					return S_OK;
				}

				UINT32 codePage = CP_UTF8;
				IDxcBlobEncoding* pBlob;
				HRESULT hr = pDxcUtils_->LoadFile(path.c_str(), &codePage, &pBlob);
				if (FAILED(hr))
				{
					*ppIncludeSource = nullptr;
					return E_FAIL;
				}

				includeCodes_[path] = pBlob;
				*ppIncludeSource = pBlob;
				pBlob->AddRef();
				return S_OK;
			}

		private:
			u32							dwRef_;
			IDxcUtils*					pDxcUtils_ = nullptr;
			std::vector<std::wstring>	includeDirs_;

		public:
			static void DestroyAllBlobs()
			{
				std::unique_lock<std::mutex> lock(mutex_);
				for (auto&& v : includeCodes_)
				{
					v.second->Release();
				}
				includeCodes_.clear();
			}

		private:
			static std::map<std::wstring, IDxcBlobEncoding*>	includeCodes_;
			static std::mutex									mutex_;
		};	// class DefaultIncludeHandler

		std::map<std::wstring, IDxcBlobEncoding*>	DefaultIncludeHandler::includeCodes_;
		std::mutex									DefaultIncludeHandler::mutex_;
	}

	// shader compiler
	class ShaderCompiler
	{
		friend class ShaderManager;

	public:
		ShaderCompiler()
		{}
		~ShaderCompiler()
		{
			Destroy();
		}

		bool Initialize()
		{
			HRESULT hr;
			hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pDxcUtils_));
			if (FAILED(hr))
			{
				return false;
			}

			hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pDxcCompiler_));
			if (FAILED(hr))
			{
				return false;
			}

			return true;
		}

		void Destroy()
		{
			SafeRelease(pDxcCompiler_);
			SafeRelease(pDxcUtils_);
		}

		bool Compile(
			IDxcBlobEncoding* pSrcBlob,
			const std::vector<std::wstring>& args,
			IDxcIncludeHandler* pInclude,
			IDxcBlob** ppResult,
			IDxcBlobEncoding** ppError,
			IDxcBlob** ppPDB, IDxcBlobUtf16** ppPDBName)
		{
			assert(pDxcUtils_ != nullptr);
			assert(pDxcCompiler_ != nullptr);

			HRESULT hr;

			std::vector<const wchar_t*> arg_array;
			arg_array.reserve(args.size());
			for (auto&& a : args)
			{
				arg_array.push_back(a.c_str());
			}
			
			DxcBuffer Source;
			Source.Ptr = pSrcBlob->GetBufferPointer();
			Source.Size = pSrcBlob->GetBufferSize();
			Source.Encoding = DXC_CP_ACP;

			IDxcResult* pCompileResult = nullptr;
			hr = pDxcCompiler_->Compile(&Source, arg_array.data(), (UINT32)arg_array.size(), pInclude, IID_PPV_ARGS(&pCompileResult));
			if (FAILED(hr))
			{
				return false;
			}

			pCompileResult->GetStatus(&hr);
			if (FAILED(hr))
			{
				pCompileResult->GetErrorBuffer(ppError);
				pCompileResult->Release();
				return false;
			}

			if (ppPDB && ppPDBName)
			{
				pCompileResult->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(ppPDB), ppPDBName);
			}
			
			pCompileResult->GetResult(ppResult);
			pCompileResult->Release();
			return true;
		}

	private:
		IDxcUtils* pDxcUtils_ = nullptr;
		IDxcCompiler3* pDxcCompiler_ = nullptr;
	};	// class ShaderCompiler


	bool ShaderHandle::IsValid() const
	{
		return GetShader() != nullptr;
	}

	const Shader* ShaderHandle::GetShader() const
	{
		if (pParent_ == nullptr)
			return nullptr;
		auto it = pParent_->shaderMap_.find(id_);
		return (it == pParent_->shaderMap_.end()) ? nullptr
			: &it->second->shader;
	}

	Shader* ShaderHandle::GetShader()
	{
		if (pParent_ == nullptr)
			return nullptr;
		auto it = pParent_->shaderMap_.find(id_);
		return (it == pParent_->shaderMap_.end()) ? nullptr
			: &it->second->shader;
	}


	bool ShaderManager::Initialize(
		Device* pDev,
		const std::vector<std::string>* includeDirs,
		ShaderPDB::Type pdbType,
		const std::string* pdbDir)
	{
		pDevice_ = pDev;

		pCompiler_ = new ShaderCompiler();
		if (!pCompiler_->Initialize())
		{
			return false;
		}

		if (includeDirs)
			includeDirs_ = *includeDirs;

		pdbExportType_ = ShaderPDB::None;
		if (pdbType != ShaderPDB::None && pdbDir != nullptr)
		{
			pdbExportType_ = pdbType;
			pdbDir_ = *pdbDir;
		}
		
		// create thread.
		std::thread th([&]
			{
				isAlive_ = true;
				while (isAlive_)
				{
					{
						std::unique_lock<std::mutex> lock(requestMutex_);
						requestCV_.wait(lock, [&] { return !requestList_.empty() || !isAlive_; });
					}

					if (!isAlive_)
					{
						break;
					}

					isCompiling_ = true;

					std::list<RequestItemPtr> items;
					{
						std::lock_guard<std::mutex> lock(listMutex_);
						items.swap(requestList_);
					}

					for (auto&& item : items)
					{
						ShaderItem* shader = Compile(item.get());
						if (shader)
						{
							shaderMap_[item->id].reset(shader);
						}

						if (!isAlive_)
						{
							break;
						}
					}

					isCompiling_ = false;
				}
			});
		loadingThread_ = std::move(th);

		return true;
	}

	void ShaderManager::Destroy()
	{
		isAlive_ = false;
		requestCV_.notify_one();

		if (loadingThread_.joinable())
			loadingThread_.join();

		requestList_.clear();
		shaderMap_.clear();
		DefaultIncludeHandler::DestroyAllBlobs();
		SafeDelete(pCompiler_);
	}

	ShaderHandle ShaderManager::LoadRequest(RequestItem* item)
	{
		if (item->entryPoint.empty())
		{
			item->entryPoint = L"main";
		}

		{
			std::lock_guard<std::mutex> lock(listMutex_);
			auto it = shaderMap_.begin();
			do
			{
				item->id = handleID_.fetch_add(1);
				it = shaderMap_.find(item->id);
			} while (it != shaderMap_.end());
			shaderMap_[item->id].reset();
			requestList_.push_back(RequestItemPtr(item));
		}

		std::lock_guard<std::mutex> lock(requestMutex_);
		requestCV_.notify_one();

		return ShaderHandle(this, item->id);
	}

	ShaderHandle ShaderManager::CompileFromMemory(
		const std::string& srcCode,
		const std::string& fileName,
		const std::string& entryPoint,
		ShaderType::Type type,
		int majorVersion, int minorVersion,
		const std::vector<std::string>* args,
		const std::vector<ShaderDefine>* defines)
	{
		RequestItem* item = new RequestItem();

		item->srcCode = ToWString(srcCode);
		item->fileName = ToWString(fileName);
		item->entryPoint = ToWString(entryPoint);
		item->targetProfile = GetTargetProfile(type, majorVersion, minorVersion);
		item->type = type;
		if (args)
		{
			item->args.reserve(args->size());
			for (auto&& a : *args)
				item->args.push_back(ToWString(a));
		}
		if (defines)
			item->defines = *defines;

		return LoadRequest(item);
	}

	ShaderHandle ShaderManager::CompileFromFile(
		const std::string& filePath,
		const std::string& entryPoint,
		ShaderType::Type type,
		int majorVersion, int minorVersion,
		const std::vector<std::string>* args,
		const std::vector<ShaderDefine>* defines)
	{
		RequestItem* item = new RequestItem();

		item->filePath = ToWString(filePath);
		item->fileDir = GetFilePath(filePath);
		item->fileName = ToWString(GetFileNameWithoutExtent(filePath));
		item->entryPoint = ToWString(entryPoint);
		item->targetProfile = GetTargetProfile(type, majorVersion, minorVersion);
		item->type = type;
		if (args)
		{
			item->args.reserve(args->size());
			for (auto&& a : *args)
				item->args.push_back(ToWString(a));
		}
		if (defines)
			item->defines = *defines;

		return LoadRequest(item);
	}

	ShaderManager::ShaderItem* ShaderManager::Compile(RequestItem* item)
	{
		auto includeDirs = includeDirs_;

		HRESULT hr;
		IDxcBlobEncoding* pSrcBlob = nullptr;
		if (!item->srcCode.empty())
		{
			hr = pCompiler_->pDxcUtils_->CreateBlobFromPinned(item->srcCode.c_str(), (UINT32)item->srcCode.size(), CP_UTF8, &pSrcBlob);
			if (FAILED(hr))
			{
				return nullptr;
			}
		}
		else
		{
			UINT32 codePage = CP_UTF8;
			hr = pCompiler_->pDxcUtils_->LoadFile(item->filePath.c_str(), &codePage, &pSrcBlob);
			if (FAILED(hr))
			{
				return nullptr;
			}
			includeDirs.push_back(item->fileDir);
		}

		IDxcIncludeHandler* pInclude = new DefaultIncludeHandler(pCompiler_->pDxcUtils_, includeDirs);

		bool bOutputPDB = false;
		std::wstring cso_name;
		std::vector<std::wstring> tmp_args;
		tmp_args.push_back(item->fileName);
		tmp_args.push_back(L"-E"); tmp_args.push_back(item->entryPoint);
		tmp_args.push_back(L"-T"); tmp_args.push_back(item->targetProfile);
		if (pdbExportType_ != ShaderPDB::None)
		{
			bOutputPDB = true;
			cso_name = item->fileName + L"_" + item->entryPoint + L".cso";
			tmp_args.push_back(L"-Fo"); tmp_args.push_back(cso_name);
			tmp_args.push_back(L"-Fd"); tmp_args.push_back(item->fileName + L"_" + item->entryPoint + L".pdb");
			if (pdbExportType_ == ShaderPDB::Full)
			{
				tmp_args.push_back(L"-Zi");
			}
			else
			{
				tmp_args.push_back(L"-Zs");
			}
		}
		tmp_args.insert(tmp_args.end(), item->args.begin(), item->args.end());
		for (auto&& s : item->defines)
		{
			tmp_args.push_back(L"-D");
			if (s.value.empty())
			{
				tmp_args.push_back(ToWString(s.name));
			}
			else
			{
				std::wstring str = ToWString(s.name);
				str += L"=";
				str += ToWString(s.value);
				tmp_args.push_back(str);
			}
		}

		IDxcBlob* pResult = nullptr;
		IDxcBlobEncoding* pError = nullptr;
		IDxcBlob* pPDB = nullptr;
		IDxcBlobUtf16* pPDBName = nullptr;
		bool bSuccess = pCompiler_->Compile(
			pSrcBlob,
			tmp_args,
			pInclude,
			&pResult,
			&pError,
			bOutputPDB ? &pPDB : nullptr,
			bOutputPDB ? &pPDBName : nullptr);
		pInclude->Release();
		if (!bSuccess)
		{
			std::wostringstream stream;
			stream << L"Shader Compilation failed!\n" << item->fileName << L"\n" << (const char*)pError->GetBufferPointer() << L"\n";
			OutputDebugStringW(stream.str().c_str());
			return nullptr;
		}

		if (bOutputPDB)
		{
			if (pPDB && pPDBName)
			{
				std::wstring pdbfile = ToWString(pdbDir_) + pPDBName->GetStringPointer();
				FILE* fp = nullptr;
				_wfopen_s(&fp, pdbfile.c_str(), L"wb");
				fwrite(pPDB->GetBufferPointer(), pPDB->GetBufferSize(), 1, fp);
				fclose(fp);
			}
			if (pResult)
			{
				std::wstring pdbfile = ToWString(pdbDir_) + cso_name;
				FILE* fp = nullptr;
				_wfopen_s(&fp, pdbfile.c_str(), L"wb");
				fwrite(pResult->GetBufferPointer(), pResult->GetBufferSize(), 1, fp);
				fclose(fp);
			}
		}

		ShaderItem* shader = new ShaderItem();
		shader->binary.resize(pResult->GetBufferSize());
		memcpy(shader->binary.data(), pResult->GetBufferPointer(), pResult->GetBufferSize());
		shader->shader.Initialize(pDevice_, item->type, shader->binary.data(), shader->binary.size());
		SafeRelease(pResult);
		SafeRelease(pError);
		return shader;
	}

}	// namespace sl12


//	EOF
