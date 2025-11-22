#pragma once
#include"SSDBType.h"
#include<chrono>
#include<string>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>

namespace SSDB
{
	class Event
	{
	private:
		std::chrono::time_point<std::chrono::system_clock> event_time;
		int id; //事件本身的 ID
		EventType event_type;
		int operating_object; //事件操作的对象的 ID
		std::string reason;
		int operate_person; //操作者
		int delta_score; //分数变动情况
	public:
		// 构造函数
		Event(int _id,
			EventType _type,
			int _operating_object,
			const std::string& _reason,
			int _operate_person,
			int _delta_score,
			std::chrono::time_point<std::chrono::system_clock> _time = std::chrono::system_clock::now())
			: event_time(_time), id(_id), event_type(_type), operating_object(_operating_object), reason(_reason), operate_person(_operate_person), delta_score(_delta_score) {}

		// 默认构造函数
		Event() : event_time(std::chrono::system_clock::now()), id(0), event_type(EventType::STUDENT), operating_object(0), operate_person(0), delta_score(0) {}

		// Getters
		std::chrono::time_point<std::chrono::system_clock> GetEventTime() const { return event_time; }
		int GetId() const { return id; }
		EventType GetEventType() const { return event_type; }
		int GetOperatingObject() const { return operating_object; }
		const std::string& GetReason() const { return reason; }
		int GetOperatePerson() const { return operate_person; }
		int GetDeltaScore() const { return delta_score; }

		// Setters
		void SetEventTime(std::chrono::time_point<std::chrono::system_clock> t) { event_time = t; }
		void SetId(int _id) { id = _id; }
		void SetEventType(EventType t) { event_type = t; }
		void SetOperatingObject(int obj) { operating_object = obj; }
		void SetReason(const std::string& r) { reason = r; }
		void SetOperatePerson(int p) { operate_person = p; }
		void SetDeltaScore(int s) { delta_score = s; }

		// cereal 序列化
		template<class Archive>
		void serialize(Archive& ar)
		{
			// 将时间点转换为可序列化的整数 (自 epoch 的毫秒)
			auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(event_time.time_since_epoch()).count();
			ar(CEREAL_NVP(millis),
				CEREAL_NVP(id),
				CEREAL_NVP(event_type),
				CEREAL_NVP(operating_object),
				CEREAL_NVP(reason),
				CEREAL_NVP(operate_person),
				CEREAL_NVP(delta_score));
			// 反序列化后重建 event_time
			if(Archive::is_loading::value)
			{
				event_time = std::chrono::system_clock::time_point(std::chrono::milliseconds(millis));
			}
		}
	};
}