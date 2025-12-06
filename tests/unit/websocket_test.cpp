/**
 * @file websocket_test.cpp
 * @brief WebSocket 服务器单元测试（使用 GoogleTest）
 *
 * 测试 WebSocket 协议层和处理器的功能。
 * 这些测试不需要运行服务器，直接测试内部组件。
 */
#include <gtest/gtest.h>

#include "Errors.hpp"
#include "Handlers.hpp"
#include "JsonUtils.hpp"
#include "Protocol.hpp"
#include "SecScoreDB.h"

#include <filesystem>
#include <mutex>

using json = nlohmann::json;

// ============================================================
// JsonUtils 测试
// ============================================================

class JsonUtilsTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(JsonUtilsTest, ToLowerCopy)
{
    EXPECT_EQ(ws::toLowerCopy("HELLO"), "hello");
    EXPECT_EQ(ws::toLowerCopy("Hello World"), "hello world");
    EXPECT_EQ(ws::toLowerCopy("already lower"), "already lower");
    EXPECT_EQ(ws::toLowerCopy(""), "");
    EXPECT_EQ(ws::toLowerCopy("MiXeD CaSe 123"), "mixed case 123");
}

TEST_F(JsonUtilsTest, ToUpperCopy)
{
    EXPECT_EQ(ws::toUpperCopy("hello"), "HELLO");
    EXPECT_EQ(ws::toUpperCopy("Hello World"), "HELLO WORLD");
    EXPECT_EQ(ws::toUpperCopy("ALREADY UPPER"), "ALREADY UPPER");
    EXPECT_EQ(ws::toUpperCopy(""), "");
}

TEST_F(JsonUtilsTest, ParseFieldType)
{
    EXPECT_EQ(ws::parseFieldType("string"), SSDB::FieldType::String);
    EXPECT_EQ(ws::parseFieldType("String"), SSDB::FieldType::String);
    EXPECT_EQ(ws::parseFieldType("STRING"), SSDB::FieldType::String);
    EXPECT_EQ(ws::parseFieldType("int"), SSDB::FieldType::Int);
    EXPECT_EQ(ws::parseFieldType("INT"), SSDB::FieldType::Int);
    EXPECT_EQ(ws::parseFieldType("double"), SSDB::FieldType::Double);
    EXPECT_EQ(ws::parseFieldType("Double"), SSDB::FieldType::Double);
}

TEST_F(JsonUtilsTest, ParseFieldTypeInvalid)
{
    EXPECT_THROW(ws::parseFieldType("invalid"), ws::ApiError);
    EXPECT_THROW(ws::parseFieldType("boolean"), ws::ApiError);
}

TEST_F(JsonUtilsTest, ParseSchema)
{
    json schemaJson = {
        {"name", "string"},
        {"age", "int"},
        {"score", "double"}
    };

    auto schema = ws::parseSchema(schemaJson);

    EXPECT_EQ(schema.size(), 3);
    EXPECT_EQ(schema["name"], SSDB::FieldType::String);
    EXPECT_EQ(schema["age"], SSDB::FieldType::Int);
    EXPECT_EQ(schema["score"], SSDB::FieldType::Double);
}

TEST_F(JsonUtilsTest, ParseSchemaEmpty)
{
    EXPECT_THROW(ws::parseSchema(json::object()), ws::ApiError);
    EXPECT_THROW(ws::parseSchema(json::array()), ws::ApiError);
}

TEST_F(JsonUtilsTest, EnsureSchemaReady)
{
    SSDB::SchemaDef emptySchema;
    SSDB::SchemaDef validSchema = {{"name", SSDB::FieldType::String}};

    EXPECT_THROW(ws::ensureSchemaReady(emptySchema, "student"), ws::ApiError);
    EXPECT_NO_THROW(ws::ensureSchemaReady(validSchema, "student"));
}

TEST_F(JsonUtilsTest, RequireNumber)
{
    EXPECT_DOUBLE_EQ(ws::requireNumber(json(42), "test"), 42.0);
    EXPECT_DOUBLE_EQ(ws::requireNumber(json(3.14), "test"), 3.14);
    EXPECT_DOUBLE_EQ(ws::requireNumber(json(-100), "test"), -100.0);

    EXPECT_THROW(ws::requireNumber(json("string"), "test"), ws::ApiError);
    EXPECT_THROW(ws::requireNumber(json(true), "test"), ws::ApiError);
    EXPECT_THROW(ws::requireNumber(json::object(), "test"), ws::ApiError);
}

