#pragma once
#include "SSDBType.h"
#include <chrono>
#include <string>
#include <cstdint> // 必须包含，用于 std::int64_t
#include <utility> // 用于 std::move

// Cereal
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>

namespace SSDB
{
    class Event
    {
    private:
        // 1. 类内初始化：直接在这里赋默认值，更安全，更现代
        std::chrono::time_point<std::chrono::system_clock> event_time = std::chrono::system_clock::now();
        int id = 0;
        EventType event_type = EventType::STUDENT; // 假设这是默认值
        int operating_object = 0;
        std::string reason;
        int operate_person = 0;
        int delta_score = 0;
        bool erased = false;

    public:
        // 2. 默认构造函数：因为成员已经有默认值了，这里可以是空的
        Event() = default;

        // 3. 带参构造函数：使用 Sink 模式 (std::string 传值 + move)
        // 注意：初始化列表的顺序必须与成员变量声明顺序一致！
        Event(int _id,
              EventType _type,
              int _operating_object,
              std::string _reason, // Pass-by-value
              int _operate_person,
              int _delta_score,
              std::chrono::time_point<std::chrono::system_clock> _time = std::chrono::system_clock::now())
            : event_time(_time),
              id(_id),
              event_type(_type),
              operating_object(_operating_object),
              reason(std::move(_reason)), // Move! 0拷贝接管
              operate_person(_operate_person),
              delta_score(_delta_score)
        {
        }

        // 4. Getters：加上 [[nodiscard]]
        [[nodiscard]] std::chrono::time_point<std::chrono::system_clock> GetEventTime() const { return event_time; }
        [[nodiscard]] int GetId() const { return id; }
        [[nodiscard]] EventType GetEventType() const { return event_type; }
        [[nodiscard]] int GetOperatingObject() const { return operating_object; }
        [[nodiscard]] const std::string& GetReason() const { return reason; }
        [[nodiscard]] int GetOperatePerson() const { return operate_person; }
        [[nodiscard]] int GetDeltaScore() const { return delta_score; }
        [[nodiscard]] bool IsErased() const { return erased; }

        // 5. Setters
        void SetEventTime(std::chrono::time_point<std::chrono::system_clock> t) { event_time = t; }
        void SetId(int _id) { id = _id; }
        void SetEventType(EventType t) { event_type = t; }
        void SetOperatingObject(int obj) { operating_object = obj; }

        // Setter 优化：Sink 模式
        void SetReason(std::string r) { reason = std::move(r); }

        void SetOperatePerson(int p) { operate_person = p; }
        void SetDeltaScore(int s) { delta_score = s; }
        void SetErased(bool e) { erased = e; }

        // cereal 序列化
        template <class Archive>
        void save(Archive& ar) const
        {
            // 显式使用 std::int64_t 确保跨平台一致性
            auto millis = static_cast<std::int64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(event_time.time_since_epoch()).count()
            );
            int et = static_cast<int>(event_type);

            ar(cereal::make_nvp("millis", millis),
               cereal::make_nvp("id", id),
               cereal::make_nvp("event_type", et),
               cereal::make_nvp("operating_object", operating_object),
               cereal::make_nvp("reason", reason),
               cereal::make_nvp("operate_person", operate_person),
               cereal::make_nvp("delta_score", delta_score),
               cereal::make_nvp("erased", erased));
        }

        template <class Archive>
        void load(Archive& ar)
        {
            std::int64_t millis = 0;
            int et = 0;

            ar(cereal::make_nvp("millis", millis),
               cereal::make_nvp("id", id),
               cereal::make_nvp("event_type", et),
               cereal::make_nvp("operating_object", operating_object),
               cereal::make_nvp("reason", reason),
               cereal::make_nvp("operate_person", operate_person),
               cereal::make_nvp("delta_score", delta_score),
               cereal::make_nvp("erased", erased));

            event_time = std::chrono::time_point<std::chrono::system_clock>(std::chrono::milliseconds(millis));
            event_type = static_cast<EventType>(et);
        }
    };
}
