#include <chrono>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <algorithm>

#include <nlohmann/json.hpp>
#include <ixwebsocket/IXWebSocketServer.h>
#include <ixwebsocket/IXNetSystem.h>

#include "SecScoreDB.h"

using nlohmann::json;
using SSDB::FieldType;
using SSDB::SchemaDef;

namespace
{
    struct ApiError : std::runtime_error
    {
        int code;
        ApiError(int c, std::string msg) : std::runtime_error(std::move(msg)), code(c) {}
    };

    std::string toLowerCopy(std::string_view view)
    {
        std::string out(view.begin(), view.end());
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return out;
    }

    std::string toUpperCopy(std::string_view view)
    {
        std::string out(view.begin(), view.end());
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return out;
    }

    const char* fieldTypeName(FieldType type)
    {
        switch (type)
        {
        case FieldType::Int: return "int";
        case FieldType::Double: return "double";
        case FieldType::String: return "string";
        default: return "unknown";
        }
    }

    FieldType parseFieldType(const std::string& value)
    {
        auto lower = toLowerCopy(value);
        if (lower == "string") return FieldType::String;
        if (lower == "int") return FieldType::Int;
        if (lower == "double") return FieldType::Double;
        throw ApiError(400, "Unsupported field type: " + value);
    }

    void ensureSchemaReady(const SchemaDef& schema, std::string_view target)
    {
        if (schema.empty())
        {
            throw ApiError(422, std::string(target) + " schema is not defined.");
        }
    }

    SchemaDef parseSchema(const json& schemaJson)
    {
        if (!schemaJson.is_object() || schemaJson.empty())
        {
            throw ApiError(400, "schema must be a non-empty object.");
        }

        SchemaDef schema;
        for (const auto& [field, typeNode] : schemaJson.items())
        {
            if (!typeNode.is_string())
            {
                throw ApiError(400, "Field type for '" + field + "' must be string.");
            }
            schema[field] = parseFieldType(typeNode.get<std::string>());
        }
        return schema;
    }

