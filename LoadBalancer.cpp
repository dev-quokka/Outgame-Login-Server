#include "LoadBalancer.h"

std::vector<LobbyServerInfo> LoadBalancer::GetAvailableServers() {
    auto& redis = RedisManager::GetInstance().GetRedis();

    std::vector<sw::redis::OptionalString> values;
    redis.mget(serverUserCntkeys.begin(), serverUserCntkeys.end(), std::back_inserter(values));

    std::vector<LobbyServerInfo> available;
    for (int i = 0; i < (int)serverUserCntkeys.size(); ++i) {
        if (!values[i].has_value()) {
            // 값 없음(해당 서버 다운 or heartbeat 끊김)
            // 서버 종료시키지 말고 해당 서버만 배정 대상에서 자동으로 제외시키기
            std::cout << "[Balancer] Server " << (i + 1) << " is down or unreachable\n";
            continue;
        }

        LobbyServerInfo info;
        info.userCount = std::stoi(*values[i]);
        info.serverId = i + 1;
        available.push_back(info);
    }

    return available;
}

ReturnServerAddress LoadBalancer::SelectServer() {
    auto servers = GetAvailableServers();

    // 살아있는 서버가 하나도 없으면
    if (servers.empty()) {
        std::cerr << "[Balancer] No available lobby servers!\n";
        return { "", 0};
    }

    // 인원수 오름차순 정렬
    std::sort(servers.begin(), servers.end(),
        [](const LobbyServerInfo& a, const LobbyServerInfo& b) {
            return a.userCount < b.userCount;
        });

    LobbyServerInfo& result = *servers.begin();
    std::cout << "[Balancer] Selected server " << result.serverId
        << " (users: " << result.userCount << ")\n";

    ReturnServerAddress sAddr;
    if (result.serverId == 1) {
        strncpy_s(sAddr.ip, sizeof(sAddr.ip), ServerAddressMap[ServerType::LobbyServer01].ip, _TRUNCATE);
        sAddr.port = ServerAddressMap[ServerType::LobbyServer01].port;
        sAddr.serverType = ServerType::LobbyServer01;
    }
    else if (result.serverId == 2) {
        strncpy_s(sAddr.ip, sizeof(sAddr.ip), ServerAddressMap[ServerType::LobbyServer02].ip, _TRUNCATE);
        sAddr.port = ServerAddressMap[ServerType::LobbyServer02].port;
        sAddr.serverType = ServerType::LobbyServer02;
    }

    // 서버 중 현재 인원 수 가장 적은 서버 주소 반환
    return sAddr;
}