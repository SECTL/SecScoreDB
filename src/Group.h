#pragma once
#include<vector>
#include<map>
#include"SSDBType.h"
#include <cereal/cereal.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>

namespace SSDB
{
	class Group
	{
	private:
		int id;
		std::vector<int> containStudents;
		t_metadata metadata;
	public:
		// 构造函数
		Group(int _id, const std::vector<int>& students, const t_metadata& meta)
			: id(_id), containStudents(students), metadata(meta) {}

		// 默认构造函数
		Group() : id(0) {}

		// id Getter / Setter
		int GetId() const { return id; }
		void SetId(int _id) { id = _id; }

		// students Getter / Setter
		const std::vector<int>& GetStudents() const { return containStudents; }
		void SetStudents(const std::vector<int>& students) { containStudents = students; }
		void AddStudent(int studentId) { containStudents.push_back(studentId); }
		bool RemoveStudent(int studentId)
		{
			for (auto it = containStudents.begin(); it != containStudents.end(); ++it)
			{
				if (*it == studentId) { containStudents.erase(it); return true; }
			}
			return false;
		}

		// metadata Getter / Setter
		const t_metadata& GetMetadata() const { return metadata; }
		void SetMetadata(const t_metadata& meta) { metadata = meta; }
		void SetMetadataValue(const std::string& key, const std::string& value) { metadata[key] = value; }
		std::string GetMetadataValue(const std::string& key) const
		{
			auto it = metadata.find(key);
			return it != metadata.end() ? it->second : std::string();
		}

		// cereal 序列化
		template<class Archive>
		void serialize(Archive& ar)
		{
			ar(
				CEREAL_NVP(id),
				CEREAL_NVP(containStudents),
				CEREAL_NVP(metadata)
			);
		}
	};
}