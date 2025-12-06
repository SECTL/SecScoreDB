/**
 * @file websocket_e2e_test.cpp
 * @brief WebSocket 端到端集成测试（使用 GoogleTest）
 *
 * 这些测试需要 WebSocket 服务器正在运行。
 * 测试会建立实际的 WebSocket 连接并测试各个 API。
 *
 * 运行方式：
 *   1. 启动服务器：./SecScoreDB-Websockets --port 8765 --db ./testdata_e2e
 *   2. 运行测试：./SecScoreDB_UnitTests --gtest_filter="WebSocketE2E*"
 *
 * 或者使用 --gtest_also_run_disabled_tests 运行这些默认禁用的测试。
 */
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace std::chrono_literals;

namespace
{
    // 测试配置
    constexpr const char* WS_URL = "ws://127.0.0.1:8765";
    constexpr int TIMEOUT_MS = 5000;

    /**
     * @brief 响应等待器
     */
    class ResponseWaiter
    {
    public:
        void setResponse(const json& resp)
        {
            std::lock_guard lock(mutex_);
            response_ = resp;
            received_ = true;
            cv_.notify_all();
        }

        [[nodiscard]] json waitForResponse(int timeoutMs = TIMEOUT_MS)
        {
            std::unique_lock lock(mutex_);
            if (cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                            [this] { return received_; }))
            {
                received_ = false;
                return response_;
            }
            throw std::runtime_error("Timeout waiting for response");
        }

        void reset()
        {
            std::lock_guard lock(mutex_);
            received_ = false;
            response_ = json();
        }

    private:
        std::mutex mutex_;
        std::condition_variable cv_;
        json response_;
        bool received_ = false;
    };

    /**
     * @brief WebSocket 测试客户端
     */
    class WebSocketClient
    {
    public:
        WebSocketClient()
        {
            ix::initNetSystem();
        }

        ~WebSocketClient()
        {
            disconnect();
            ix::uninitNetSystem();
        }

        // 禁用拷贝
        WebSocketClient(const WebSocketClient&) = delete;
        WebSocketClient& operator=(const WebSocketClient&) = delete;

        bool connect(const std::string& url = WS_URL)
        {
            ws_.setUrl(url);

            ws_.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
                switch (msg->type)
                {
                    case ix::WebSocketMessageType::Message:
                        try
                        {
                            auto resp = json::parse(msg->str);
                            waiter_.setResponse(resp);
                        }
                        catch (const std::exception& e)
                        {
                            std::cerr << "[Client] JSON parse error: " << e.what() << '\n';
                        }
                        break;

                    case ix::WebSocketMessageType::Open:
                        connected_ = true;
                        break;

                    case ix::WebSocketMessageType::Close:
                        connected_ = false;
                        break;

                    case ix::WebSocketMessageType::Error:
                        std::cerr << "[Client] Error: " << msg->errorInfo.reason << '\n';
                        break;

                    default:
                        break;
                }
            });

            ws_.start();

            // 等待连接
            for (int i = 0; i < 50 && !connected_; ++i)
            {
                std::this_thread::sleep_for(100ms);
            }

            return connected_;
        }

        void disconnect()
        {
            ws_.stop();
            connected_ = false;
        }

        [[nodiscard]] json sendRequest(const std::string& category,
                                        const std::string& action,
                                        const json& payload)
        {
            std::string seq = "test-" + std::to_string(++seqCounter_);
            json request = {
                {"seq", seq},
                {"category", category},
                {"action", action},
                {"payload", payload}
            };

            waiter_.reset();
            ws_.send(request.dump());

            auto response = waiter_.waitForResponse();

            // 验证 seq 匹配
            if (response.contains("seq") && response["seq"] != seq)
            {
                throw std::runtime_error("Response seq mismatch");
            }

            return response;
        }

        [[nodiscard]] bool isConnected() const noexcept { return connected_; }

    private:
        ix::WebSocket ws_;
        ResponseWaiter waiter_;
        std::atomic<bool> connected_{false};
        std::atomic<int> seqCounter_{0};
    };

} // anonymous namespace

// ============================================================
// WebSocket 端到端测试
// 注意：这些测试默认禁用，需要服务器运行时才能测试
// ============================================================

class WebSocketE2ETest : public ::testing::Test
{
protected:
    std::unique_ptr<WebSocketClient> client;
    bool serverAvailable = false;

    void SetUp() override
    {
        client = std::make_unique<WebSocketClient>();
        serverAvailable = client->connect();

        if (!serverAvailable)
        {
            GTEST_SKIP() << "WebSocket server not available at " << WS_URL;
        }
    }

