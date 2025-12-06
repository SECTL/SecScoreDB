/**
 * @file JsonUtils.hpp
 * @brief JSON 工具函数和模板
 */
#pragma once

#include "Errors.hpp"
#include "SecScoreDB.h"

#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace ws
{
    // ============================================================
    // 字符串工具
    // ============================================================

    /**
     * @brief 转换为小写副本
     */
    [[nodiscard]] std::string toLowerCopy(std::string_view view);

    /**
     * @brief 转换为大写副本
     */
    [[nodiscard]] std::string toUpperCopy(std::string_view view);

    // ============================================================
    // Schema 工具
    // ============================================================

    /**
     * @brief 解析字段类型字符串
     */
    [[nodiscard]] SSDB::FieldType parseFieldType(std::string_view value);

    /**
     * @brief 确保 Schema 已初始化
     */
    void ensureSchemaReady(const SSDB::SchemaDef& schema, std::string_view target);

    /**
     * @brief 从 JSON 解析 Schema
     */
    [[nodiscard]] SSDB::SchemaDef parseSchema(const nlohmann::json& schemaJson);

    // ============================================================
    // 值处理
    // ============================================================

    /**
     * @brief 解码存储的字符串值
     */
    [[nodiscard]] std::optional<nlohmann::json> decodeStoredValue(const std::string& raw,
                                                                   SSDB::FieldType type);

    /**
     * @brief 将实体数据物化为 JSON
     */
    template <typename Entity>
    [[nodiscard]] nlohmann::json materializeEntityData(const Entity& entity,
                                                        const SSDB::SchemaDef& schema)
    {
        nlohmann::json data = nlohmann::json::object();
        const auto& meta = entity.GetMetadata();

        for (const auto& [field, type] : schema)
        {
            if (auto it = meta.find(field); it != meta.end())
            {
                if (auto decoded = decodeStoredValue(it->second, type))
                {
                    data[field] = *decoded;
                }
            }
        }

        return data;
    }

    // ============================================================
    // 比较和逻辑
    // ============================================================

    /**
     * @brief 要求值为数字，否则抛出异常
     */
    [[nodiscard]] double requireNumber(const nlohmann::json& value, std::string_view context);

    /**
     * @brief 比较两个数字
     */
    [[nodiscard]] bool compareNumbers(double lhs, double rhs, std::string_view op);

    /**
     * @brief 比较两个字符串
     */
    [[nodiscard]] bool compareStrings(const std::string& lhs,
                                      const std::string& rhs,
                                      std::string_view opLower);

    /**
     * @brief 评估逻辑节点
     */
    [[nodiscard]] bool evaluateLogicNode(const nlohmann::json& entityData,
                                         const nlohmann::json& node,
                                         const SSDB::SchemaDef& schema);

    // ============================================================
    // 动态字段赋值
    // ============================================================

    /**
     * @brief 将 JSON 数据赋值到动态字段包装器
     */
    template <typename Wrapper>
    void assignDynamicFields(Wrapper& wrapper,
                             const nlohmann::json& data,
                             const SSDB::SchemaDef& schema)
    {
        if (!data.is_object())
        {
            throw ApiError(400, "data must be an object.");
        }

        for (const auto& [field, value] : data.items())
        {
            auto it = schema.find(field);
            if (it == schema.end())
            {
                throw ApiError(422, "Field '" + field + "' is not defined in schema.");
            }

            switch (it->second)
            {
                case SSDB::FieldType::String:
                    if (!value.is_string())
                    {
                        throw ApiError(422, "Field '" + field + "' requires string value.");
                    }
                    wrapper[field] = value.get_ref<const std::string&>();
                    break;

                case SSDB::FieldType::Int:
                    if (!value.is_number_integer())
                    {
                        throw ApiError(422, "Field '" + field + "' requires integer value.");
                    }
                    wrapper[field] = value.get<long long>();
                    break;

                case SSDB::FieldType::Double:
                    if (!value.is_number())
                    {
                        throw ApiError(422, "Field '" + field + "' requires numeric value.");
                    }
                    wrapper[field] = value.get<double>();
                    break;

                default:
                    throw ApiError(422, "Unsupported field type for '" + field + "'.");
            }
        }
    }

} // namespace ws
