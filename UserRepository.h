#pragma once
#include "UserTypes.h"
#include <optional>

class UserRepository {
public:
    std::optional<UserInfo> GetUserInfo(uint32_t userPk);
    Equipment GetEquipment(uint32_t userPk);
};