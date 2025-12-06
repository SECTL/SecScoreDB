/**
 * @file SSDBType.h
 * @brief SecScoreDB 核心类型定义
 */
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>

namespace SSDB
{
    /**
     * @brief 元数据类型（键值对存储）
     */
    using t_metadata = std::map<std::string, std::string, std::less<>>;

    /**
     * @brief 文件系统命名空间别名
     */
    namespace fs = std::filesystem;

    /**
     * @brief 事件类型枚举
     */
    enum class EventType : std::uint8_t
    {
        Group,
        Student
    };

    /**
     * @brief 数据库文件类型枚举
     */
    enum class DataBaseFileType : std::uint8_t
    {
        Event,
        Student,
        Group
    };

    /**
     * @brief 将 EventType 转换为字符串
     */
    [[nodiscard]] constexpr std::string_view eventTypeToString(EventType type) noexcept
    {
        switch (type)
        {
            case EventType::Group:   return "Group";
            case EventType::Student: return "Student";
            default:                 return "Unknown";
        }
    }
}