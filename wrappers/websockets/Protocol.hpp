#pragma once

#include <mutex>
#include <string>

#include <nlohmann/json.hpp>

#include "SecScoreDB.h"
#include "Errors.hpp"

namespace ws
{
    struct RequestContext
    {
        SSDB::SecScoreDB& db;
        std::mutex& mutex;
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
