#pragma once
#include <cstdint>
#include <string>

struct Costume {
    uint32_t head = 0;
    uint32_t body = 0;
    uint32_t legs = 0;
    uint32_t feet = 0;
};

struct UserInfo {
    std::string userId;
    uint16_t level = 1;
    uint32_t exp = 0;
};

struct Currency {
    uint32_t bp = 0;
    uint32_t gcoin = 0;
};

struct InventoryItem {
    uint32_t itemCode = 0;
    uint8_t  itemType = 0;  // 1=ÄÚ½ºÆ¬, 2=ÃÑ½ºÅ², 3=Àç·á
    uint32_t quantity = 0;
};