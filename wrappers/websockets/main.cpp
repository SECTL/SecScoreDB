#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>

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
                dbPath = argv[++i];
                continue;
            }
            std::cerr << "Unknown argument: " << arg << "\n";
            std::cerr << "Usage: SecScoreDB-Websockets [--port <number>] [--db <path>]" << std::endl;
            return 1;
        }

        ix::initNetSystem();
        SSDB::SecScoreDB database(dbPath);
        std::mutex dbMutex;
        ws::RequestContext ctx{database, dbMutex};

        ix::WebSocketServer server(port);

        server.setOnClientMessageCallback([&](std::shared_ptr<ix::ConnectionState>,
                                              ix::WebSocket& connection,
                                              const ix::WebSocketMessagePtr& msg)
        {
            if (msg->type != ix::WebSocketMessageType::Message)
            {
                return;
            }

            std::string seq;
            try
            {
                auto request = json::parse(msg->str);
                if (!request.contains("seq") || !request.at("seq").is_string())
                {
                    throw ws::ApiError(400, "seq is required and must be string.");
                }
                seq = request.at("seq").get<std::string>();
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
                connection.send(ws::makeOkResponse(seq, data).dump());
            }
            catch (const ws::ApiError& err)
            {
                connection.send(ws::makeErrorResponse(seq, err.code, err.what()).dump());
            }
            catch (const json::exception& err)
            {
                connection.send(ws::makeErrorResponse(seq, 400, std::string("Invalid JSON: ") + err.what()).dump());
            }
            catch (const std::exception& err)
            {
                connection.send(ws::makeErrorResponse(seq, 500, err.what()).dump());
            }
        });

        if (!server.listenAndStart())
        {
            std::cerr << "Failed to listen on port " << port << std::endl;
            return 1;
        }
        std::cout << "SecScoreDB WebSocket server listening on ws://0.0.0.0:" << port
                  << " using data directory '" << dbPath.string() << "'" << std::endl;
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