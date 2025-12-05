#pragma once

#include <nlohmann/json.hpp>

#include "Protocol.hpp"

namespace ws
{
    nlohmann::json handleSystem(const std::string& actionRaw,
                                const nlohmann::json& payload,
                                RequestContext& ctx);

    nlohmann::json handleStudent(const std::string& actionRaw,
                                 const nlohmann::json& payload,
                                 RequestContext& ctx);

    nlohmann::json handleGroup(const std::string& actionRaw,
                               const nlohmann::json& payload,
                               RequestContext& ctx);

    nlohmann::json handleEvent(const std::string& actionRaw,
                               const nlohmann::json& payload,
                               RequestContext& ctx);

    nlohmann::json handleUser(const std::string& actionRaw,
                              const nlohmann::json& payload,
                              RequestContext& ctx);
}

