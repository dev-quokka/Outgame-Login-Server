#include <iostream>

#include "RedisConnection.h"
#include "MySQLConnectionPool.h"

int main() {
    RedisConnection::GetInstance().Connect("127.0.0.1", 6379); // 레디스 연결
    auto& redis = RedisConnection::GetInstance().GetRedis();

    MySQLConnectionPool m;
    m.init();

	return 0;
}