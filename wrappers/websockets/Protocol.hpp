/**
 * @file Protocol.hpp
 * @brief WebSocket 协议定义和请求上下文
 */
#pragma once

#include "Errors.hpp"
#include "SecScoreDB.h"

#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace ws
{
    /**
     * @brief WebSocket 连接的请求上下文
     *
     * 每个连接有独立的登录状态 (currentUserId)，
     * 但共享同一个数据库实例。
     */
    struct RequestContext
    {
        SSDB::SecScoreDB& db;
        std::mutex& mutex;
        std::optional<int> currentUserId = std::nullopt;

        [[nodiscard]] bool isLoggedIn() const noexcept { return currentUserId.has_value(); }
        void login(int userId) noexcept { currentUserId = userId; }
        void logout() noexcept { currentUserId = std::nullopt; }
    };

    /**
     * @brief 分发请求到对应的处理器
     */
    [[nodiscard]] nlohmann::json dispatch(std::string_view category,
                                          std::string_view action,
                                          const nlohmann::json& payload,
                                          RequestContext& ctx);

    /**
     * @brief 构造成功响应
     */
    [[nodiscard]] nlohmann::json makeOkResponse(std::string_view seq,
                                                const nlohmann::json& data = nlohmann::json::object());

    /**
     * @brief 构造错误响应
     */
    [[nodiscard]] nlohmann::json makeErrorResponse(std::string_view seq,
                                                   int code,
                                                   std::string_view message);

} // namespace ws
