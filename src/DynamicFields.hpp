// DynamicWrapper.h
#pragma once
#include "Schema.hpp"
#include "Group.h"
#include "Student.h"
#include <format>
#include <charconv>
#include <print>      // C++23 只有 MSVC/Clang19 支持得好
#include <stdexcept>

namespace SSDB
{
    template <MetadataEntity T> // 使用 Concept 约束 T 必须有 Metadata 接口
    class DynamicWrapper
    {
    private:
        T& _obj;
        const SchemaDef& _schema;

    public:
        DynamicWrapper(T& obj, const SchemaDef& schema) : _obj(obj), _schema(schema)
        {
        }

        // 内部代理：处理 s["key"] 的中间状态
        struct FieldProxy
        {
            T& obj;
            const SchemaDef& schema;
            std::string key;

            // Setter: s["age"] = 18
            template <SupportedValue V>
            FieldProxy& operator=(const V& value)
            {
                Validate(getTypeId<V>());

                if constexpr (std::is_arithmetic_v<std::decay_t<V>>)
                {
                    obj.SetMetadataValue(key, std::format("{}", value)); // C++20 format
                }
                else
                {
                    obj.SetMetadataValue(key, value);
                }
                return *this;
            }

            // Getter: int x = s["age"]
            template <SupportedValue V>
            operator V() const
            {
                Validate(getTypeId<V>());
                std::string str = obj.GetMetadataValue(key);

                if constexpr (std::is_same_v<std::decay_t<V>, std::string>)
                {
                    return str;
                }
                else
                {
                    V val{};
                    // MSVC 对浮点数的 from_chars 支持极其完美
                    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);
                    if (ec != std::errc()) return V{}; // 解析失败返回0
                    return val;
                }
            }

        private:
            void Validate(FieldType type) const
            {
                if (!schema.contains(key)) // C++20 contains
                    throw std::runtime_error(std::format("Field '{}' not found in Schema", key));
                if (schema.at(key) != type)
                    throw std::runtime_error(std::format("Type mismatch for '{}'", key));
            }
        };

        // C++23 Deducing This: 显式对象参数
        // 不再需要写 const重载 和 非const重载 两个版本
        template <typename Self>
        auto operator[](this Self&& self, std::string key)
        {
            return FieldProxy{self._obj, self._schema, std::move(key)};
        }
    };
}
