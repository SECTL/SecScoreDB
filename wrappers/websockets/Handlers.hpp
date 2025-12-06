/**
 * @file Handlers.hpp
 * @brief WebSocket API 处理器声明
 */
#pragma once

#include "Protocol.hpp"

#include <string_view>

#include <nlohmann/json.hpp>

namespace ws
{
    /**
     * @brief 系统操作处理器（define, commit）
     */
    [[nodiscard]] nlohmann::json handleSystem(std::string_view action,
                                              const nlohmann::json& payload,
                                              RequestContext& ctx);

    /**
     * @brief 学生操作处理器（create, query, update, delete）
     */
    [[nodiscard]] nlohmann::json handleStudent(std::string_view action,
                                               const nlohmann::json& payload,
                                               RequestContext& ctx);

    /**
     * @brief 分组操作处理器（create, query, update, delete）
     */
    [[nodiscard]] nlohmann::json handleGroup(std::string_view action,
                                             const nlohmann::json& payload,
                                             RequestContext& ctx);

    /**
     * @brief 事件操作处理器（create, update）
     */
    [[nodiscard]] nlohmann::json handleEvent(std::string_view action,
                                             const nlohmann::json& payload,
                                             RequestContext& ctx);

    /**
     * @brief 用户操作处理器（login, logout, create, delete, update, query）
     */
    [[nodiscard]] nlohmann::json handleUser(std::string_view action,
                                            const nlohmann::json& payload,
                                            RequestContext& ctx);

} // namespace ws

