#pragma once

#include <iostream>
#include <fstream>
#include <memory>


namespace sl12
{
	class File
	{
	public:
		File()
		{}
		File(const char* filename)
		{
			ReadFile(filename);
		}
		~File()
		{
			Destroy();
		}

		bool ReadFile(const char* filename)
		{
			std::ifstream fin;

			fin.open(filename, std::ios::in | std::ios::binary | std::ios::ate);
			if (!fin.is_open())
			{
				return false;
			}

			size_ = static_cast<uint64_t>(fin.seekg(0, std::ios::end).tellg());
			fin.seekg(0, std::ios::beg);

			data_.reset(new uint8_t[size_]);
			fin.read(reinterpret_cast<char*>(data_.get()), size_);

			fin.close();

			return true;
		}

		void Destroy()
		{
			data_.reset(nullptr);
			size_ = 0;
		}

		// getter
		void* GetData() { return data_.get(); }
		uint64_t GetSize() { return size_; }

	private:
		std::unique_ptr<uint8_t[]>	data_{};
		uint64_t					size_{ 0 };
	};	// File

}	// namespace sl12


//	EOF