TEST_F(JsonUtilsTest, CompareNumbers)
{
    EXPECT_TRUE(ws::compareNumbers(5.0, 5.0, "=="));
    EXPECT_FALSE(ws::compareNumbers(5.0, 3.0, "=="));

    EXPECT_TRUE(ws::compareNumbers(5.0, 3.0, "!="));
    EXPECT_FALSE(ws::compareNumbers(5.0, 5.0, "!="));

    EXPECT_TRUE(ws::compareNumbers(5.0, 3.0, ">"));
    EXPECT_FALSE(ws::compareNumbers(3.0, 5.0, ">"));

    EXPECT_TRUE(ws::compareNumbers(5.0, 5.0, ">="));
    EXPECT_TRUE(ws::compareNumbers(5.0, 3.0, ">="));

    EXPECT_TRUE(ws::compareNumbers(3.0, 5.0, "<"));
    EXPECT_FALSE(ws::compareNumbers(5.0, 3.0, "<"));

    EXPECT_TRUE(ws::compareNumbers(5.0, 5.0, "<="));
    EXPECT_TRUE(ws::compareNumbers(3.0, 5.0, "<="));
}

TEST_F(JsonUtilsTest, CompareStrings)
{
    EXPECT_TRUE(ws::compareStrings("hello", "hello", "=="));
    EXPECT_FALSE(ws::compareStrings("hello", "world", "=="));

    EXPECT_TRUE(ws::compareStrings("hello", "world", "!="));
    EXPECT_FALSE(ws::compareStrings("hello", "hello", "!="));

    EXPECT_TRUE(ws::compareStrings("hello world", "world", "contains"));
    EXPECT_FALSE(ws::compareStrings("hello", "world", "contains"));

    EXPECT_TRUE(ws::compareStrings("hello world", "hello", "starts_with"));
    EXPECT_FALSE(ws::compareStrings("hello world", "world", "starts_with"));

    EXPECT_TRUE(ws::compareStrings("hello world", "world", "ends_with"));
    EXPECT_FALSE(ws::compareStrings("hello world", "hello", "ends_with"));
}

TEST_F(JsonUtilsTest, DecodeStoredValue)
{
    // String
    auto strVal = ws::decodeStoredValue("hello", SSDB::FieldType::String);
    ASSERT_TRUE(strVal.has_value());
    EXPECT_EQ(strVal->get<std::string>(), "hello");

    // Int
    auto intVal = ws::decodeStoredValue("42", SSDB::FieldType::Int);
    ASSERT_TRUE(intVal.has_value());
    EXPECT_EQ(intVal->get<long long>(), 42);

    // Double
    auto dblVal = ws::decodeStoredValue("3.14", SSDB::FieldType::Double);
    ASSERT_TRUE(dblVal.has_value());
    EXPECT_DOUBLE_EQ(dblVal->get<double>(), 3.14);

    // Invalid
    auto invalid = ws::decodeStoredValue("not_a_number", SSDB::FieldType::Int);
    EXPECT_FALSE(invalid.has_value());
}

// ============================================================
// ApiError 测试
// ============================================================

class ApiErrorTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ApiErrorTest, Construction)
{
    ws::ApiError error(404, "Not found");
    EXPECT_EQ(error.code, 404);
    EXPECT_EQ(error.getCode(), 404);
    EXPECT_STREQ(error.what(), "Not found");
}

TEST_F(ApiErrorTest, ThrowAndCatch)
{
    try
    {
        throw ws::ApiError(400, "Bad request");
    }
    catch (const ws::ApiError& e)
    {
        EXPECT_EQ(e.code, 400);
        EXPECT_STREQ(e.what(), "Bad request");
    }
}

// ============================================================
// Protocol 响应构造测试
// ============================================================

class ProtocolTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ProtocolTest, MakeOkResponse)
{
    json data = {{"id", 123}, {"name", "test"}};
    auto response = ws::makeOkResponse("seq-001", data);

    EXPECT_EQ(response["seq"], "seq-001");
    EXPECT_EQ(response["status"], "ok");
    EXPECT_EQ(response["code"], 200);
    EXPECT_EQ(response["data"]["id"], 123);
    EXPECT_EQ(response["data"]["name"], "test");
}

