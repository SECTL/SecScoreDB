#pragma once
#include "Schema.hpp"
#include <string>
#include <format>   // C++20
#include <charconv> // 高性能转换
#include <stdexcept>

namespace SSDB {

    // T 可以是 Student 或 Group
    template<typename T>
    class DynamicWrapper {
    private:
        T& _obj;                  // 实际对象的引用
        const SchemaDef& _schema; // 对应的 Schema

    public:
        DynamicWrapper(T& obj, const SchemaDef& schema) : _obj(obj), _schema(schema) {}

        // 内部代理类，表示 obj["key"] 的结果
        struct FieldProxy {
            T& obj;
            const SchemaDef& schema;
            std::string key;

            // 1. Setter: obj["age"] = 20;
            template<SupportedValue V>
            FieldProxy& operator=(const V& value) {
                CheckSchema(getTypeId<V>());

                // 序列化：利用 C++20 format
                if constexpr (std::is_arithmetic_v<std::decay_t<V>>) {
                    obj.SetMetadataValue(key, std::format("{}", value));
                } else {
                    obj.SetMetadataValue(key, value);
                }
                return *this;
            }

            // 2. Getter: int age = obj["age"];
            template<SupportedValue V>
            explicit operator V() const {
                CheckSchema(getTypeId<V>());
                std::string strVal = obj.GetMetadataValue(key);

                if constexpr (std::is_same_v<std::decay_t<V>, std::string>) {
                    return strVal;
                } else {
                    // 反序列化：利用 C++17/20 from_chars (性能远超 stoi)
                    V result{};
                    auto [ptr, ec] = std::from_chars(strVal.data(), strVal.data() + strVal.size(), result);
                    if (ec != std::errc()) {
                        // 解析失败处理，这里简单返回 0
                        return static_cast<V>(0);
                    }
                    return result;
                }
            }

        private:
            // 核心逻辑：运行时类型检查
            void CheckSchema(FieldType incomingType) const {
                auto it = schema.find(key);
                if (it == schema.end()) {
                    throw std::runtime_error(std::format("Error: Field '{}' is not defined in Schema.", key));
                }
                if (it->second != incomingType) {
                    // 允许 int 和 double 之间某种程度的混用逻辑可以在这里放宽，
                    // 但为了严格模拟强类型，这里直接报错。
                    throw std::runtime_error(std::format("Error: Type mismatch for field '{}'.", key));
                }
            }
        };

        // 3. 索引操作符重载
        FieldProxy operator[](std::string key) {
            return FieldProxy{ _obj, _schema, std::move(key) };
        }
    };
}