    void TearDown() override
    {
        if (client)
        {
            client->disconnect();
        }
    }

    // 辅助方法：登录
    json login(const std::string& username = "root", const std::string& password = "root")
    {
        return client->sendRequest("user", "login", {
            {"username", username},
            {"password", password}
        });
    }

    // 辅助方法：定义 Schema
    json defineStudentSchema()
    {
        return client->sendRequest("system", "define", {
            {"target", "student"},
            {"schema", {
                {"name", "string"},
                {"age", "int"},
                {"score", "double"}
            }}
        });
    }

    json defineGroupSchema()
    {
        return client->sendRequest("system", "define", {
            {"target", "group"},
            {"schema", {
                {"title", "string"},
                {"level", "int"}
            }}
        });
    }
};

// ============================================================
// 连接测试
// ============================================================

TEST_F(WebSocketE2ETest, ConnectionEstablished)
{
    EXPECT_TRUE(client->isConnected());
}

// ============================================================
// 系统 API 测试
// ============================================================

TEST_F(WebSocketE2ETest, SystemDefineStudentSchema)
{
    auto resp = defineStudentSchema();

    EXPECT_EQ(resp["status"], "ok");
    EXPECT_EQ(resp["code"], 200);
    EXPECT_EQ(resp["data"]["target"], "student");
    EXPECT_EQ(resp["data"]["fields"], 3);
}

TEST_F(WebSocketE2ETest, SystemDefineGroupSchema)
{
    auto resp = defineGroupSchema();

    EXPECT_EQ(resp["status"], "ok");
    EXPECT_EQ(resp["code"], 200);
    EXPECT_EQ(resp["data"]["target"], "group");
    EXPECT_EQ(resp["data"]["fields"], 2);
}

TEST_F(WebSocketE2ETest, SystemCommit)
{
    auto resp = client->sendRequest("system", "commit", json::object());

    EXPECT_EQ(resp["status"], "ok");
    EXPECT_TRUE(resp["data"]["committed"].get<bool>());
}

TEST_F(WebSocketE2ETest, SystemInvalidAction)
{
    auto resp = client->sendRequest("system", "invalid_action", json::object());

    EXPECT_EQ(resp["status"], "error");
    EXPECT_EQ(resp["code"], 400);
}

// ============================================================
// 用户认证 API 测试
// ============================================================

TEST_F(WebSocketE2ETest, UserLoginSuccess)
{
    auto resp = login();

    EXPECT_EQ(resp["status"], "ok");
    EXPECT_TRUE(resp["data"]["success"].get<bool>());
    EXPECT_EQ(resp["data"]["user"]["username"], "root");
    EXPECT_EQ(resp["data"]["user"]["permission"], "root");
}

TEST_F(WebSocketE2ETest, UserLoginWrongPassword)
{
    auto resp = login("root", "wrongpassword");

    EXPECT_EQ(resp["status"], "error");
    EXPECT_EQ(resp["code"], 401);
}

TEST_F(WebSocketE2ETest, UserLoginNonexistentUser)
{
    auto resp = login("nonexistent", "password");

    EXPECT_EQ(resp["status"], "error");
    EXPECT_EQ(resp["code"], 401);
}

TEST_F(WebSocketE2ETest, UserCurrentNotLoggedIn)
{
    auto resp = client->sendRequest("user", "current", json::object());

    EXPECT_EQ(resp["status"], "ok");
    EXPECT_FALSE(resp["data"]["logged_in"].get<bool>());
}

TEST_F(WebSocketE2ETest, UserCurrentLoggedIn)
{
    login();

    auto resp = client->sendRequest("user", "current", json::object());

    EXPECT_EQ(resp["status"], "ok");
    EXPECT_TRUE(resp["data"]["logged_in"].get<bool>());
    EXPECT_EQ(resp["data"]["user"]["username"], "root");
}

TEST_F(WebSocketE2ETest, UserLogout)
{
    login();

    auto resp = client->sendRequest("user", "logout", json::object());

    EXPECT_EQ(resp["status"], "ok");
    EXPECT_TRUE(resp["data"]["success"].get<bool>());

    // 验证已登出
    auto currentResp = client->sendRequest("user", "current", json::object());
    EXPECT_FALSE(currentResp["data"]["logged_in"].get<bool>());
}

TEST_F(WebSocketE2ETest, UserList)
{
    login();

    auto resp = client->sendRequest("user", "query", json::object());

    EXPECT_EQ(resp["status"], "ok");
    EXPECT_TRUE(resp["data"]["users"].is_array());
    EXPECT_GE(resp["data"]["users"].size(), 1);  // 至少有 root 用户
}

