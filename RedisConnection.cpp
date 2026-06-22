#include "RedisConnection.h"
#include <iostream>
#include <stdexcept>

RedisConnection& RedisConnection::GetInstance() {
    static RedisConnection instance;  // 싱글톤 초기화
    return instance;
}

void RedisConnection::Connect(const std::string& host, int port) {
    sw::redis::ConnectionOptions opts;
    opts.host = host;
    opts.port = port;
    opts.socket_timeout = std::chrono::seconds(10);
    opts.keep_alive = true;

    try {
        redis_ = std::make_unique<sw::redis::Redis>(opts);
        redis_->ping();
        std::cout << "[Redis] Connected to " << host << ":" << port << std::endl;
    }
    catch (const sw::redis::Error& e) {
        std::cerr << "[Redis] Connection failed: " << e.what() << std::endl;
        throw;  // 연결 실패하면 서버 시작 중단
    }
}

sw::redis::Redis& RedisConnection::GetRedis() {
    if (!redis_) {
        throw std::runtime_error("[Redis] Not connected. Call Connect() first.");
    }
    return *redis_;
}


void RedisConnection::SetUserEquip(uint32_t userId, const Equipment& equip) {
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