/**
 * @file Protocol.cpp
 * @brief WebSocket 协议实现
 */
#include "Protocol.hpp"

#include "Errors.hpp"
#include "Handlers.hpp"
#include "JsonUtils.hpp"

#include <format>
#include <string>

using nlohmann::json;

namespace ws
{
    json dispatch(std::string_view category,
                  std::string_view action,
                  const json& payload,
                  RequestContext& ctx)
    {
        const auto cat = toLowerCopy(category);

        if (cat == "system")
        {
            return handleSystem(action, payload, ctx);
        }
        if (cat == "student")
        {
            return handleStudent(action, payload, ctx);
        }
        if (cat == "group")
        {
            return handleGroup(action, payload, ctx);
        }
        if (cat == "event")
        {
            return handleEvent(action, payload, ctx);
        }
        if (cat == "user")
        {
            return handleUser(action, payload, ctx);
        }

        throw ApiError(400, std::format("Unsupported category: {}", category));
    }

    json makeOkResponse(std::string_view seq, const json& data)
    {
        json response{
            {"seq", std::string(seq)},
            {"status", "ok"},
            {"code", 200}
        };

        if (!data.is_null())
        {
            response["data"] = data;
        }

        return response;
    }

    json makeErrorResponse(std::string_view seq, int code, std::string_view message)
    {
        return json{
            {"seq", std::string(seq)},
            {"status", "error"},
            {"code", code},
            {"message", std::string(message)}
        };
    }

} // namespace ws
