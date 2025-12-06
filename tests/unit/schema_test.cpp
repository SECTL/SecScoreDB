/**
 * @file schema_test.cpp
 * @brief Schema 模块单元测试
 */
#include <gtest/gtest.h>
#include "Schema.hpp"

using namespace SSDB;

// ============================================================
// Schema 类型测试
// ============================================================

class SchemaTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SchemaTest, GetTypeIdForInt)
{
    EXPECT_EQ(getTypeId<int>(), FieldType::Int);
    EXPECT_EQ(getTypeId<short>(), FieldType::Int);
    EXPECT_EQ(getTypeId<long>(), FieldType::Int);
    EXPECT_EQ(getTypeId<long long>(), FieldType::Int);
    EXPECT_EQ(getTypeId<unsigned int>(), FieldType::Int);
}

TEST_F(SchemaTest, GetTypeIdForDouble)
{
    EXPECT_EQ(getTypeId<double>(), FieldType::Double);
    EXPECT_EQ(getTypeId<float>(), FieldType::Double);
    EXPECT_EQ(getTypeId<long double>(), FieldType::Double);
}

TEST_F(SchemaTest, GetTypeIdForString)
{
    EXPECT_EQ(getTypeId<std::string>(), FieldType::String);
    EXPECT_EQ(getTypeId<const char*>(), FieldType::String);
    EXPECT_EQ(getTypeId<std::string_view>(), FieldType::String);
}

TEST_F(SchemaTest, FieldTypeToString)
{
    EXPECT_EQ(fieldTypeToString(FieldType::Int), "Int");
    EXPECT_EQ(fieldTypeToString(FieldType::Double), "Double");
    EXPECT_EQ(fieldTypeToString(FieldType::String), "String");
    EXPECT_EQ(fieldTypeToString(FieldType::Unknown), "Unknown");
}

TEST_F(SchemaTest, SchemaDefCreation)
{
    SchemaDef schema = {
        {"name", FieldType::String},
        {"age", FieldType::Int},
        {"score", FieldType::Double}
    };

    EXPECT_EQ(schema.size(), 3);
    EXPECT_EQ(schema["name"], FieldType::String);
    EXPECT_EQ(schema["age"], FieldType::Int);
    EXPECT_EQ(schema["score"], FieldType::Double);
}

TEST_F(SchemaTest, SchemaDefContainsCheck)
{
    SchemaDef schema = {
        {"name", FieldType::String},
        {"age", FieldType::Int}
    };

    EXPECT_TRUE(schema.contains("name"));
    EXPECT_TRUE(schema.contains("age"));
    EXPECT_FALSE(schema.contains("score"));
}

// ============================================================
// Constexpr 测试
// ============================================================

TEST_F(SchemaTest, ConstexprGetTypeId)
{
    constexpr FieldType intType = getTypeId<int>();
    EXPECT_EQ(intType, FieldType::Int);

    constexpr FieldType doubleType = getTypeId<double>();
    EXPECT_EQ(doubleType, FieldType::Double);

    constexpr FieldType stringType = getTypeId<std::string>();
    EXPECT_EQ(stringType, FieldType::String);
}

TEST_F(SchemaTest, ConstexprFieldTypeToString)
{
    constexpr std::string_view intStr = fieldTypeToString(FieldType::Int);
    EXPECT_EQ(intStr, "Int");
}
ng