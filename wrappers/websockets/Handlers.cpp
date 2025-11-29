#include "Handlers.hpp"

#include <cmath>
#include <limits>
#include <chrono>

#include "JsonUtils.hpp"

using nlohmann::json;

namespace ws
{
    template <typename WrapperFactory, typename CleanupFn, typename AllocFn>
    json handleCreate(const json& payload,
                      const SSDB::SchemaDef& schema,
                      WrapperFactory&& factory,
                      CleanupFn&& cleanup,
                      AllocFn&& allocator)
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
    json handleQuery(const json& payload,
                     const MapType& entities,
                     const SSDB::SchemaDef& schema)
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
    json handleUpdate(const json& payload,
                      const SSDB::SchemaDef& schema,
                      WrapperFetcher&& fetcher)
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
    json handleDelete(const json& payload,
                      std::string_view targetName,
                      DeleteFn&& deleter)
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

    json handleSystem(const std::string& actionRaw,
                      const json& payload,
                      RequestContext& ctx)
    {
        auto action = toLowerCopy(actionRaw);
        if (action == "commit")
        {
            std::scoped_lock lock(ctx.mutex);
            ctx.db.commit();
            return json{{"committed", true}};
        }
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
        std::scoped_lock lock(ctx.mutex);
        if (target == "student")
        {
            ctx.db.initStudentSchema(schema);
        }
        else if (target == "group")
        {
            ctx.db.initGroupSchema(schema);
        }
        else
        {
            throw ApiError(400, "target must be 'student' or 'group'.");
        }
        return json{{"target", target}, {"fields", schema.size()}};
    }

    json handleStudent(const std::string& actionRaw,
                       const json& payload,
                       RequestContext& ctx)
    {
        std::scoped_lock lock(ctx.mutex);
        const auto& schema = ctx.db.studentSchema();
        auto action = toLowerCopy(actionRaw);
        if (action == "create")
        {
            auto result = handleCreate(payload, schema,
                [&](int id) { return ctx.db.createStudent(id); },
                [&](int id) { ctx.db.deleteStudent(id); },
                [&]() { return ctx.db.allocateStudentId(); });
            if (result.at("count").get<size_t>() > 0)
            {
                ctx.db.commit();
            }
            return result;
        }
        if (action == "query")
        {
            return handleQuery(payload, ctx.db.students(), schema);
        }
        if (action == "update")
        {
            auto result = handleUpdate(payload, schema, [&](int id) { return ctx.db.getStudent(id); });
            ctx.db.commit();
            return result;
        }
        if (action == "delete")
        {
            auto result = handleDelete(payload, "student", [&](int id) { return ctx.db.deleteStudent(id); });
            ctx.db.commit();
            return result;
        }
        throw ApiError(400, "Unsupported student action: " + actionRaw);
    }

    json handleGroup(const std::string& actionRaw,
                     const json& payload,
                     RequestContext& ctx)
    {
        std::scoped_lock lock(ctx.mutex);
        const auto& schema = ctx.db.groupSchema();
        auto action = toLowerCopy(actionRaw);
        if (action == "create")
        {
            auto result = handleCreate(payload, schema,
                [&](int id) { return ctx.db.createGroup(id); },
                [&](int id) { ctx.db.deleteGroup(id); },
                [&]() { return ctx.db.allocateGroupId(); });
            if (result.at("count").get<size_t>() > 0)
            {
                ctx.db.commit();
            }
            return result;
        }
        if (action == "query")
        {
            return handleQuery(payload, ctx.db.groups(), schema);
        }
        if (action == "update")
        {
            auto result = handleUpdate(payload, schema, [&](int id) { return ctx.db.getGroup(id); });
            ctx.db.commit();
            return result;
        }
        if (action == "delete")
        {
            auto result = handleDelete(payload, "group", [&](int id) { return ctx.db.deleteGroup(id); });
            ctx.db.commit();
            return result;
        }
        throw ApiError(400, "Unsupported group action: " + actionRaw);
    }

    json handleEvent(const std::string& actionRaw,
                     const json& payload,
                     RequestContext& ctx)
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

            std::scoped_lock lock(ctx.mutex);
            int id = ctx.db.addEvent(evt);
            ctx.db.commit();
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
            std::scoped_lock lock(ctx.mutex);
            int id = payload.at("id").get<int>();
            ctx.db.setEventErased(id, payload.at("erased").get<bool>());
            ctx.db.commit();
            return json{{"id", id}, {"erased", payload.at("erased").get<bool>()}};
        }
        throw ApiError(400, "Unsupported event action: " + actionRaw);
    }
 }