TEST_F(ProtocolTest, MakeOkResponseEmpty)
{
    auto response = ws::makeOkResponse("seq-002");

    EXPECT_EQ(response["seq"], "seq-002");
    EXPECT_EQ(response["status"], "ok");
    EXPECT_EQ(response["code"], 200);
    // 默认参数是 json::object()，所以 data 会存在但为空对象
    EXPECT_TRUE(response.contains("data"));
    EXPECT_TRUE(response["data"].is_object());
    EXPECT_TRUE(response["data"].empty());
}

TEST_F(ProtocolTest, MakeOkResponseNull)
{
    // 显式传入 null 时不应该有 data 键
    auto response = ws::makeOkResponse("seq-002b", json());

    EXPECT_EQ(response["seq"], "seq-002b");
    EXPECT_EQ(response["status"], "ok");
    EXPECT_EQ(response["code"], 200);
    EXPECT_FALSE(response.contains("data"));
}

TEST_F(ProtocolTest, MakeErrorResponse)
{
    auto response = ws::makeErrorResponse("seq-003", 404, "Not found");

    EXPECT_EQ(response["seq"], "seq-003");
    EXPECT_EQ(response["status"], "error");
    EXPECT_EQ(response["code"], 404);
    EXPECT_EQ(response["message"], "Not found");
}

// ============================================================
// RequestContext 测试
// ============================================================

class RequestContextTest : public ::testing::Test
{
protected:
    std::filesystem::path testDbPath{"./test_ws_context"};
    std::mutex dbMutex;

    void SetUp() override
    {
        std::error_code ec;
        std::filesystem::remove_all(testDbPath, ec);
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(testDbPath, ec);
    }
};

TEST_F(RequestContextTest, InitialState)
{
    SSDB::SecScoreDB db(testDbPath);
    ws::RequestContext ctx{db, dbMutex, std::nullopt};

    EXPECT_FALSE(ctx.isLoggedIn());
    EXPECT_FALSE(ctx.currentUserId.has_value());
}

TEST_F(RequestContextTest, LoginLogout)
{
    SSDB::SecScoreDB db(testDbPath);
    ws::RequestContext ctx{db, dbMutex, std::nullopt};

    ctx.login(42);
    EXPECT_TRUE(ctx.isLoggedIn());
    EXPECT_EQ(ctx.currentUserId.value(), 42);

    ctx.logout();
    EXPECT_FALSE(ctx.isLoggedIn());
}

// ============================================================
// Logic 评估测试
// ============================================================

class LogicEvaluationTest : public ::testing::Test
{
protected:
    SSDB::SchemaDef schema;

    void SetUp() override
    {
        schema = {
            {"name", SSDB::FieldType::String},
            {"age", SSDB::FieldType::Int},
            {"score", SSDB::FieldType::Double}
        };
    }
};

TEST_F(LogicEvaluationTest, SimpleFieldComparison)
{
    json entityData = {{"name", "Alice"}, {"age", 25}, {"score", 85.5}};

    // 字符串比较
    json nameRule = {{"field", "name"}, {"op", "=="}, {"val", "Alice"}};
    EXPECT_TRUE(ws::evaluateLogicNode(entityData, nameRule, schema));

    json nameRuleNe = {{"field", "name"}, {"op", "!="}, {"val", "Bob"}};
    EXPECT_TRUE(ws::evaluateLogicNode(entityData, nameRuleNe, schema));

    // 整数比较
    json ageRule = {{"field", "age"}, {"op", ">="}, {"val", 18}};
    EXPECT_TRUE(ws::evaluateLogicNode(entityData, ageRule, schema));

    json ageRuleLt = {{"field", "age"}, {"op", "<"}, {"val", 20}};
    EXPECT_FALSE(ws::evaluateLogicNode(entityData, ageRuleLt, schema));

    // 浮点比较
    json scoreRule = {{"field", "score"}, {"op", ">"}, {"val", 80.0}};
    EXPECT_TRUE(ws::evaluateLogicNode(entityData, scoreRule, schema));
}

TEST_F(LogicEvaluationTest, AndLogic)
{
    json entityData = {{"name", "Alice"}, {"age", 25}, {"score", 85.5}};

    json andRule = {
        {"op", "AND"},
        {"rules", json::array({
            {{"field", "age"}, {"op", ">="}, {"val", 18}},
            {{"field", "score"}, {"op", ">"}, {"val", 80}}
        })}
    };

    EXPECT_TRUE(ws::evaluateLogicNode(entityData, andRule, schema));

    // 添加一个失败的条件
    json andRuleFail = {
        {"op", "AND"},
        {"rules", json::array({
            {{"field", "age"}, {"op", ">="}, {"val", 18}},
            {{"field", "score"}, {"op", ">"}, {"val", 90}}  // 这个失败
        })}
    };

    EXPECT_FALSE(ws::evaluateLogicNode(entityData, andRuleFail, schema));
}

