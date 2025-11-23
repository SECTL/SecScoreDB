#pragma once

#include <string>
#include <unordered_map>
#include <concepts>
#include <type_traits>

namespace SSDB
{
    enum class FieldType
    {
        Int,
        Double,
        String,
        Unkown
    };

    using SchemaDef = std::unordered_map<std::string, FieldType>;
    template<typename T>
    concept SupportedValue=std::integral<int>||std::floating_point<double>||std::convertible_to<T,std::string>;

    template<typename T>
    consteval FieldType getTypeId()
    {
        using TYPE=std::decay_t<T>;
        if constexpr(std::is_same_v<TYPE,std::string>||std::is_same_v<TYPE,const char *>)
            return FieldType::String;
        else if constexpr (std::floating_point<TYPE>)
            return FieldType::Double;
        else if constexpr(std::integral<TYPE>)
            return FieldType::Int;
        else
            return FieldType::Unkown;
    }
}