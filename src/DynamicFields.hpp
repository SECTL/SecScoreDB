/**
 * @file DynamicFields.hpp
 * @brief 动态字段访问包装器
 */
#pragma once

#include "Group.h"
#include "Schema.hpp"
#include "Student.h"

#include <charconv>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace SSDB
{
    /**
     * @brief 动态字段访问包装器
     *
     * 提供基于 Schema 的类型安全字段访问
     */
    template <MetadataEntity T>
    class DynamicWrapper
    {
    private:
        T& obj_;
        const SchemaDef& schema_;

    public:
        // 禁用默认构造和复制赋值
        DynamicWrapper() = delete;
        DynamicWrapper& operator=(const DynamicWrapper&) = delete;

        // 允许移动构造
        DynamicWrapper(DynamicWrapper&&) noexcept = default;

        DynamicWrapper(T& obj, const SchemaDef& schema) noexcept
            : obj_(obj)
            , schema_(schema)
        {
        }

        /**
         * @brief 字段代理类
         *
         * 提供类型安全的字段读写操作
         */
        struct FieldProxy
        {
            T& obj;
            const SchemaDef::value_type* schemaEntry;

            [[nodiscard]] std::string_view Name() const noexcept { return schemaEntry->first; }
            [[nodiscard]] FieldType Type() const noexcept { return schemaEntry->second; }

            // Setter
            template <SupportedValue V>
            FieldProxy& operator=(const V& value)
            {
                if (getTypeId<V>() != Type())
                {
                    throw std::runtime_error(std::format(
                        "Type mismatch for field '{}'. Expected {}, got {}.",
                        Name(),
                        fieldTypeToString(Type()),
                        fieldTypeToString(getTypeId<V>())
                    ));
                }

                if constexpr (std::is_arithmetic_v<std::decay_t<V>>)
                {
                    obj.SetMetadataValue(std::string(Name()), std::format("{}", value));
                }
                else
                {
                    obj.SetMetadataValue(std::string(Name()), value);
                }
                return *this;
            }

            // String getter
            [[nodiscard]] operator std::string() const
            {
                if (Type() != FieldType::String)
                {
                    throw std::runtime_error(std::format(
                        "Type mismatch for field '{}'. Expected String, got {}.",
                        Name(),
                        fieldTypeToString(Type())
                    ));
                }
                return obj.GetMetadataValue(std::string(Name()));
            }

            // Arithmetic getter
            template <typename V>
                requires std::is_arithmetic_v<V>
            [[nodiscard]] operator V() const
            {
                if (getTypeId<V>() != Type())
                {
                    throw std::runtime_error(std::format(
                        "Type mismatch for field '{}' during read. Expected {}, requested {}.",
                        Name(),
                        fieldTypeToString(Type()),
                        fieldTypeToString(getTypeId<V>())
                    ));
                }

                const std::string str = obj.GetMetadataValue(std::string(Name()));

                if (str.empty())
                {
                    throw std::runtime_error(std::format(
                        "Value for field '{}' is empty, cannot convert to number.",
                        Name()
                    ));
                }

                V val{};
                const auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);

                if (ec == std::errc::invalid_argument)
                {
                    throw std::runtime_error(std::format(
                        "Invalid number format for field '{}': \"{}\"",
                        Name(), str
                    ));
                }
                if (ec == std::errc::result_out_of_range)
                {
                    throw std::runtime_error(std::format(
                        "Number out of range for field '{}': \"{}\"",
                        Name(), str
                    ));
                }
                if (ptr != str.data() + str.size())
                {
                    throw std::runtime_error(std::format(
                        "Partial conversion error for field '{}': \"{}\"",
                        Name(), str
                    ));
                }

                return val;
            }
        };

        // operator[] 实现（使用显式对象参数 - C++23）
        template <typename Self>
        [[nodiscard]] auto operator[](this Self&& self, std::string_view key)
        {
            auto it = self.schema_.find(std::string(key));

            if (it == self.schema_.end())
            {
                throw std::runtime_error(std::format(
                    "Field '{}' is not defined in the Schema.",
                    key
                ));
            }

            return FieldProxy{self.obj_, &(*it)};
        }

        /**
         * @brief 获取底层实体的只读引用
         */
        [[nodiscard]] const T& GetEntity() const noexcept
        {
            return obj_;
        }

        /**
         * @brief 获取底层实体的引用
         */
        [[nodiscard]] T& GetMutableEntity() noexcept
        {
            return obj_;
        }
    };
}
