#pragma once
#include<fstream>
#include<filesystem>
#include<unordered_map>
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
		std::unordered_map<int, std::streampos> offsetTable;  // 索引表，记录每个对象在文件中的偏移量（key-value形式）
		
	public:
		explicit DataBaseFile(fs::path path);
		~DataBaseFile();  // 析构函数，确保文件正确关闭
		DataBaseFile(const DataBaseFile&) = delete;
		DataBaseFile& operator=(const DataBaseFile&) = delete;
		DataBaseFile(DataBaseFile&&)  noexcept = default;
		DataBaseFile& operator=(DataBaseFile&&)  noexcept = default;

		template<typename T>
		void writeObj(const T& obj);
		template<typename T>
		bool readNextObj(const T& obj);
		
		// 索引表相关方法
		void recordOffset(int key);                  // 记录指定key的当前位置到索引表
		std::streampos getOffset(int key) const;    // 获取指定key的对象偏移量
		bool hasKey(int key) const;                 // 检查是否存在指定的key
		void removeKey(int key);                    // 从索引表中移除指定key
		size_t getOffsetTableSize() const;          // 获取索引表大小
		
		// 随机访问方法
		void seek(std::streampos pos);              // 定位到文件指定位置
		std::streampos tell();                      // 获取当前文件位置
		
		// 基于key的读写方法
		template<typename T>
		void writeObjAtKey(int key, const T& obj);
		template<typename T>
		bool readObjAtKey(int key, T& obj);
	};
}