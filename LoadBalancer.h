#pragma once
#include <string>
#include <vector>
#include <random>
#include <iostream>
#include <algorithm>

#include "RedisManager.h"
#include "ServerChannelEnum.h"

struct LobbyServerInfo {
    int serverId;
    int userCount;
};

class LoadBalancer {
public:
    // 살아있는 로비 서버 중 최소 인원 서버 선택
    // 반환값: 선택된 서버 정보 (없으면 ip = "")
    ServerAddress SelectServer();

private:
    // 살아있는 서버 목록 조회 (TTL 살아있는 키만 조회)
    std::vector<LobbyServerInfo> GetAvailableServers();
    

    // 서버별 인원수 확인 레디스 키
    std::vector<std::string> serverUserCntkeys = {
        "lobby:1:status",
        "lobby:2:status"
    };
};