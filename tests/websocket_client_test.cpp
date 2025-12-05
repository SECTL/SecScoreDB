/**
 * @file websocket_client_test.cpp
 * @brief WebSocket 客户端测试程序
 *
 * 用于测试 SecScoreDB WebSocket 服务端的功能。
 * 确保在运行此测试前，WebSocket 服务端已经启动。
 *
 * 使用方法：
 *   1. 先启动服务端：./SecScoreDB-Websockets --port 8765 --db ./testdata_ws
 *   2. 再运行此测试：./SecScoreDBWebsocketTest
 */

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <cassert>
#include <functional>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace std::chrono_literals;

// 测试配置
static const std::string WS_URL = "ws://localhost:8765";
static std::atomic<int> testsPassed{0};
static std::atomic<int> testsFailed{0};

// 辅助函数：等待响应
class ResponseWaiter
{
public:
    void setResponse(const json& resp)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        response_ = resp;
        received_ = true;
        cv_.notify_all();
    }

    json waitForResponse(int timeoutMs = 5000)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this] { return received_; }))
        {
            received_ = false;
            return response_;
        }
        throw std::runtime_error("Timeout waiting for response");
    }

    void reset()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        received_ = false;
        response_ = json();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    json response_;
    bool received_ = false;
};

// 测试辅助类
class WebSocketTester
{
public:
    WebSocketTester() : connected_(false), seqCounter_(0)
    {
        ix::initNetSystem();
    }

    ~WebSocketTester()
    {
        disconnect();
        ix::uninitNetSystem();
    }

    bool connect(const std::string& url = WS_URL)
    {
        ws_.setUrl(url);

        ws_.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Message)
            {
                try
                {
                    json resp = json::parse(msg->str);
                    waiter_.setResponse(resp);
                }
                catch (const std::exception& e)
                {
                    std::cerr << "[ERROR] Failed to parse response: " << e.what() << std::endl;
                }
            }
            else if (msg->type == ix::WebSocketMessageType::Open)
            {
                connected_ = true;
                std::cout << "[INFO] WebSocket connected" << std::endl;
            }
            else if (msg->type == ix::WebSocketMessageType::Close)
            {
                connected_ = false;
                std::cout << "[INFO] WebSocket disconnected" << std::endl;
            }
            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                std::cerr << "[ERROR] " << msg->errorInfo.reason << std::endl;
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

    json sendRequest(const std::string& category, const std::string& action, const json& payload)
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

        return waiter_.waitForResponse();
    }

    bool isConnected() const { return connected_; }

private:
    ix::WebSocket ws_;
    ResponseWaiter waiter_;
    std::atomic<bool> connected_;
    std::atomic<int> seqCounter_;
};

// 测试宏
#define TEST(name) \
    void test_##name(WebSocketTester& tester); \
    struct TestRunner_##name { \
        TestRunner_##name() { \
            std::cout << "\n[TEST] " << #name << "..." << std::endl; \
            WebSocketTester tester; \
            if (!tester.connect()) { \
                std::cerr << "[FAIL] Cannot connect to server" << std::endl; \
                testsFailed++; \
                return; \
            } \
            try { \
                test_##name(tester); \
                std::cout << "[PASS] " << #name << std::endl; \
                testsPassed++; \
            } catch (const std::exception& e) { \
                std::cerr << "[FAIL] " << #name << ": " << e.what() << std::endl; \
                testsFailed++; \
            } \
        } \
    } testRunner_##name; \
    void test_##name(WebSocketTester& tester)

// ============================================================
// 测试用例
// ============================================================

// 测试1: 系统定义 Schema
TEST(DefineStudentSchema)
{
    json resp = tester.sendRequest("system", "define", {
        {"target", "student"},
        {"schema", {
            {"name", "string"},
            {"age", "int"},
            {"score", "double"}
        }}
    });

    assert(resp["status"] == "ok");
    assert(resp["code"] == 200);
    std::cout << "  Schema defined with " << resp["data"]["fields"] << " fields" << std::endl;
}

// 测试2: 用户登录
TEST(UserLogin)
{
    json resp = tester.sendRequest("user", "login", {
        {"username", "root"},
        {"password", "root"}
    });

    assert(resp["status"] == "ok");
    assert(resp["code"] == 200);
    assert(resp["data"]["success"] == true);
    std::cout << "  Logged in as: " << resp["data"]["user"]["username"] << std::endl;
}

// 测试3: 错误密码登录
TEST(UserLoginWrongPassword)
{
    json resp = tester.sendRequest("user", "login", {
        {"username", "root"},
        {"password", "wrongpassword"}
    });

    assert(resp["status"] == "error");
    assert(resp["code"] == 401);
    std::cout << "  Correctly rejected wrong password" << std::endl;
}

// 测试4: 获取当前用户（未登录状态）
TEST(GetCurrentUserNotLoggedIn)
{
    json resp = tester.sendRequest("user", "current", json::object());

    assert(resp["status"] == "ok");
    assert(resp["data"]["logged_in"] == false);
    std::cout << "  Correctly shows not logged in" << std::endl;
}

