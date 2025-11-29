#include "Errors.hpp"

namespace ws
{
    ApiError::ApiError(int c, std::string msg)
        : std::runtime_error(std::move(msg)), code(c)
    {
    }
}