    std::optional<json> decodeStoredValue(const std::string& raw, FieldType type)
    {
        try
        {
            switch (type)
            {
            case FieldType::String:
                return raw;
            case FieldType::Int:
                return static_cast<long long>(std::stoll(raw));
            case FieldType::Double:
                return std::stod(raw);
            default:
                return std::nullopt;
            }
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    template <typename Entity>
    json materializeEntityData(const Entity& entity, const SchemaDef& schema)
    {
        json data = json::object();
        const auto& meta = entity.GetMetadata();
        for (const auto& [field, type] : schema)
        {
            auto it = meta.find(field);
            if (it == meta.end())
            {
                continue;
            }
            if (auto decoded = decodeStoredValue(it->second, type))
            {
                data[field] = *decoded;
            }
        }
        return data;
    }

    double requireNumber(const json& value, std::string_view context)
    {
        if (!value.is_number())
        {
            throw ApiError(422, std::string(context) + " must be numeric.");
        }
        return value.get<double>();
    }

    bool compareNumbers(double lhs, double rhs, const std::string& op)
    {
        if (op == "==") return lhs == rhs;
        if (op == "!=") return lhs != rhs;
        if (op == ">") return lhs > rhs;
        if (op == ">=") return lhs >= rhs;
        if (op == "<") return lhs < rhs;
        if (op == "<=") return lhs <= rhs;
        throw ApiError(422, "Unsupported numeric operator: " + op);
    }

    bool compareStrings(const std::string& lhs, const std::string& rhs, const std::string& opLower)
    {
        if (opLower == "==") return lhs == rhs;
        if (opLower == "!=") return lhs != rhs;
        if (opLower == "contains") return lhs.find(rhs) != std::string::npos;
        if (opLower == "starts_with") return lhs.rfind(rhs, 0) == 0;
        if (opLower == "ends_with")
        {
            if (rhs.size() > lhs.size()) return false;
            return std::equal(rhs.rbegin(), rhs.rend(), lhs.rbegin());
        }
        throw ApiError(422, "Unsupported string operator: " + opLower);
    }

    bool evaluateLogicNode(const json& entityData, const json& node, const SchemaDef& schema)
    {
        if (!node.is_object())
        {
            throw ApiError(400, "logic node must be an object.");
        }

        if (node.contains("field"))
        {
            auto field = node.at("field").get<std::string>();
            auto opRaw = node.at("op").get<std::string>();
            if (!node.contains("val"))
            {
                throw ApiError(400, "Leaf rule is missing 'val'.");
            }
            auto schemaIt = schema.find(field);
            if (schemaIt == schema.end())
            {
                throw ApiError(422, "Field '" + field + "' is not defined in schema.");
            }
            if (!entityData.contains(field))
            {
                return false;
            }
            const auto& lhs = entityData.at(field);
            const auto& rhs = node.at("val");
            auto type = schemaIt->second;
            if (type == FieldType::String)
            {
                if (!lhs.is_string() || !rhs.is_string())
                {
                    throw ApiError(422, "String comparison requires string operands.");
                }
                return compareStrings(lhs.get_ref<const std::string&>(), rhs.get_ref<const std::string&>(), toLowerCopy(opRaw));
            }
            if (type == FieldType::Int || type == FieldType::Double)
            {
                double lhsVal = requireNumber(lhs, field);
                double rhsVal = requireNumber(rhs, "val");
                return compareNumbers(lhsVal, rhsVal, opRaw);
            }
            throw ApiError(422, "Unsupported field type in logic rule.");
        }

        auto op = toUpperCopy(node.at("op").get<std::string>());
        const auto& rules = node.at("rules");
        if (!rules.is_array() || rules.empty())
        {
            throw ApiError(400, "logic.rules must be a non-empty array.");
        }

        if (op == "AND")
        {
            for (const auto& child : rules)
            {
                if (!evaluateLogicNode(entityData, child, schema))
                {
                    return false;
                }
            }
            return true;
        }
        if (op == "OR")
        {
            for (const auto& child : rules)
            {
                if (evaluateLogicNode(entityData, child, schema))
                {
                    return true;
                }
            }
            return false;
        }
        throw ApiError(400, "Unsupported logic operator: " + op);
    }

    template <typename Wrapper>
    void assignDynamicFields(Wrapper& wrapper, const json& data, const SchemaDef& schema)
    {
        if (!data.is_object())
        {
            throw ApiError(400, "data must be an object.");
        }
        for (const auto& [field, value] : data.items())
        {
            auto it = schema.find(field);
            if (it == schema.end())
            {
                throw ApiError(422, "Field '" + field + "' is not defined in schema.");
            }
            switch (it->second)
            {
            case FieldType::String:
                if (!value.is_string())
                {
                    throw ApiError(422, "Field '" + field + "' requires string value.");
                }
                wrapper[field] = value.get_ref<const std::string&>();
                break;
            case FieldType::Int:
                if (!value.is_number_integer())
                {
                    throw ApiError(422, "Field '" + field + "' requires integer value.");
                }
                wrapper[field] = value.get<long long>();
                break;
            case FieldType::Double:
                if (!value.is_number())
                {
                    throw ApiError(422, "Field '" + field + "' requires numeric value.");
                }
                wrapper[field] = value.get<double>();
                break;
            default:
                throw ApiError(422, "Unsupported field type for '" + field + "'.");
            }
        }
    }

    template <typename WrapperFactory, typename CleanupFn, typename AllocFn>
    json handleCreate(const json& payload, const SchemaDef& schema, WrapperFactory&& factory, CleanupFn&& cleanup, AllocFn&& allocator)
    {
        ensureSchemaReady(schema, "entity");
        if (!payload.contains("items") || !payload.at("items").is_array())
        {
            throw ApiError(400, "payload.items must be an array.");
        }

        const auto& items = payload.at("items");
        json results = json::array();
        size_t successCount = 0;
        for (const auto& item : items)
        {
            int index = item.value("index", 0);
            try
            {
                if (!item.contains("data"))
                {
                    throw ApiError(400, "Each item must include data.");
                }
                int id;
                if (!item.contains("id") || item.at("id").is_null())
                {
                    id = allocator();
                }
                else if (item.at("id").is_number_integer())
                {
                    id = item.at("id").get<int>();
                }
                else
                {
                    throw ApiError(422, "id must be null or integer.");
                }

                auto wrapper = factory(id);
                bool assigned = false;
                try
                {
                    assignDynamicFields(wrapper, item.at("data"), schema);
                    assigned = true;
                }
                catch (...)
                {
                    cleanup(id);
                    throw;
                }

                (void)assigned;
                results.push_back({{"index", index}, {"success", true}, {"id", id}});
                successCount++;
            }
            catch (const ApiError& err)
            {
                results.push_back({{"index", index}, {"success", false}, {"message", err.what()}});
            }
            catch (const std::exception& err)
            {
                results.push_back({{"index", index}, {"success", false}, {"message", err.what()}});
            }
        }
        return json{{"count", successCount}, {"results", results}};
    }

    template <typename MapType>
    json handleQuery(const json& payload, const MapType& entities, const SchemaDef& schema)
    {
        ensureSchemaReady(schema, "entity");
        size_t limit = std::numeric_limits<size_t>::max();
        if (payload.contains("limit"))
        {
            if (!payload.at("limit").is_number_unsigned())
            {
                throw ApiError(400, "limit must be a non-negative integer.");
            }
            limit = payload.at("limit").get<size_t>();
            if (limit == 0)
            {
                limit = std::numeric_limits<size_t>::max();
            }
        }
        const json* logicNode = nullptr;
        if (payload.contains("logic"))
        {
            logicNode = &payload.at("logic");
        }

        json items = json::array();
        for (const auto& [id, entity] : entities)
        {
            auto data = materializeEntityData(entity, schema);
            bool matched = true;
            if (logicNode && !logicNode->is_null())
            {
                matched = evaluateLogicNode(data, *logicNode, schema);
            }
            if (matched)
            {
                items.push_back({{"id", id}, {"data", data}});
                if (items.size() >= limit)
                {
                    break;
                }
            }
        }
        return json{{"items", items}};
    }

    template <typename WrapperFetcher>
    json handleUpdate(const json& payload, const SchemaDef& schema, WrapperFetcher&& fetcher)
    {
        ensureSchemaReady(schema, "entity");
        if (!payload.contains("id") || !payload.at("id").is_number_integer())
        {
            throw ApiError(400, "payload.id must be integer.");
        }
        if (!payload.contains("set"))
        {
            throw ApiError(400, "payload.set is required.");
        }
        int id = payload.at("id").get<int>();
        auto wrapper = fetcher(id);
        assignDynamicFields(wrapper, payload.at("set"), schema);
        return json{{"id", id}, {"updated", true}};
    }

    template <typename DeleteFn>
    json handleDelete(const json& payload, std::string_view targetName, DeleteFn&& deleter)
    {
        if (!payload.contains("id") || !payload.at("id").is_number_integer())
        {
            throw ApiError(400, "payload.id must be integer.");
        }
        int id = payload.at("id").get<int>();
        if (!deleter(id))
        {
            throw ApiError(404, std::string(targetName) + " id not found.");
        }
        return json{{"id", id}, {"deleted", true}};
    }

    SSDB::EventType parseEventType(int type)
    {
        if (type == 1) return SSDB::EventType::STUDENT;
        if (type == 2) return SSDB::EventType::GROUP;
        throw ApiError(422, "event.type must be 1 (student) or 2 (group).");
    }

    json makeOkResponse(const std::string& seq, const json& data = json::object())
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

    json handleSystem(const std::string& actionRaw, const json& payload, SSDB::SecScoreDB& db, std::mutex& mutex)
    {
        auto action = toLowerCopy(actionRaw);
        if (action != "define")
        {
            throw ApiError(400, "Unsupported system action: " + actionRaw);
        }
        if (!payload.contains("target") || !payload.at("target").is_string())
        {
            throw ApiError(400, "payload.target must be string.");
        }
        if (!payload.contains("schema"))
        {
            throw ApiError(400, "payload.schema is required.");
        }

        auto target = toLowerCopy(payload.at("target").get<std::string>());
        auto schema = parseSchema(payload.at("schema"));
        std::scoped_lock lock(mutex);
        if (target == "student")
        {
            db.initStudentSchema(schema);
        }
        else if (target == "group")
        {
            db.initGroupSchema(schema);
        }
        else
        {
            throw ApiError(400, "target must be 'student' or 'group'.");
        }
        return json{{"target", target}, {"fields", schema.size()}};
    }

    json handleStudent(const std::string& actionRaw, const json& payload, SSDB::SecScoreDB& db, std::mutex& mutex)
    {
        std::scoped_lock lock(mutex);
        const auto& schema = db.studentSchema();
        auto action = toLowerCopy(actionRaw);
        if (action == "create")
        {
            auto result = handleCreate(payload, schema,
                [&](int id) { return db.createStudent(id); },
                [&](int id) { db.deleteStudent(id); },
                [&]() { return db.allocateStudentId(); });
            if (result.at("count").get<size_t>() > 0)
            {
                db.commit();
            }
            return result;
        }
        if (action == "query")
        {
            return handleQuery(payload, db.students(), schema);
        }
        if (action == "update")
        {
            auto result = handleUpdate(payload, schema, [&](int id) { return db.getStudent(id); });
            db.commit();
            return result;
        }
        if (action == "delete")
        {
            auto result = handleDelete(payload, "student", [&](int id) { return db.deleteStudent(id); });
            db.commit();
            return result;
        }
        throw ApiError(400, "Unsupported student action: " + actionRaw);
    }

    json handleGroup(const std::string& actionRaw, const json& payload, SSDB::SecScoreDB& db, std::mutex& mutex)
    {
        std::scoped_lock lock(mutex);
        const auto& schema = db.groupSchema();
        auto action = toLowerCopy(actionRaw);
        if (action == "create")
        {
            auto result = handleCreate(payload, schema,
                [&](int id) { return db.createGroup(id); },
                [&](int id) { db.deleteGroup(id); },
                [&]() { return db.allocateGroupId(); });
            if (result.at("count").get<size_t>() > 0)
            {
                db.commit();
            }
            return result;
        }
        if (action == "query")
        {
            return handleQuery(payload, db.groups(), schema);
        }
        if (action == "update")
        {
            auto result = handleUpdate(payload, schema, [&](int id) { return db.getGroup(id); });
            db.commit();
            return result;
        }
        if (action == "delete")
        {
            auto result = handleDelete(payload, "group", [&](int id) { return db.deleteGroup(id); });
            db.commit();
            return result;
        }
        throw ApiError(400, "Unsupported group action: " + actionRaw);
    }

    json handleEvent(const std::string& actionRaw, const json& payload, SSDB::SecScoreDB& db, std::mutex& mutex)
    {
        auto action = toLowerCopy(actionRaw);
        if (action == "create")
        {
            if (!payload.contains("id") || !payload.at("id").is_null())
            {
                throw ApiError(422, "event.id must be null for auto generation.");
            }
            if (!payload.contains("type") || !payload.at("type").is_number_integer())
            {
                throw ApiError(400, "event.type must be integer.");
            }
            if (!payload.contains("ref_id") || !payload.at("ref_id").is_number_integer())
            {
                throw ApiError(400, "event.ref_id must be integer.");
            }
            if (!payload.contains("desc") || !payload.at("desc").is_string())
            {
                throw ApiError(400, "event.desc must be string.");
            }
            if (!payload.contains("val_prev") || !payload.contains("val_curr"))
            {
                throw ApiError(400, "event.val_prev and event.val_curr are required.");
            }
            double prev = requireNumber(payload.at("val_prev"), "val_prev");
            double curr = requireNumber(payload.at("val_curr"), "val_curr");

            SSDB::Event evt;
            evt.SetId(SSDB::INVALID_ID);
            evt.SetEventType(parseEventType(payload.at("type").get<int>()));
            evt.SetOperatingObject(payload.at("ref_id").get<int>());
            evt.SetReason(payload.at("desc").get<std::string>());
            evt.SetDeltaScore(static_cast<int>(std::llround(curr - prev)));

            auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(evt.GetEventTime().time_since_epoch()).count();

            std::scoped_lock lock(mutex);
            int id = db.addEvent(evt);
            db.commit();
            return json{{"id", id}, {"timestamp", timestamp}};
        }
        if (action == "update")
        {
            if (!payload.contains("id") || !payload.at("id").is_number_integer())
            {
                throw ApiError(400, "event.id must be integer.");
            }
            if (!payload.contains("erased") || !payload.at("erased").is_boolean())
            {
                throw ApiError(400, "event.erased must be boolean.");
            }
            std::scoped_lock lock(mutex);
            int id = payload.at("id").get<int>();
            db.setEventErased(id, payload.at("erased").get<bool>());
            db.commit();
            return json{{"id", id}, {"erased", payload.at("erased").get<bool>()}};
        }
        throw ApiError(400, "Unsupported event action: " + actionRaw);
    }

    json dispatch(const std::string& categoryRaw, const std::string& action, const json& payload, SSDB::SecScoreDB& db, std::mutex& mutex)
    {
        auto category = toLowerCopy(categoryRaw);
        if (category == "system")
        {
            return handleSystem(action, payload, db, mutex);
        }
        if (category == "student")
        {
            return handleStudent(action, payload, db, mutex);
        }
        if (category == "group")
        {
            return handleGroup(action, payload, db, mutex);
        }
        if (category == "event")
        {
            return handleEvent(action, payload, db, mutex);
        }
        throw ApiError(400, "Unsupported category: " + categoryRaw);
    }
}

int main(int argc, char** argv)
{
    try
    {
        uint16_t port = 8765;
        std::filesystem::path dbPath = std::filesystem::current_path() / "data";

        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];
            if (arg == "--port" && i + 1 < argc)
            {
                port = static_cast<uint16_t>(std::stoi(argv[++i]));
                continue;
            }
            if (arg == "--db" && i + 1 < argc)
            {
                dbPath = argv[++i];
                continue;
            }
            std::cerr << "Unknown argument: " << arg << "\n";
            std::cerr << "Usage: SecScoreDB-Websockets [--port <number>] [--db <path>]" << std::endl;
            return 1;
        }

        ix::initNetSystem();
        SSDB::SecScoreDB database(dbPath);
        std::mutex dbMutex;

        ix::WebSocketServer server(port);
        server.setOnConnection([&](const ix::WebSocketConnectionPtr& connection)
        {
            connection->setOnMessageCallback([&, connection](const ix::WebSocketMessagePtr& msg)
            {
                if (msg->type != ix::WebSocketMessageType::Message)
                {
                    return;
                }

                std::string seq;
                try
                {
                    auto request = json::parse(msg->str);
                    if (!request.contains("seq") || !request.at("seq").is_string())
                    {
                        throw ApiError(400, "seq is required and must be string.");
                    }
                    seq = request.at("seq").get<std::string>();
                    if (!request.contains("category") || !request.at("category").is_string())
                    {
                        throw ApiError(400, "category is required.");
                    }
                    if (!request.contains("action") || !request.at("action").is_string())
                    {
                        throw ApiError(400, "action is required.");
                    }
                    if (!request.contains("payload"))
                    {
                        throw ApiError(400, "payload is required.");
                    }
                    const auto& payload = request.at("payload");
                    if (!payload.is_object())
                    {
                        throw ApiError(400, "payload must be an object.");
                    }

                    auto data = dispatch(request.at("category").get<std::string>(), request.at("action").get<std::string>(), payload, database, dbMutex);
                    connection->send(makeOkResponse(seq, data).dump());
                }
                catch (const ApiError& err)
                {
                    connection->send(makeErrorResponse(seq, err.code, err.what()).dump());
                }
                catch (const json::exception& err)
                {
                    connection->send(makeErrorResponse(seq, 400, std::string("Invalid JSON: ") + err.what()).dump());
                }
                catch (const std::exception& err)
                {
                    connection->send(makeErrorResponse(seq, 500, err.what()).dump());
                }
            });
        });

        if (!server.listen().first)
        {
            std::cerr << "Failed to listen on port " << port << std::endl;
            return 1;
        }
        server.start();
        std::cout << "SecScoreDB WebSocket server listening on ws://0.0.0.0:" << port
                  << " using data directory '" << dbPath.string() << "'" << std::endl;
        server.wait();
        ix::uninitNetSystem();
    }
    catch (const std::exception& err)
    {
        std::cerr << "Fatal error: " << err.what() << std::endl;
        return 1;
    }
    return 0;
}