TEST_F(LogicEvaluationTest, OrLogic)
{
    json entityData = {{"name", "Alice"}, {"age", 25}, {"score", 85.5}};

    json orRule = {
        {"op", "OR"},
        {"rules", json::array({
            {{"field", "age"}, {"op", "<"}, {"val", 18}},  // 失败
            {{"field", "score"}, {"op", ">"}, {"val", 80}}  // 成功
        })}
    };

    EXPECT_TRUE(ws::evaluateLogicNode(entityData, orRule, schema));

    // 全部失败
    json orRuleFail = {
        {"op", "OR"},
        {"rules", json::array({
            {{"field", "age"}, {"op", "<"}, {"val", 18}},
            {{"field", "score"}, {"op", ">"}, {"val", 90}}
        })}
    };

    EXPECT_FALSE(ws::evaluateLogicNode(entityData, orRuleFail, schema));
}

TEST_F(LogicEvaluationTest, StringOperators)
{
    json entityData = {{"name", "Alice Johnson"}, {"age", 25}, {"score", 85.5}};

    json containsRule = {{"field", "name"}, {"op", "contains"}, {"val", "John"}};
    EXPECT_TRUE(ws::evaluateLogicNode(entityData, containsRule, schema));

    json startsWithRule = {{"field", "name"}, {"op", "starts_with"}, {"val", "Alice"}};
    EXPECT_TRUE(ws::evaluateLogicNode(entityData, startsWithRule, schema));

    json endsWithRule = {{"field", "name"}, {"op", "ends_with"}, {"val", "Johnson"}};
    EXPECT_TRUE(ws::evaluateLogicNode(entityData, endsWithRule, schema));
}

// ============================================================
// Handler 集成测试
// ============================================================

class HandlerIntegrationTest : public ::testing::Test
{
protected:
    std::filesystem::path testDbPath{"./test_ws_handler"};
    std::mutex dbMutex;
    std::unique_ptr<SSDB::SecScoreDB> db;

    void SetUp() override
    {
        std::error_code ec;
        std::filesystem::remove_all(testDbPath, ec);
        db = std::make_unique<SSDB::SecScoreDB>(testDbPath);
    }

    void TearDown() override
    {
        db.reset();
        std::error_code ec;
        std::filesystem::remove_all(testDbPath, ec);
    }
};

TEST_F(HandlerIntegrationTest, SystemDefineSchema)
{
    ws::RequestContext ctx{*db, dbMutex, std::nullopt};

    json payload = {
        {"target", "student"},
        {"schema", {
            {"name", "string"},
            {"age", "int"}
        }}
    };

    auto result = ws::handleSystem("define", payload, ctx);

    EXPECT_EQ(result["target"], "student");
    EXPECT_EQ(result["fields"], 2);
    EXPECT_EQ(db->studentSchema().size(), 2);
}

TEST_F(HandlerIntegrationTest, SystemCommit)
{
    ws::RequestContext ctx{*db, dbMutex, std::nullopt};

    auto result = ws::handleSystem("commit", json::object(), ctx);

    EXPECT_TRUE(result["committed"].get<bool>());
}

TEST_F(HandlerIntegrationTest, UserLogin)
{
    ws::RequestContext ctx{*db, dbMutex, std::nullopt};

    json payload = {
        {"username", "root"},
        {"password", "root"}
    };

    auto result = ws::handleUser("login", payload, ctx);

    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_EQ(result["user"]["username"], "root");
    EXPECT_TRUE(ctx.isLoggedIn());
}

TEST_F(HandlerIntegrationTest, UserLoginWrongPassword)
{
    ws::RequestContext ctx{*db, dbMutex, std::nullopt};

    json payload = {
        {"username", "root"},
        {"password", "wrong"}
    };

    EXPECT_THROW(ws::handleUser("login", payload, ctx), ws::ApiError);
    EXPECT_FALSE(ctx.isLoggedIn());
}

