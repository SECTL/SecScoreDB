/**
 * @file Errors.cpp
 * @brief WebSocket API 异常实现
 */
#include "Errors.hpp"

#include <utility>

namespace ws
{
    ApiError::ApiError(int c, std::string msg)
        : std::runtime_error(std::move(msg))
        , code(c)
    {
    }

} // namespace ws

