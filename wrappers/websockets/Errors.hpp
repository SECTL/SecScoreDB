#pragma once

#include <stdexcept>
#include <string>

namespace ws
{
    struct ApiError : std::runtime_error
    {
        int code;
        ApiError(int c, std::string msg);
    };
}

