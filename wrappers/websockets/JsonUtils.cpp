/**
 * @file JsonUtils.cpp
 * @brief JSON 工具函数实现
 */
#include "JsonUtils.hpp"

#include "Protocol.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <format>
#include <string>

using nlohmann::json;

namespace ws
{
    // ============================================================
    // 字符串工具
    // ============================================================

    std::string toLowerCopy(std::string_view view)
    {
        std::string out(view);
        std::ranges::transform(out, out.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return out;
    }

    std::string toUpperCopy(std::string_view view)
    {
        std::string out(view);
        std::ranges::transform(out, out.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        return out;
    }

    // ============================================================
    // Schema 工具
    // ============================================================

    SSDB::FieldType parseFieldType(std::string_view value)
    {
        const auto lower = toLowerCopy(value);

        if (lower == "string") return SSDB::FieldType::String;
        if (lower == "int") return SSDB::FieldType::Int;
        if (lower == "double") return SSDB::FieldType::Double;

        throw ApiError(400, std::format("Unsupported field type: {}", value));
    }

    void ensureSchemaReady(const SSDB::SchemaDef& schema, std::string_view target)
    {
        if (schema.empty())
        {
            throw ApiError(422, std::format("{} schema is not defined.", target));
        }
    }

    SSDB::SchemaDef parseSchema(const json& schemaJson)
    {
        if (!schemaJson.is_object() || schemaJson.empty())
        {
            throw ApiError(400, "schema must be a non-empty object.");
        }

        SSDB::SchemaDef schema;
        for (const auto& [field, typeNode] : schemaJson.items())
        {
            if (!typeNode.is_string())
            {
                throw ApiError(400, std::format("Field type for '{}' must be string.", field));
            }
            schema[field] = parseFieldType(typeNode.get<std::string>());
        }

        return schema;
    }

    // ============================================================
    // 值处理
    // ============================================================

    std::optional<json> decodeStoredValue(const std::string& raw, SSDB::FieldType type)
    {
        try
        {
            switch (type)
            {
                case SSDB::FieldType::String:
                    return raw;

                case SSDB::FieldType::Int:
                    return static_cast<long long>(std::stoll(raw));

                case SSDB::FieldType::Double:
                    return std::stod(raw);

                default:
                    return std::nullopt;
            }
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    // ============================================================
    // 比较和逻辑
    // ============================================================

    double requireNumber(const json& value, std::string_view context)
    {
        if (!value.is_number())
        {
            throw ApiError(422, std::format("{} must be numeric.", context));
        }
        return value.get<double>();
    }

    bool compareNumbers(double lhs, double rhs, std::string_view op)
    {
        if (op == "==") return lhs == rhs;
        if (op == "!=") return lhs != rhs;
        if (op == ">")  return lhs > rhs;
        if (op == ">=") return lhs >= rhs;
        if (op == "<")  return lhs < rhs;
        if (op == "<=") return lhs <= rhs;

        throw ApiError(422, std::format("Unsupported numeric operator: {}", op));
    }

    bool compareStrings(const std::string& lhs, const std::string& rhs, std::string_view opLower)
    {
        if (opLower == "==") return lhs == rhs;
        if (opLower == "!=") return lhs != rhs;
        if (opLower == "contains") return lhs.find(rhs) != std::string::npos;
        if (opLower == "starts_with") return lhs.starts_with(rhs);
        if (opLower == "ends_with") return lhs.ends_with(rhs);

        throw ApiError(422, std::format("Unsupported string operator: {}", opLower));
    }

    bool evaluateLogicNode(const json& entityData, const json& node, const SSDB::SchemaDef& schema)
    {
        if (!node.is_object())
        {
            throw ApiError(400, "logic node must be an object.");
        }

        // 叶子节点：字段比较
        if (node.contains("field"))
        {
            const auto field = node.at("field").get<std::string>();
            const auto opRaw = node.at("op").get<std::string>();

            if (!node.contains("val"))
            {
                throw ApiError(400, "Leaf rule is missing 'val'.");
            }

            auto schemaIt = schema.find(field);
            if (schemaIt == schema.end())
            {
                throw ApiError(422, std::format("Field '{}' is not defined in schema.", field));
            }

            if (!entityData.contains(field))
            {
                return false;
            }

            const auto& lhs = entityData.at(field);
            const auto& rhs = node.at("val");
            const auto type = schemaIt->second;

            if (type == SSDB::FieldType::String)
            {
                if (!lhs.is_string() || !rhs.is_string())
                {
                    throw ApiError(422, "String comparison requires string operands.");
                }
                return compareStrings(lhs.get_ref<const std::string&>(),
                                     rhs.get_ref<const std::string&>(),
                                     toLowerCopy(opRaw));
            }

            if (type == SSDB::FieldType::Int || type == SSDB::FieldType::Double)
            {
                const double lhsVal = requireNumber(lhs, field);
                const double rhsVal = requireNumber(rhs, "val");
                return compareNumbers(lhsVal, rhsVal, opRaw);
            }

            throw ApiError(422, "Unsupported field type in logic rule.");
        }

        // 组合节点：AND/OR
        const auto op = toUpperCopy(node.at("op").get<std::string>());
        const auto& rules = node.at("rules");

        if (!rules.is_array() || rules.empty())
        {
            throw ApiError(400, "logic.rules must be a non-empty array.");
        }

        if (op == "AND")
        {
            return std::ranges::all_of(rules, [&](const auto& child) {
                return evaluateLogicNode(entityData, child, schema);
            });
        }

        if (op == "OR")
        {
            return std::ranges::any_of(rules, [&](const auto& child) {
                return evaluateLogicNode(entityData, child, schema);
            });
        }

        throw ApiError(400, std::format("Unsupported logic operator: {}", op));
    }

} // namespace ws

