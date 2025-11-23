#pragma once
#include "Schema.hpp"
#include "Group.h"
#include "Student.h"
#include <format>
#include <charconv>
#include <print>
#include <stdexcept>
#include <string_view>

namespace SSDB
{
    template <MetadataEntity T>
    class DynamicWrapper
    {
    private:
        T& _obj;
        const SchemaDef& _schema;

    public:
        // 明确删除默认构造和复制赋值，强调它是引用视图
        DynamicWrapper() = delete;
        DynamicWrapper& operator=(const DynamicWrapper&) = delete;

        // 允许移动构造（配合 AddStudent 返回值）
        DynamicWrapper(DynamicWrapper&&) = default;

        DynamicWrapper(T& obj, const SchemaDef& schema) : _obj(obj), _schema(schema)
        {
        }

        // -----------------------------------------------------------
        // 优化点 1 & 3: FieldProxy 直接持有 Schema 指针，避免 map 查找和 key 拷贝
        // -----------------------------------------------------------
        struct FieldProxy
        {
            T& obj;
            // 存储指向 Schema 条目的指针 (std::pair<const string, FieldType>*)
            // 这要求 Schema 在 Wrapper 生命周期内必须稳定 (map/unordered_map 只要不删key就是稳定的)
            const SchemaDef::value_type* schemaEntry;

            // 辅助：获取字段名和类型
            std::string_view Name() const { return schemaEntry->first; }
            FieldType Type() const { return schemaEntry->second; }

            // Setter
            template <SupportedValue V>
            FieldProxy& operator=(const V& value)
            {
                // 类型检查优化：错误信息包含具体的类型名称
                if (getTypeId<V>() != Type())
                {
                    throw std::runtime_error(std::format(
                        "Type mismatch for field '{}'. Expected {}, got {}.",
                        Name(), (int)Type(), (int)getTypeId<V>() // 实际代码可以用 Helper 转枚举名为字符串
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

            // Getter
            template <SupportedValue V>
            operator V() const
            {
                // 类型检查
                if (getTypeId<V>() != Type())
                {
                    throw std::runtime_error(std::format(
                        "Type mismatch for field '{}' during read. Expected {}, requested {}.",
                        Name(), (int)Type(), (int)getTypeId<V>()
                    ));
                }

                std::string str = obj.GetMetadataValue(std::string(Name()));

                if constexpr (std::is_same_v<std::decay_t<V>, std::string>)
                {
                    return str;
                }
                else
                {
                    // -----------------------------------------------------------
                    // 优化点 2: 严厉的异常处理
                    // -----------------------------------------------------------
                    if (str.empty())
                    {
                        // 空字符串对于数字类型来说是无效的，抛异常还是返0？
                        // 工业级通常抛异常，或者提供 Get(default_val) 接口
                        throw std::runtime_error(std::format("Value for field '{}' is empty, cannot convert to number.",
                                                             Name()));
                    }

                    V val{};
                    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);

                    if (ec == std::errc::invalid_argument)
                    {
                        throw std::runtime_error(std::format("Invalid number format for field '{}': \"{}\"", Name(),
                                                             str));
                    }
                    else if (ec == std::errc::result_out_of_range)
                    {
                        throw std::runtime_error(std::format("Number out of range for field '{}': \"{}\"", Name(),
                                                             str));
                    }

                    // 确保整个字符串都被解析了 (避免 "123abc" 被解析成 123)
                    if (ptr != str.data() + str.size())
                    {
                        throw std::runtime_error(std::format("Partial conversion error for field '{}': \"{}\"", Name(),
                                                             str));
                    }

                    return val;
                }
            }
        };

        // operator[] 实现
        template <typename Self>
        auto operator[](this Self&& self, std::string_view key)
        {
            // 1. 这里做一次查找 (Map Lookup)
            // 注意：C++20 unordered_map 支持异构查找 (transparent key comparison)
            // 如果编译器支持，直接传 string_view，无需构造 string
            auto it = self._schema.find(std::string(key));

            if (it == self._schema.end())
            {
                throw std::runtime_error(std::format("Field '{}' is not defined in the Schema.", key));
            }

            // 2. 将迭代器指针传给 Proxy，避免 Proxy 内部再次查找
            return FieldProxy{self._obj, &(*it)};
        }

        // -----------------------------------------------------------
        // 优化点 4: 显式暴露生命周期风险的接口
        // -----------------------------------------------------------
        // 增加一个 IsValid() 方法很难，因为只有 DB 知道。
        // 但我们可以提供 ToStruct() 方法，快速把数据拷出来，脱离引用。
        // (这需要复杂的元编程把 Schema 转 struct，暂时略过)
        const T& GetEntity() const {
            return _obj;
        }
    };
}
