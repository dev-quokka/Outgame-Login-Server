#include "EquipManager.h"

void SetUserState(uint32_t userId, const Equipment& equip) {
    // MySQL에서 불러온 equip로 함수 실행
    auto& redis = RedisConnection::GetInstance().GetRedis();

    // MySQL에서 읽어온 장비를 map으로 구성
    std::unordered_map<std::string, std::string> fields = {
        {"head", std::to_string(equip.head)},
        {"body", std::to_string(equip.body)},
        {"legs", std::to_string(equip.legs)},
        {"feet", std::to_string(equip.feet)}
    };

    // 여러 필드 hset으로 한 번에 세팅
    std::string key = "user:" + std::to_string(userId) + ":equip";
    redis.hset(key, fields.begin(), fields.end());
}