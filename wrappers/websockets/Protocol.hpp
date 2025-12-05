#pragma once

#include <mutex>
#include <string>
#include <optional>

#include <nlohmann/json.hpp>

#include "SecScoreDB.h"
#include "Errors.hpp"

namespace ws
{
    /**
     * @brief 每个 WebSocket 连接的请求上下文
     *
     * 每个连接有独立的登录状态 (currentUserId)，
     * 但共享同一个数据库实例。
     */
    struct RequestContext
    {
        SSDB::SecScoreDB& db;
        std::mutex& mutex;

        // 每个连接独立的登录用户 ID
        // -1 或 nullopt 表示未登录
        std::optional<int> currentUserId = std::nullopt;

        bool isLoggedIn() const { return currentUserId.has_value(); }
        void login(int userId) { currentUserId = userId; }
        void logout() { currentUserId = std::nullopt; }
    };

    nlohmann::json dispatch(const std::string& category,
                            const std::string& action,
                            const nlohmann::json& payload,
                            RequestContext& ctx);

    nlohmann::json makeOkResponse(const std::string& seq,
                                  const nlohmann::json& data = nlohmann::json::object());

    nlohmann::json makeErrorResponse(const std::string& seq,
                                     int code,
                                     const std::string& message);
}
