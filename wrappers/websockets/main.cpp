/**
 * @file main.cpp
 * @brief SecScoreDB WebSocket 服务器入口点
 */
#include "Protocol.hpp"
#include "SecScoreDB.h"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <nlohmann/json.hpp>

using nlohmann::json;

namespace
{
    /**
     * @brief 打印使用说明
     */
    void printUsage(const char* programName)
    {
        std::cerr << "Usage: " << programName << " [--port <number>] [--db <path>]\n"
                  << "Options:\n"
                  << "  --port <number>  WebSocket server port (default: 8765)\n"
                  << "  --db <path>      Database directory path (default: ./data)\n";
    }
}

int main(int argc, char** argv)
{
    try
    {
        std::uint16_t port = 8765;
        std::filesystem::path dbPath = std::filesystem::current_path() / "data";

        // 解析命令行参数
        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];

            if (arg == "--port" && i + 1 < argc)
            {
                port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
                continue;
            }

            if (arg == "--db" && i + 1 < argc)
            {
                dbPath = std::filesystem::path(argv[++i]);
                continue;
            }

            if (arg == "--help" || arg == "-h")
            {
                printUsage(argv[0]);
                return 0;
            }

            std::cerr << "Unknown argument: " << arg << '\n';
            printUsage(argv[0]);
            return 1;
        }

        // 初始化网络系统
        ix::initNetSystem();

        // 数据库和同步
        std::mutex dbMutex;
        SSDB::SecScoreDB database(dbPath);

        // 连接登录状态管理
        std::unordered_map<std::string, std::optional<int>> connectionUserMap;
        std::mutex connectionMapMutex;

        // 创建服务器
        ix::WebSocketServer server(port);

        server.setOnClientMessageCallback(
            [&](std::shared_ptr<ix::ConnectionState> connState,
                ix::WebSocket& connection,
                const ix::WebSocketMessagePtr& msg)
        {
            const std::string connId = connState->getId();

            // 处理连接关闭
            if (msg->type == ix::WebSocketMessageType::Close)
            {
                std::scoped_lock lock(connectionMapMutex);
                connectionUserMap.erase(connId);
                std::cout << "[DEBUG] Connection " << connId << " closed\n";
                return;
            }

            std::cout << "[DEBUG] Message type: " << static_cast<int>(msg->type) << '\n';

            if (msg->type == ix::WebSocketMessageType::Message)
            {
                std::cout << "[DEBUG] Received: " << msg->str << '\n';
            }

            if (msg->type != ix::WebSocketMessageType::Message)
            {
                return;
            }

            // 获取此连接的登录状态
            std::optional<int> currentUserId;
            {
                std::scoped_lock lock(connectionMapMutex);
                currentUserId = connectionUserMap[connId];
            }

            // 创建请求上下文
            ws::RequestContext ctx{database, dbMutex, currentUserId};

            std::string seq;

            try
            {
                auto request = json::parse(msg->str);

                if (!request.contains("seq") || !request.at("seq").is_string())
                {
                    throw ws::ApiError(400, "seq is required and must be string.");
                }
                seq = request.at("seq").get<std::string>();
                std::cout << "[DEBUG] Processing seq: " << seq << '\n';

                if (!request.contains("category") || !request.at("category").is_string())
                {
                    throw ws::ApiError(400, "category is required.");
                }
                if (!request.contains("action") || !request.at("action").is_string())
                {
                    throw ws::ApiError(400, "action is required.");
                }
                if (!request.contains("payload"))
                {
                    throw ws::ApiError(400, "payload is required.");
                }

                const auto& payload = request.at("payload");
                if (!payload.is_object())
                {
                    throw ws::ApiError(400, "payload must be an object.");
                }

                auto data = ws::dispatch(
                    request.at("category").get<std::string>(),
                    request.at("action").get<std::string>(),
                    payload,
                    ctx
                );

                // 保存更新后的登录状态
                {
                    std::scoped_lock lock(connectionMapMutex);
                    connectionUserMap[connId] = ctx.currentUserId;
                }

                auto response = ws::makeOkResponse(seq, data).dump();
                std::cout << "[DEBUG] Sending: " << response << '\n';
                auto result = connection.send(response);
                std::cout << "[DEBUG] Send result: " << (result.success ? "SUCCESS" : "FAILED") << '\n';
            }
            catch (const ws::ApiError& err)
            {
                auto response = ws::makeErrorResponse(seq, err.code, err.what()).dump();
                std::cout << "[DEBUG] Sending error: " << response << '\n';
                connection.send(response);
            }
            catch (const json::exception& err)
            {
                auto response = ws::makeErrorResponse(seq, 400,
                    std::string("Invalid JSON: ") + err.what()).dump();
                std::cout << "[DEBUG] Sending JSON error: " << response << '\n';
                connection.send(response);
            }
            catch (const std::exception& err)
            {
                auto response = ws::makeErrorResponse(seq, 500, err.what()).dump();
                std::cout << "[DEBUG] Sending exception: " << response << '\n';
                connection.send(response);
            }
        });

        // 启动服务器
        if (!server.listenAndStart())
        {
            std::cerr << "Failed to listen on port " << port << '\n';
            return 1;
        }

        std::cout << "SecScoreDB WebSocket server listening on ws://0.0.0.0:" << port
                  << "\nDatabase directory: " << dbPath.string() << '\n';

        server.wait();
        ix::uninitNetSystem();
    }
    catch (const std::exception& err)
    {
        std::cerr << "Fatal error: " << err.what() << '\n';
        return 1;
    }

    return 0;
}