TEST_F(HandlerIntegrationTest, UserLogout)
{
    ws::RequestContext ctx{*db, dbMutex, 1};
    EXPECT_TRUE(ctx.isLoggedIn());

    auto result = ws::handleUser("logout", json::object(), ctx);

    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_FALSE(ctx.isLoggedIn());
}

TEST_F(HandlerIntegrationTest, UserCurrent)
{
    // 未登录
    ws::RequestContext ctx1{*db, dbMutex, std::nullopt};
    auto result1 = ws::handleUser("current", json::object(), ctx1);
    EXPECT_FALSE(result1["logged_in"].get<bool>());

    // 登录后
    ws::handleUser("login", {{"username", "root"}, {"password", "root"}}, ctx1);
    auto result2 = ws::handleUser("current", json::object(), ctx1);
    EXPECT_TRUE(result2["logged_in"].get<bool>());
    EXPECT_EQ(result2["user"]["username"], "root");
}

TEST_F(HandlerIntegrationTest, DispatchRouting)
{
    ws::RequestContext ctx{*db, dbMutex, std::nullopt};

    // 测试 dispatch 路由到正确的 handler
    auto result = ws::dispatch("SYSTEM", "commit", json::object(), ctx);
    EXPECT_TRUE(result.contains("committed"));

    // 测试大小写不敏感
    auto result2 = ws::dispatch("System", "COMMIT", json::object(), ctx);
    EXPECT_TRUE(result2.contains("committed"));
}

TEST_F(HandlerIntegrationTest, DispatchInvalidCategory)
{
    ws::RequestContext ctx{*db, dbMutex, std::nullopt};

    EXPECT_THROW(
        ws::dispatch("invalid", "action", json::object(), ctx),
        ws::ApiError
    );
}

TEST_F(HandlerIntegrationTest, StudentCRUD)
{
    ws::RequestContext ctx{*db, dbMutex, std::nullopt};

    // 定义 Schema
    ws::handleSystem("define", {
        {"target", "student"},
        {"schema", {{"name", "string"}, {"age", "int"}}}
    }, ctx);

    // 创建学生
    json createPayload = {
        {"items", json::array({
            {{"index", 0}, {"id", 1001}, {"data", {{"name", "Alice"}, {"age", 20}}}},
            {{"index", 1}, {"id", 1002}, {"data", {{"name", "Bob"}, {"age", 21}}}}
        })}
    };

    auto createResult = ws::handleStudent("create", createPayload, ctx);
    EXPECT_EQ(createResult["count"], 2);

    // 查询学生
    json queryPayload = json::object();
    auto queryResult = ws::handleStudent("query", queryPayload, ctx);
    EXPECT_EQ(queryResult["items"].size(), 2);

    // 条件查询
    json condQueryPayload = {
        {"logic", {{"field", "age"}, {"op", ">="}, {"val", 21}}}
    };
    auto condResult = ws::handleStudent("query", condQueryPayload, ctx);
    EXPECT_EQ(condResult["items"].size(), 1);

    // 更新学生
    json updatePayload = {
        {"id", 1001},
        {"set", {{"age", 22}}}
    };
    auto updateResult = ws::handleStudent("update", updatePayload, ctx);
    EXPECT_TRUE(updateResult["updated"].get<bool>());

    // 删除学生
    json deletePayload = {{"id", 1002}};
    auto deleteResult = ws::handleStudent("delete", deletePayload, ctx);
    EXPECT_TRUE(deleteResult["deleted"].get<bool>());

    // 验证删除
    auto finalQuery = ws::handleStudent("query", json::object(), ctx);
    EXPECT_EQ(finalQuery["items"].size(), 1);
}

TEST_F(HandlerIntegrationTest, EventCreateAndUpdate)
{
    ws::RequestContext ctx{*db, dbMutex, std::nullopt};

    // 创建事件
    json createPayload = {
        {"id", nullptr},
        {"type", 1},  // Student
        {"ref_id", 1001},
        {"desc", "Test event"},
        {"val_prev", 80.0},
        {"val_curr", 90.0}
    };

    auto createResult = ws::handleEvent("create", createPayload, ctx);
    EXPECT_GT(createResult["id"].get<int>(), 0);
    int eventId = createResult["id"].get<int>();

    // 更新事件（标记为已擦除）
    json updatePayload = {
        {"id", eventId},
        {"erased", true}
    };

    auto updateResult = ws::handleEvent("update", updatePayload, ctx);
    EXPECT_TRUE(updateResult["erased"].get<bool>());
}