// ============================================================
// 学生 CRUD API 测试
// ============================================================

TEST_F(WebSocketE2ETest, StudentCreate)
{
    defineStudentSchema();

    json payload = {
        {"items", json::array({
            {{"index", 0}, {"id", nullptr}, {"data", {{"name", "Alice"}, {"age", 20}, {"score", 85.5}}}},
            {{"index", 1}, {"id", nullptr}, {"data", {{"name", "Bob"}, {"age", 21}, {"score", 90.0}}}}
        })}
    };

    auto resp = client->sendRequest("student", "create", payload);

    EXPECT_EQ(resp["status"], "ok");
    EXPECT_EQ(resp["data"]["count"], 2);
    EXPECT_EQ(resp["data"]["results"].size(), 2);

    for (const auto& result : resp["data"]["results"])
    {
        EXPECT_TRUE(result["success"].get<bool>());
        EXPECT_GT(result["id"].get<int>(), 0);
    }
}

TEST_F(WebSocketE2ETest, StudentQuery)
{
    defineStudentSchema();

    // 创建测试数据
    client->sendRequest("student", "create", {
        {"items", json::array({
            {{"index", 0}, {"id", 2001}, {"data", {{"name", "Test1"}, {"age", 18}, {"score", 70.0}}}},
            {{"index", 1}, {"id", 2002}, {"data", {{"name", "Test2"}, {"age", 22}, {"score", 95.0}}}}
        })}
    });

    // 查询所有
    auto resp = client->sendRequest("student", "query", json::object());

    EXPECT_EQ(resp["status"], "ok");
    EXPECT_GE(resp["data"]["items"].size(), 2);
}

TEST_F(WebSocketE2ETest, StudentQueryWithCondition)
{
    defineStudentSchema();

    // 创建测试数据
    client->sendRequest("student", "create", {
        {"items", json::array({
            {{"index", 0}, {"id", 3001}, {"data", {{"name", "Young"}, {"age", 18}, {"score", 70.0}}}},
            {{"index", 1}, {"id", 3002}, {"data", {{"name", "Old"}, {"age", 30}, {"score", 95.0}}}}
        })}
    });

    // 条件查询：年龄 >= 25
    auto resp = client->sendRequest("student", "query", {
        {"logic", {{"field", "age"}, {"op", ">="}, {"val", 25}}}
    });

    EXPECT_EQ(resp["status"], "ok");

    // 验证结果都满足条件
    for (const auto& item : resp["data"]["items"])
    {
        EXPECT_GE(item["data"]["age"].get<int>(), 25);
    }
}

