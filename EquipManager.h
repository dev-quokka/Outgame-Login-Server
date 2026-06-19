#pragma once
#include <unordered_map>

#include "RedisConnection.h"
#include "UserTypes.h"

class UserStateManager {
public:
	void SetUserEquip(uint32_t userId, const Equipment& equip);


private:

};