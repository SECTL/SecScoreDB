#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <optional>

#include <nlohmann/json.hpp>
#include <ixwebsocket/IXWebSocketServer.h>
#include <ixwebsocket/IXNetSystem.h>

#include "SecScoreDB.h"
#include "Protocol.hpp"

using nlohmann::json;

int main(int argc, char** argv)
{
    try
    {
        uint16_t port = 8765;
        std::filesystem::path dbPath = std::filesystem::current_path() / "data";

        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];
            if (arg == "--port" && i + 1 < argc)
            {
                port = static_cast<uint16_t>(std::stoi(argv[++i]));
                continue;
            }
            if (arg == "--db" && i + 1 < argc)
            {
                dbPath = std::filesystem::path(argv[++i]);
                continue;
            }
            std::cerr << "Unknown argument: " << arg << "\n";
            std::cerr << "Usage: SecScoreDB-Websockets [--port <number>] [--db <path>]" << std::endl;
            return 1;
        }

        ix::initNetSystem();
        std::mutex dbMutex;
        SSDB::SecScoreDB database(dbPath);

        // 为每个连接存储独立的登录状态
        std::unordered_map<std::string, std::optional<int>> connectionUserMap;
        std::mutex connectionMapMutex;

        ix::WebSocketServer server(port);

        server.setOnClientMessageCallback([&](std::shared_ptr<ix::ConnectionState> connState,
                                              ix::WebSocket& connection,
                                              const ix::WebSocketMessagePtr& msg)
        {
            std::string connId = connState->getId();

            // 处理连接关闭，清理登录状态
            if (msg->type == ix::WebSocketMessageType::Close)
            {
                std::scoped_lock lock(connectionMapMutex);
                connectionUserMap.erase(connId);
                std::cout << "[DEBUG] Connection " << connId << " closed, cleaned up user state" << std::endl;
                return;
            }

            std::cout << "[DEBUG] Message type: " << static_cast<int>(msg->type) << std::endl;
            if (msg->type == ix::WebSocketMessageType::Message)
            {
                std::cout << "[DEBUG] Received: " << msg->str << std::endl;
            }

            if (msg->type != ix::WebSocketMessageType::Message)
            {
                return;
            }

            // 获取或创建此连接的登录状态
            std::optional<int> currentUserId;
            {
                std::scoped_lock lock(connectionMapMutex);
                currentUserId = connectionUserMap[connId];
            }

            // 创建此请求的上下文
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
                std::cout << "[DEBUG] Processing seq: " << seq << std::endl;

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

                auto data = ws::dispatch(request.at("category").get<std::string>(),
                                         request.at("action").get<std::string>(),
                                         payload,
                                         ctx);

                // 保存更新后的登录状态
                {
                    std::scoped_lock lock(connectionMapMutex);
                    connectionUserMap[connId] = ctx.currentUserId;
                }

                auto response = ws::makeOkResponse(seq, data).dump();
                std::cout << "[DEBUG] Sending: " << response << std::endl;
                auto result = connection.send(response);
                std::cout << "[DEBUG] Send result: " << (result.success ? "SUCCESS" : "FAILED") << std::endl;
            }
            catch (const ws::ApiError& err)
            {
                auto response = ws::makeErrorResponse(seq, err.code, err.what()).dump();
                std::cout << "[DEBUG] Sending error: " << response << std::endl;
                connection.send(response);
            }
            catch (const json::exception& err)
            {
                auto response = ws::makeErrorResponse(seq, 400, std::string("Invalid JSON: ") + err.what()).dump();
                std::cout << "[DEBUG] Sending JSON error: " << response << std::endl;
                connection.send(response);
            }
            catch (const std::exception& err)
            {
                auto response = ws::makeErrorResponse(seq, 500, err.what()).dump();
                std::cout << "[DEBUG] Sending exception: " << response << std::endl;
                connection.send(response);
            }
        });

        if (!server.listenAndStart())
        {
            std::cerr << "Failed to listen on port " << port << std::endl;
            return 1;
        }
        std::cout << "SecScoreDB WebSocket server listening on ws://0.0.0.0:" << port
                  << " using data directory: " << dbPath.string() << std::endl;
        server.wait();
        ix::uninitNetSystem();
    }
    catch (const std::exception& err)
    {
        std::cerr << "Fatal error: " << err.what() << std::endl;
        return 1;
    }
    return 0;
}