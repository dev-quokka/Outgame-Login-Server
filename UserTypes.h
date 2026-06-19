#pragma once
#include <cstdint>
#include <string>

struct Equipment {
    uint32_t head = 0;
    uint32_t body = 0;
    uint32_t legs = 0;
    uint32_t feet = 0;
};

struct UserInfo {
    uint32_t userPk = 0;
    std::string userId;
    uint16_t level = 1;
    uint32_t exp = 0;
};