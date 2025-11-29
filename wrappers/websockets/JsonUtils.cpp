#include "JsonUtils.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>

#include "Protocol.hpp"

using nlohmann::json;

namespace ws
{
    std::string toLowerCopy(std::string_view view)
    {
        std::string out(view.begin(), view.end());
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
        return out;
    }

    std::string toUpperCopy(std::string_view view)
    {
        std::string out(view.begin(), view.end());
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::toupper(c));
        });
        return out;
    }

    SSDB::FieldType parseFieldType(const std::string& value)
    {
        auto lower = toLowerCopy(value);
        if (lower == "string") return SSDB::FieldType::String;
        if (lower == "int") return SSDB::FieldType::Int;
        if (lower == "double") return SSDB::FieldType::Double;
        throw ApiError(400, "Unsupported field type: " + value);
    }

    void ensureSchemaReady(const SSDB::SchemaDef& schema, std::string_view target)
    {
        if (schema.empty())
        {
            throw ApiError(422, std::string(target) + " schema is not defined.");
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
                throw ApiError(400, "Field type for '" + field + "' must be string.");
            }
            schema[field] = parseFieldType(typeNode.get<std::string>());
        }
        return schema;
    }

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

    double requireNumber(const json& value, std::string_view context)
    {
        if (!value.is_number())
        {
            throw ApiError(422, std::string(context) + " must be numeric.");
        }
        return value.get<double>();
    }

    bool compareNumbers(double lhs, double rhs, const std::string& op)
    {
        if (op == "==") return lhs == rhs;
        if (op == "!=") return lhs != rhs;
        if (op == ">") return lhs > rhs;
        if (op == ">=") return lhs >= rhs;
        if (op == "<") return lhs < rhs;
        if (op == "<=") return lhs <= rhs;
        throw ApiError(422, "Unsupported numeric operator: " + op);
    }

    bool compareStrings(const std::string& lhs, const std::string& rhs, const std::string& opLower)
    {
        if (opLower == "==") return lhs == rhs;
        if (opLower == "!=") return lhs != rhs;
        if (opLower == "contains") return lhs.find(rhs) != std::string::npos;
        if (opLower == "starts_with") return lhs.rfind(rhs, 0) == 0;
        if (opLower == "ends_with")
        {
            if (rhs.size() > lhs.size()) return false;
            return std::equal(rhs.rbegin(), rhs.rend(), lhs.rbegin());
        }
        throw ApiError(422, "Unsupported string operator: " + opLower);
    }

    bool evaluateLogicNode(const json& entityData, const json& node, const SSDB::SchemaDef& schema)
    {
        if (!node.is_object())
        {
            throw ApiError(400, "logic node must be an object.");
        }

        if (node.contains("field"))
        {
            auto field = node.at("field").get<std::string>();
            auto opRaw = node.at("op").get<std::string>();
            if (!node.contains("val"))
            {
                throw ApiError(400, "Leaf rule is missing 'val'.");
            }
            auto schemaIt = schema.find(field);
            if (schemaIt == schema.end())
            {
                throw ApiError(422, "Field '" + field + "' is not defined in schema.");
            }
            if (!entityData.contains(field))
            {
                return false;
            }
            const auto& lhs = entityData.at(field);
            const auto& rhs = node.at("val");
            auto type = schemaIt->second;
            if (type == SSDB::FieldType::String)
            {
                if (!lhs.is_string() || !rhs.is_string())
                {
                    throw ApiError(422, "String comparison requires string operands.");
                }
                return compareStrings(lhs.get_ref<const std::string&>(), rhs.get_ref<const std::string&>(), toLowerCopy(opRaw));
            }
            if (type == SSDB::FieldType::Int || type == SSDB::FieldType::Double)
            {
                double lhsVal = requireNumber(lhs, field);
                double rhsVal = requireNumber(rhs, "val");
                return compareNumbers(lhsVal, rhsVal, opRaw);
            }
            throw ApiError(422, "Unsupported field type in logic rule.");
        }

        auto op = toUpperCopy(node.at("op").get<std::string>());
        const auto& rules = node.at("rules");
        if (!rules.is_array() || rules.empty())
        {
            throw ApiError(400, "logic.rules must be a non-empty array.");
        }

        if (op == "AND")
        {
            for (const auto& child : rules)
            {
                if (!evaluateLogicNode(entityData, child, schema))
                {
                    return false;
                }
            }
            return true;
        }
        if (op == "OR")
        {
            for (const auto& child : rules)
            {
                if (evaluateLogicNode(entityData, child, schema))
                {
                    return true;
                }
            }
            return false;
        }
        throw ApiError(400, "Unsupported logic operator: " + op);
    }
}

