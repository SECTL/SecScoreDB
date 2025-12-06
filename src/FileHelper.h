/**
 * @file FileHelper.h
 * @brief 数据库文件操作辅助类
 */
#pragma once

#include "SSDBType.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>

// Cereal 序列化
#include <cereal/archives/binary.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/vector.hpp>

namespace SSDB
{
    /**
     * @brief 数据库文件操作类
     *
     * 提供二进制文件的读写操作，使用 Cereal 进行序列化
     */
    class DataBaseFile
    {
    private:
        fs::path filePath_;
        std::fstream file_;

    public:
        explicit DataBaseFile(fs::path path);
        ~DataBaseFile();

        // 禁用拷贝
        DataBaseFile(const DataBaseFile&) = delete;
        DataBaseFile& operator=(const DataBaseFile&) = delete;

        // 允许移动
        DataBaseFile(DataBaseFile&&) noexcept = default;
        DataBaseFile& operator=(DataBaseFile&&) noexcept = default;

        /**
         * @brief 获取文件路径
         */
        [[nodiscard]] const fs::path& GetPath() const noexcept { return filePath_; }

        /**
         * @brief 从文件加载所有数据
         * @tparam T 数据类型（需要支持 Cereal 序列化）
         * @return 加载的数据映射表
         */
        template <typename T>
        [[nodiscard]] std::unordered_map<int, T> LoadAll()
        {
            std::unordered_map<int, T> dataMap;

            if (!file_.is_open())
            {
                return dataMap;
            }

            // 检查文件是否为空
            file_.seekg(0, std::ios::end);
            if (file_.tellg() == 0)
            {
                file_.seekg(0, std::ios::beg);
                return dataMap;
            }
            file_.seekg(0, std::ios::beg);

            try
            {
                cereal::BinaryInputArchive archive(file_);
                archive(dataMap);
            }
            catch (const std::exception& e)
            {
                std::cerr << "[DB Load Error] " << filePath_ << ": " << e.what() << '\n';
            }

            return dataMap;
        }

        /**
         * @brief 将所有数据保存到文件
         * @tparam T 数据类型（需要支持 Cereal 序列化）
         * @param dataMap 要保存的数据映射表
         */
        template <typename T>
        void SaveAll(const std::unordered_map<int, T>& dataMap)
        {
            // 关闭当前文件流
            file_.close();

            // 以截断模式重新打开
            file_.open(filePath_, std::ios::out | std::ios::binary | std::ios::trunc);
            if (!file_.is_open())
            {
                throw std::runtime_error("Failed to open file for writing: " + filePath_.string());
            }

            // 写入数据
            try
            {
                cereal::BinaryOutputArchive archive(file_);
                archive(dataMap);
            }
            catch (const std::exception& e)
            {
                std::cerr << "[DB Save Error] " << filePath_ << ": " << e.what() << '\n';
                throw;
            }

            // 刷新并关闭
            file_.flush();
            file_.close();

            // 重新以读写模式打开
            file_.open(filePath_, std::ios::in | std::ios::out | std::ios::binary);
            if (!file_.is_open())
            {
                throw std::runtime_error("Failed to reopen file in read-write mode: " + filePath_.string());
            }
        }
    };
}