#pragma once

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <sstream>
#include <iostream>

class User
{
public:
	int id;
	std::string username;
	std::string password;

	User() = default;

	User(int id, std::string username, std::string password) : id(id), username(username), password(password) {}

	std::string serializeUserToBinary(const User& user)
	{
		std::ostringstream os;
		cereal::BinaryOutputArchive archive(os);
		archive(user);
		return os.str();
	}

	User deserializeUserFromBinary(const std::string& data)
	{
		std::istringstream is(data);
		cereal::BinaryInputArchive archive(is);
		User user;
		archive(user);
		return user;
	}

	// serialization function
	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(id, username, password);
	}
};
