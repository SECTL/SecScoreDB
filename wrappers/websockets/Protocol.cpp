#include "Protocol.hpp"

#include "Errors.hpp"
#include "JsonUtils.hpp"
#include "Handlers.hpp"

using nlohmann::json;

namespace ws
{
    json dispatch(const std::string& category,
                  const std::string& action,
                  const json& payload,
                  RequestContext& ctx)
    {
        auto cat = toLowerCopy(category);
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
        throw ApiError(400, "Unsupported category: " + category);
    }

    json makeOkResponse(const std::string& seq, const json& data)
    {
        json response{{"seq", seq}, {"status", "ok"}, {"code", 200}};
        if (!data.is_null())
        {
            response["data"] = data;
        }
        return response;
    }

    json makeErrorResponse(const std::string& seq, int code, const std::string& message)
    {
        return json{{"seq", seq}, {"status", "error"}, {"code", code}, {"message", message}};
    }
}
