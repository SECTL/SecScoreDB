/**
 * @file Errors.hpp
 * @brief WebSocket API 错误类型定义
 */
#pragma once

#include <stdexcept>
#include <string>

namespace ws
{
    /**
     * @brief API 错误异常
     *
     * 包含 HTTP 状态码和错误消息
     */
    class ApiError : public std::runtime_error
    {
    public:
        int code;

        ApiError(int c, std::string msg);

        [[nodiscard]] int getCode() const noexcept { return code; }
    };

} // namespace ws

