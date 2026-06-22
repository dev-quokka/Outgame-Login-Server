#include <iostream>

#include "RedisConnection.h"
#include "MySQLConnectionPool.h"

int main() {
    RedisConnection::GetInstance().Connect("127.0.0.1", 6379); // 레디스 연결
    auto& redis = RedisConnection::GetInstance().GetRedis();

    bool m = MySQLConnectionPool::GetInstance().init();
    if (!m) {
        return 0;
    }


	return 0;
}