TEST_F(WebSocketE2ETest, StudentUpdate)
{
    defineStudentSchema();

    // 创建测试数据
    client->sendRequest("student", "create", {
        {"items", json::array({
            {{"index", 0}, {"id", 4001}, {"data", {{"name", "UpdateMe"}, {"age", 20}, {"score", 80.0}}}}
        })}
    });

    // 更新
    auto resp = client->sendRequest("student", "update", {
        {"id", 4001},
        {"set", {{"age", 25}, {"score", 95.0}}}
    });

    EXPECT_EQ(resp["status"], "ok");
    EXPECT_TRUE(resp["data"]["updated"].get<bool>());

    // 验证更新
    auto queryResp = client->sendRequest("student", "query", {
        {"logic", {{"field", "age"}, {"op", "=="}, {"val", 25}}}
    });

    bool found = false;
    for (const auto& item : queryResp["data"]["items"])
    {
        if (item["id"] == 4001)
        {
            found = true;
            EXPECT_EQ(item["data"]["age"], 25);
            EXPECT_EQ(item["data"]["score"], 95.0);
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(WebSocketE2ETest, StudentDelete)
{
    defineStudentSchema();

    // 创建测试数据
    client->sendRequest("student", "create", {
        {"items", json::array({
            {{"index", 0}, {"id", 5001}, {"data", {{"name", "DeleteMe"}, {"age", 20}, {"score", 80.0}}}}
        })}
    });

    // 删除
    auto resp = client->sendRequest("student", "delete", {{"id", 5001}});

    EXPECT_EQ(resp["status"], "ok");
    EXPECT_TRUE(resp["data"]["deleted"].get<bool>());

    // 验证删除（查询应该找不到）
    auto queryResp = client->sendRequest("student", "query", {
        {"logic", {{"field", "age"}, {"op", "=="}, {"val", 20}}}
    });

    for (const auto& item : queryResp["data"]["items"])
    {
        EXPECT_NE(item["id"], 5001);
    }
}

// ============================================================
// 分组 CRUD API 测试
// ============================================================

TEST_F(WebSocketE2ETest, GroupCreate)
{
    defineGroupSchema();

    json payload = {
        {"items", json::array({
            {{"index", 0}, {"id", nullptr}, {"data", {{"title", "Class A"}, {"level", 1}}}},
            {{"index", 1}, {"id", nullptr}, {"data", {{"title", "Class B"}, {"level", 2}}}}
        })}
    };

    auto resp = client->sendRequest("group", "create", payload);

    EXPECT_EQ(resp["status"], "ok");
    EXPECT_EQ(resp["data"]["count"], 2);
}

TEST_F(WebSocketE2ETest, GroupQuery)
{
    defineGroupSchema();

    // 创建测试数据
    client->sendRequest("group", "create", {
        {"items", json::array({
            {{"index", 0}, {"id", 6001}, {"data", {{"title", "Test Group"}, {"level", 3}}}}
        })}
    });

    auto resp = client->sendRequest("group", "query", json::object());

    EXPECT_EQ(resp["status"], "ok");
    EXPECT_GE(resp["data"]["items"].size(), 1);
}

// ============================================================
// 事件 API 测试
// ============================================================

TEST_F(WebSocketE2ETest, EventCreate)
{
    json payload = {
        {"id", nullptr},
        {"type", 1},  // Student
        {"ref_id", 1001},
        {"desc", "E2E test event"},
        {"val_prev", 80.0},
        {"val_curr", 90.0}
    };

    auto resp = client->sendRequest("event", "create", payload);

    EXPECT_EQ(resp["status"], "ok");
    EXPECT_GT(resp["data"]["id"].get<int>(), 0);
    EXPECT_TRUE(resp["data"].contains("timestamp"));
}

TEST_F(WebSocketE2ETest, EventUpdate)
{
    // 创建事件
    auto createResp = client->sendRequest("event", "create", {
        {"id", nullptr},
        {"type", 1},
        {"ref_id", 1001},
        {"desc", "To be erased"},
        {"val_prev", 0},
        {"val_curr", 10}
    });

    int eventId = createResp["data"]["id"].get<int>();

    // 标记为已擦除
    auto updateResp = client->sendRequest("event", "update", {
        {"id", eventId},
        {"erased", true}
    });

    EXPECT_EQ(updateResp["status"], "ok");
    EXPECT_TRUE(updateResp["data"]["erased"].get<bool>());
}

// ============================================================
// 错误处理测试
// ============================================================

TEST_F(WebSocketE2ETest, InvalidCategory)
{
    auto resp = client->sendRequest("invalid_category", "action", json::object());

    EXPECT_EQ(resp["status"], "error");
    EXPECT_EQ(resp["code"], 400);
}

TEST_F(WebSocketE2ETest, StudentCreateWithoutLogin)
{
    // 未登录时尝试创建学生，应该被拒绝
    defineStudentSchema();

    json payload = {
        {"items", json::array({
            {{"index", 0}, {"id", nullptr}, {"data", {{"name", "Test"}, {"age", 20}, {"score", 80.0}}}}
        })}
    };

    auto resp = client->sendRequest("student", "create", payload);

    EXPECT_EQ(resp["status"], "error");
    EXPECT_EQ(resp["code"], 401);
}

TEST_F(WebSocketE2ETest, EventCreateWithoutLogin)
{
    // 未登录时尝试创建事件，应该被拒绝
    json payload = {
        {"id", nullptr},
        {"type", 1},
        {"ref_id", 1001},
        {"desc", "Test"},
        {"val_prev", 0},
        {"val_curr", 10}
    };

    auto resp = client->sendRequest("event", "create", payload);

    EXPECT_EQ(resp["status"], "error");
    EXPECT_EQ(resp["code"], 401);
}

TEST_F(WebSocketE2ETest, CommitWithoutLogin)
{
    // 未登录时尝试 commit，应该被拒绝
    auto resp = client->sendRequest("system", "commit", json::object());

    EXPECT_EQ(resp["status"], "error");
    EXPECT_EQ(resp["code"], 401);
}

TEST_F(WebSocketE2ETest, DefineSchemaWithoutLogin)
{
    // define 操作不需要登录，应该成功
    auto resp = defineStudentSchema();

    EXPECT_EQ(resp["status"], "ok");
    EXPECT_EQ(resp["code"], 200);
}

TEST_F(WebSocketE2ETest, InvalidFieldType)
{
    login();  // 需要先登录
    defineStudentSchema();

    // 尝试用错误类型创建
    json payload = {
        {"items", json::array({
            {{"index", 0}, {"id", nullptr}, {"data", {{"name", 123}, {"age", "not_a_number"}, {"score", "invalid"}}}}
        })}
    };

    auto resp = client->sendRequest("student", "create", payload);

    EXPECT_EQ(resp["status"], "ok");
    EXPECT_EQ(resp["data"]["count"], 0);  // 创建失败
    EXPECT_FALSE(resp["data"]["results"][0]["success"].get<bool>());
}

// ============================================================
// 复杂逻辑查询测试
// ============================================================

TEST_F(WebSocketE2ETest, ComplexLogicQuery)
{
    defineStudentSchema();

    // 创建测试数据
    client->sendRequest("student", "create", {
        {"items", json::array({
            {{"index", 0}, {"id", 7001}, {"data", {{"name", "Alice"}, {"age", 20}, {"score", 85.0}}}},
            {{"index", 1}, {"id", 7002}, {"data", {{"name", "Bob"}, {"age", 25}, {"score", 75.0}}}},
            {{"index", 2}, {"id", 7003}, {"data", {{"name", "Charlie"}, {"age", 22}, {"score", 90.0}}}},
            {{"index", 3}, {"id", 7004}, {"data", {{"name", "Diana"}, {"age", 30}, {"score", 95.0}}}}
        })}
    });

    // 复杂查询：(age >= 22 AND score >= 85) OR age >= 30
    json complexQuery = {
        {"logic", {
            {"op", "OR"},
            {"rules", json::array({
                {
                    {"op", "AND"},
                    {"rules", json::array({
                        {{"field", "age"}, {"op", ">="}, {"val", 22}},
                        {{"field", "score"}, {"op", ">="}, {"val", 85}}
                    })}
                },
                {{"field", "age"}, {"op", ">="}, {"val", 30}}
            })}
        }}
    };

    auto resp = client->sendRequest("student", "query", complexQuery);

    EXPECT_EQ(resp["status"], "ok");
    // 应该匹配：Charlie (22, 90), Diana (30, 95)
    EXPECT_GE(resp["data"]["items"].size(), 2);
}

TEST_F(WebSocketE2ETest, StringContainsQuery)
{
    defineStudentSchema();

    // 创建测试数据
    client->sendRequest("student", "create", {
        {"items", json::array({
            {{"index", 0}, {"id", 8001}, {"data", {{"name", "Alice Smith"}, {"age", 20}, {"score", 85.0}}}},
            {{"index", 1}, {"id", 8002}, {"data", {{"name", "Bob Johnson"}, {"age", 25}, {"score", 75.0}}}},
            {{"index", 2}, {"id", 8003}, {"data", {{"name", "Charlie Smith"}, {"age", 22}, {"score", 90.0}}}}
        })}
    });

    // 查询名字包含 "Smith" 的学生
    auto resp = client->sendRequest("student", "query", {
        {"logic", {{"field", "name"}, {"op", "contains"}, {"val", "Smith"}}}
    });

    EXPECT_EQ(resp["status"], "ok");
    EXPECT_GE(resp["data"]["items"].size(), 2);

    for (const auto& item : resp["data"]["items"])
    {
        std::string name = item["data"]["name"].get<std::string>();
        EXPECT_NE(name.find("Smith"), std::string::npos);
    }
}

// ============================================================
// 并发请求测试
// ============================================================

TEST_F(WebSocketE2ETest, RapidRequests)
{
    login();  // 需要先登录
    defineStudentSchema();

    // 快速发送多个请求（使用 auto-allocate ID 避免冲突）
    std::vector<json> responses;

    for (int i = 0; i < 10; ++i)
    {
        auto resp = client->sendRequest("student", "create", {
            {"items", json::array({
                {{"index", 0}, {"id", nullptr}, {"data", {{"name", "Rapid" + std::to_string(i)}, {"age", 20 + i}, {"score", 80.0 + i}}}}
            })}
        });
        responses.push_back(resp);
    }

    // 验证所有请求都成功
    int successCount = 0;
    for (const auto& resp : responses)
    {
        EXPECT_EQ(resp["status"], "ok");
        if (resp["data"]["count"].get<int>() == 1)
        {
            ++successCount;
        }
    }

    // 至少应该有大部分成功
    EXPECT_GE(successCount, 8) << "At least 8 out of 10 rapid requests should succeed";
}

