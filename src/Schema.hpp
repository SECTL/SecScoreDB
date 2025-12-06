#pragma once

#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

namespace SSDB
{
    /**
     * @brief 字段类型枚举
     */
    enum class FieldType : std::uint8_t
    {
        Int,
        Double,
        String,
        Unknown
    };

    /**
     * @brief Schema 定义类型
     */
    using SchemaDef = std::unordered_map<std::string, FieldType>;

    /**
     * @brief 支持的值类型 concept
     */
    template <typename T>
    concept SupportedValue = std::integral<T> || std::floating_point<T> || std::convertible_to<T, std::string>;

    /**
     * @brief 编译期获取类型对应的 FieldType
     */
    template <typename T>
    [[nodiscard]] constexpr FieldType getTypeId() noexcept
    {
        using Type = std::decay_t<T>;

        if constexpr (std::is_same_v<Type, std::string> ||
                      std::is_same_v<Type, std::string_view> ||
                      std::is_same_v<Type, const char*>)
        {
            return FieldType::String;
        }
        else if constexpr (std::floating_point<Type>)
        {
            return FieldType::Double;
        }
        else if constexpr (std::integral<Type>)
        {
            return FieldType::Int;
        }
        else
        {
            return FieldType::Unknown;
        }
    }

    /**
     * @brief 将 FieldType 转换为字符串（用于调试/日志）
     */
    [[nodiscard]] constexpr std::string_view fieldTypeToString(FieldType type) noexcept
    {
        switch (type)
        {
            case FieldType::Int:     return "Int";
            case FieldType::Double:  return "Double";
            case FieldType::String:  return "String";
            case FieldType::Unknown: return "Unknown";
            default:                 return "Invalid";
        }
    }
}