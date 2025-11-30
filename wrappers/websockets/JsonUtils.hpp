#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "SecScoreDB.h"
#include "Errors.hpp"

namespace ws
{
    std::string toLowerCopy(std::string_view view);
    std::string toUpperCopy(std::string_view view);

    SSDB::FieldType parseFieldType(const std::string& value);
    void ensureSchemaReady(const SSDB::SchemaDef& schema, std::string_view target);
    SSDB::SchemaDef parseSchema(const nlohmann::json& schemaJson);

    std::optional<nlohmann::json> decodeStoredValue(const std::string& raw, SSDB::FieldType type);

    template <typename Entity>
    nlohmann::json materializeEntityData(const Entity& entity, const SSDB::SchemaDef& schema)
    {
        nlohmann::json data = nlohmann::json::object();
        const auto& meta = entity.GetMetadata();
        for (const auto& [field, type] : schema)
        {
            auto it = meta.find(field);
            if (it == meta.end())
            {
                continue;
            }
            if (auto decoded = decodeStoredValue(it->second, type))
            {
                data[field] = *decoded;
            }
        }
        return data;
    }

    double requireNumber(const nlohmann::json& value, std::string_view context);
    bool compareNumbers(double lhs, double rhs, const std::string& op);
    bool compareStrings(const std::string& lhs, const std::string& rhs, const std::string& opLower);
    bool evaluateLogicNode(const nlohmann::json& entityData,
                           const nlohmann::json& node,
                           const SSDB::SchemaDef& schema);

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
}
