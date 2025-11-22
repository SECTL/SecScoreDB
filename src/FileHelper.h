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
		DataBaseFile(fs::path path);
		DataBaseFile(const DataBaseFile&) = delete;
		DataBaseFile& operator=(const DataBaseFile&) = delete;
		DataBaseFile(DataBaseFile&&) = default;
		DataBaseFile& operator=(DataBaseFile&&) = default;

		void writeBytes(const void* data, std::size_t len);
		void readBytes(const void* buf, std::size_t len);
	};
}
