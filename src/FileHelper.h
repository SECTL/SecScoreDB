#pragma once
#include<fstream>
#include<filesystem>
//#include<array>
#include"SSDBType.h"

// 负责跟操作系统交互：提供文件读写服务

namespace SSDB
{
	class DataBaseFile
	{
	private:
		fs::path filePath;
		std::fstream File;
	public:
		explicit DataBaseFile(fs::path path);
		DataBaseFile(const DataBaseFile&) = delete;
		DataBaseFile& operator=(const DataBaseFile&) = delete;
		DataBaseFile(DataBaseFile&&)  noexcept = default;
		DataBaseFile& operator=(DataBaseFile&&)  noexcept = default;

		template<typename T>
		void writeObj(const T& obj);
		template<typename T>
		bool readNextObj(const T& obj);
	};
}
