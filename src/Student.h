#pragma once
#include<string>
#include<vector>
#include<map>
#include"SSDBType.h"
// cereal 序列化所需头
#include <cereal/cereal.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>

namespace SSDB
{
	class Student
	{
	private:
		int id;
		std::vector<int> GroupBelong;
		t_metadata metadata;
	public:
		// 默认构造函数 (cereal 反序列化需要)
		Student() : id(0) {}

		// 构造函数
		Student(int _id, const std::vector<int>& Group, const t_metadata& meta)
		{
			this->id = _id;
			this->GroupBelong = Group;
			this->metadata = meta;
		}

		// Getter / Setter for id
		int GetId() const { return id; }
		void SetId(int _id) { id = _id; }

		// Getter / Setter for groups
		const std::vector<int>& GetGroups() const { return GroupBelong; }
		void SetGroups(const std::vector<int>& groups) { GroupBelong = groups; }
		void AddGroup(int groupId) { GroupBelong.push_back(groupId); }

		// Getter / Setter for metadata
		const t_metadata& GetMetadata() const { return metadata; }
		void SetMetadata(const t_metadata& meta) { metadata = meta; }
		void SetMetadataValue(const std::string& key, const std::string& value) { metadata[key] = value; }
		std::string GetMetadataValue(const std::string& key) const {
			auto it = metadata.find(key);
			return it != metadata.end() ? it->second : std::string();
		}

		// cereal 序列化
		template<class Archive>
		void serialize(Archive& ar)
		{
			ar(
				CEREAL_NVP(id),
				CEREAL_NVP(GroupBelong),
				CEREAL_NVP(metadata)
			);
		}
	};
}

