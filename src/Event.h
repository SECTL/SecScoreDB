/**
 * @file Event.h
 * @brief 事件实体定义
 */
#pragma once

#include "SSDBType.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>

// Cereal 序列化
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>

namespace SSDB
{
    /**
     * @brief 无效 ID 常量
     */
    inline constexpr int INVALID_ID = -1;

    /**
     * @brief 系统时钟时间点类型别名
     */
    using TimePoint = std::chrono::time_point<std::chrono::system_clock>;

    /**
     * @brief 事件实体类
     */
    class Event
    {
    private:
        TimePoint eventTime_ = std::chrono::system_clock::now();
        int id_ = INVALID_ID;
        EventType eventType_ = EventType::Student;
        int operatingObject_ = 0;
        std::string reason_;
        int operatePerson_ = 0;
        int deltaScore_ = 0;
        bool erased_ = false;

    public:
        // 默认构造函数
        Event() = default;

        // 带参构造函数（Sink 模式）
        Event(int id,
              EventType type,
              int operatingObject,
              std::string reason,
              int operatePerson,
              int deltaScore,
              TimePoint time = std::chrono::system_clock::now())
            : eventTime_(time)
            , id_(id)
            , eventType_(type)
            , operatingObject_(operatingObject)
            , reason_(std::move(reason))
            , operatePerson_(operatePerson)
            , deltaScore_(deltaScore)
        {
        }

        // Getters
        [[nodiscard]] TimePoint GetEventTime() const noexcept { return eventTime_; }
        [[nodiscard]] int GetId() const noexcept { return id_; }
        [[nodiscard]] EventType GetEventType() const noexcept { return eventType_; }
        [[nodiscard]] int GetOperatingObject() const noexcept { return operatingObject_; }
        [[nodiscard]] const std::string& GetReason() const noexcept { return reason_; }
        [[nodiscard]] int GetOperatePerson() const noexcept { return operatePerson_; }
        [[nodiscard]] int GetDeltaScore() const noexcept { return deltaScore_; }
        [[nodiscard]] bool IsErased() const noexcept { return erased_; }

        // Setters
        void SetEventTime(TimePoint time) noexcept { eventTime_ = time; }
        void SetId(int id) noexcept { id_ = id; }
        void SetEventType(EventType type) noexcept { eventType_ = type; }
        void SetOperatingObject(int obj) noexcept { operatingObject_ = obj; }
        void SetReason(std::string reason) { reason_ = std::move(reason); }
        void SetOperatePerson(int person) noexcept { operatePerson_ = person; }
        void SetDeltaScore(int score) noexcept { deltaScore_ = score; }
        void SetErased(bool erased) noexcept { erased_ = erased; }

        // Cereal 序列化
        template <class Archive>
        void save(Archive& ar) const
        {
            const auto millis = static_cast<std::int64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    eventTime_.time_since_epoch()
                ).count()
            );
            const auto et = static_cast<int>(eventType_);

            ar(cereal::make_nvp("millis", millis),
               cereal::make_nvp("id", id_),
               cereal::make_nvp("event_type", et),
               cereal::make_nvp("operating_object", operatingObject_),
               cereal::make_nvp("reason", reason_),
               cereal::make_nvp("operate_person", operatePerson_),
               cereal::make_nvp("delta_score", deltaScore_),
               cereal::make_nvp("erased", erased_));
        }

        template <class Archive>
        void load(Archive& ar)
        {
            std::int64_t millis = 0;
            int et = 0;

            ar(cereal::make_nvp("millis", millis),
               cereal::make_nvp("id", id_),
               cereal::make_nvp("event_type", et),
               cereal::make_nvp("operating_object", operatingObject_),
               cereal::make_nvp("reason", reason_),
               cereal::make_nvp("operate_person", operatePerson_),
               cereal::make_nvp("delta_score", deltaScore_),
               cereal::make_nvp("erased", erased_));

            eventTime_ = TimePoint(std::chrono::milliseconds(millis));
            eventType_ = static_cast<EventType>(et);
        }
    };
}