// 测试5: 创建学生
TEST(CreateStudent)
{
    // 先登录
    tester.sendRequest("user", "login", {{"username", "root"}, {"password", "root"}});

    // 定义 schema
    tester.sendRequest("system", "define", {
        {"target", "student"},
        {"schema", {{"name", "string"}, {"age", "int"}, {"score", "double"}}}
    });

    // 创建学生
    json resp = tester.sendRequest("student", "create", {
        {"items", json::array({
            {{"index", 0}, {"id", nullptr}, {"data", {{"name", "Alice"}, {"age", 18}, {"score", 95.5}}}},
            {{"index", 1}, {"id", nullptr}, {"data", {{"name", "Bob"}, {"age", 19}, {"score", 88.0}}}}
        })}
    });

    assert(resp["status"] == "ok");
    assert(resp["data"]["count"] == 2);
    std::cout << "  Created " << resp["data"]["count"] << " students" << std::endl;
}

// 测试6: 查询学生
TEST(QueryStudents)
{
    // 登录并设置
    tester.sendRequest("user", "login", {{"username", "root"}, {"password", "root"}});
    tester.sendRequest("system", "define", {
        {"target", "student"},
        {"schema", {{"name", "string"}, {"age", "int"}, {"score", "double"}}}
    });

    // 创建测试数据
    tester.sendRequest("student", "create", {
        {"items", json::array({
            {{"index", 0}, {"id", nullptr}, {"data", {{"name", "Charlie"}, {"age", 20}, {"score", 92.0}}}}
        })}
    });

    // 查询所有学生
    json resp = tester.sendRequest("student", "query", {
        {"logic", nullptr}
    });

    assert(resp["status"] == "ok");
    assert(resp["data"]["items"].is_array());
    std::cout << "  Found " << resp["data"]["items"].size() << " students" << std::endl;
}

// 测试7: 带条件查询
TEST(QueryStudentsWithCondition)
{
    tester.sendRequest("user", "login", {{"username", "root"}, {"password", "root"}});
    tester.sendRequest("system", "define", {
        {"target", "student"},
        {"schema", {{"name", "string"}, {"age", "int"}, {"score", "double"}}}
    });

    // 查询年龄大于等于18的学生
    json resp = tester.sendRequest("student", "query", {
        {"logic", {
            {"op", "AND"},
            {"rules", json::array({
                {{"field", "age"}, {"op", ">="}, {"val", 18}}
            })}
        }}
    });

    assert(resp["status"] == "ok");
    std::cout << "  Query returned " << resp["data"]["items"].size() << " matching students" << std::endl;
}

// 测试8: 创建用户
TEST(CreateUser)
{
    tester.sendRequest("user", "login", {{"username", "root"}, {"password", "root"}});

    json resp = tester.sendRequest("user", "create", {
        {"username", "testuser"},
        {"password", "testpass123"},
        {"permission", "read,write"}
    });

    assert(resp["status"] == "ok");
    assert(resp["data"]["success"] == true);
    std::cout << "  Created user: " << resp["data"]["user"]["username"] << std::endl;
}

// 测试9: 查询用户列表
TEST(QueryUsers)
{
    tester.sendRequest("user", "login", {{"username", "root"}, {"password", "root"}});

    json resp = tester.sendRequest("user", "query", json::object());

    assert(resp["status"] == "ok");
    assert(resp["data"]["users"].is_array());
    std::cout << "  Found " << resp["data"]["users"].size() << " users" << std::endl;
}

// 测试10: 手动提交
TEST(ManualCommit)
{
    tester.sendRequest("user", "login", {{"username", "root"}, {"password", "root"}});

    json resp = tester.sendRequest("system", "commit", json::object());

    assert(resp["status"] == "ok");
    assert(resp["data"]["committed"] == true);
    std::cout << "  Database committed successfully" << std::endl;
}

// 测试11: 创建事件
TEST(CreateEvent)
{
    tester.sendRequest("user", "login", {{"username", "root"}, {"password", "root"}});

    json resp = tester.sendRequest("event", "create", {
        {"id", nullptr},
        {"type", 1},  // STUDENT
        {"ref_id", 1},
        {"desc", "Test event"},
        {"val_prev", 0.0},
        {"val_curr", 10.0}
    });

    assert(resp["status"] == "ok");
    assert(resp["data"].contains("id"));
    std::cout << "  Created event with ID: " << resp["data"]["id"] << std::endl;
}

// 测试12: 用户登出
TEST(UserLogout)
{
    tester.sendRequest("user", "login", {{"username", "root"}, {"password", "root"}});

    json resp = tester.sendRequest("user", "logout", json::object());

    assert(resp["status"] == "ok");
    assert(resp["data"]["success"] == true);
    std::cout << "  Logged out successfully" << std::endl;
}

// ============================================================
// 主函数
// ============================================================

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "SecScoreDB WebSocket Client Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\nMake sure the WebSocket server is running at " << WS_URL << std::endl;
    std::cout << "Start it with: ./SecScoreDB-Websockets --port 8765 --db ./testdata_ws\n" << std::endl;

    // 等待一下让所有静态测试运行器执行
    std::this_thread::sleep_for(100ms);

    std::cout << "\n========================================" << std::endl;
    std::cout << "Test Results: " << testsPassed << " passed, " << testsFailed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return testsFailed > 0 ? 1 : 0;
}

