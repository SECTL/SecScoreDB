#pragma once
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <iostream>
#include "SSDBType.h"

// Cereal 头文件
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/map.hpp>
// 这一行很重要，支持 unordered_map 的直接序列化
#include <cereal/types/unordered_map.hpp>

namespace SSDB
{
    namespace fs = std::filesystem;

    class DataBaseFile
    {
    private:
        fs::path filePath;
        std::fstream File;

    public:
        explicit DataBaseFile(fs::path path);
        ~DataBaseFile();

        // 禁用拷贝
        DataBaseFile(const DataBaseFile&) = delete;
        DataBaseFile& operator=(const DataBaseFile&) = delete;

        // =======================================================
        //  集成核心：全量加载与保存
        // =======================================================

        // 启动时：读取整个文件到 Map 中
        template<typename T>
        std::unordered_map<int, T> LoadAll()
        {
            std::unordered_map<int, T> dataMap;

            // 确保处于读模式且在文件头
            if (!File.is_open()) return dataMap;

            // 检查文件是否为空
            File.seekg(0, std::ios::end);
                        if (File.tellg() == 0) {
                File.seekg(0, std::ios::beg); // 保证指针回到文件头
                return dataMap; // 空文件直接返回
            }
            File.seekg(0, std::ios::beg);

            try {
                cereal::BinaryInputArchive archive(File);

                // 策略 A：直接序列化整个 Map (最简单，推荐)
                // Cereal 会自动处理 Map 的大小和结构
                archive(dataMap);

                // 策略 B (备选)：如果你是逐个对象存的，就需要循环读
                // while(file_good) { T obj; archive(obj); dataMap[obj.GetId()] = std::move(obj); }
            }
            catch (const std::exception& e) {
                std::cerr << "[DB Load Error] " << filePath << ": " << e.what() << std::endl;
                // 出错时返回已读取的部分或空 map，视策略而定
            }

            return dataMap;
        }

        // Commit时：将整个 Map 覆盖写入文件
        template<typename T>
        void SaveAll(const std::unordered_map<int, T>& dataMap)
        {
            // 1. 关闭当前文件流
            File.close();

            // 2. 以 Truncate (截断/清空) 模式重新打开
            File.open(filePath, std::ios::out | std::ios::binary | std::ios::trunc);
            if (!File.is_open()) {
                throw std::runtime_error("Failed to open file for writing: " + filePath.string());
            }

            // 3. 写入数据
            try {
                cereal::BinaryOutputArchive archive(File);
                // 直接序列化整个 Map
                archive(dataMap);
            }
            catch (const std::exception& e) {
                 std::cerr << "[DB Save Error] " << filePath << ": " << e.what() << std::endl;
            }

                       File.open(filePath, std::ios::in | std::ios::out | std::ios::binary);
            if (!File.is_open()) {
                throw std::runtime_error("Failed to reopen file in read-write mode: " + filePath.string());
            }        File.open(filePath, std::ios::in | std::ios::out | std::ios::binary);
        }
    };
}