#include "RedisManager.h"

RedisManager& RedisManager::GetInstance() {
    static RedisManager instance;  // 싱글톤 초기화
    return instance;
}

sw::redis::Redis& RedisManager::GetRedis() {
    if (!redis) {
        throw std::runtime_error("[Redis] Not connected. Call Connect() first.");
    }
    return *redis;
}


void RedisManager::Init(const uint16_t RedisThreadCnt_) {

    // -------------------- SET PACKET HANDLERS ----------------------
    packetIDTable = std::unordered_map<uint16_t, RECV_PACKET_FUNCTION>();

    // LOGIN
    packetIDTable[(uint16_t)PACKET_ID::USER_LOGIN_REQUEST] = &RedisManager::ProcessLogin;


    RedisRun(RedisThreadCnt_);
}

// ===================== PACKET MANAGEMENT =====================

void RedisManager::PushRedisPacket(const uint16_t connObjNum_, const uint32_t size_, char* recvData_) {
    ConnUser* TempConnUser = connUsersManager->FindUser(connObjNum_);
    TempConnUser->WriteRecvData(recvData_, size_); // Push Data in Circualr Buffer
    DataPacket tempD(size_, connObjNum_);
    procSktQueue.push(tempD);
}


// ====================== REDIS MANAGEMENT =====================

void RedisManager::RedisRun(const uint16_t RedisThreadCnt_) { // Connect Redis Server
    try {
        sw::redis::ConnectionOptions opts;
        opts.host = host;
        opts.port = port;
        opts.socket_timeout = std::chrono::seconds(10);
        opts.keep_alive = true;

        redis = std::make_unique<sw::redis::Redis>(opts);
        redis->ping();
        std::cout << "[Redis] Connected to " << host << ":" << port << std::endl;

        CreateRedisThread(RedisThreadCnt_);

    }
    catch (const sw::redis::Error& e) {
        std::cerr << "[Redis] Connection failed: " << e.what() << std::endl;
        throw;  // 연결 실패하면 서버 시작 중단
    }
}

bool RedisManager::CreateRedisThread(const uint16_t RedisThreadCnt_) {
    redisRun = true;

    try {
        for (int i = 0; i < RedisThreadCnt_; i++) {
            redisThreads.emplace_back(std::thread([this]() { RedisThread(); }));
        }
    }
    catch (const std::system_error& e) {
        std::cerr << "Create Redis Thread Failed : " << e.what() << std::endl;
        return false;
    }

    return true;
}

void RedisManager::RedisThread() {
    DataPacket tempD(0, 0);
    ConnUser* TempConnUser = nullptr;
    char tempData[1024] = { 0 };

    while (redisRun) {
        if (procSktQueue.pop(tempD)) {
            std::memset(tempData, 0, sizeof(tempData));
            TempConnUser = connUsersManager->FindUser(tempD.connObjNum); // Find User
            PacketInfo packetInfo = TempConnUser->ReadRecvData(tempData, tempD.dataSize); // GetData
            (this->*packetIDTable[packetInfo.packetId])(packetInfo.connObjNum, packetInfo.dataSize, packetInfo.pData); // Proccess Packet
        }
        else { // Empty Queue
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}


void RedisManager::SetUserCostume(uint32_t userpk_, const Costume& costume_) {
    try {
        std::unordered_map<std::string, std::string> fields = { // 여러 필드 hset으로 한 번에 세팅
            {"head", std::to_string(costume_.head)},
            {"body", std::to_string(costume_.body)},
            {"legs", std::to_string(costume_.legs)},
            {"feet", std::to_string(costume_.feet)}
        };
        std::string key = "user:" + std::to_string(userpk_) + ":costume";
        redis->hset(key, fields.begin(), fields.end());
    }
    catch (const std::exception& e) {
        std::cerr << "[SetUserCostume] Redis error. userPk: " << userpk_ << " / " << e.what() << '\n';
    }
}

void RedisManager::SetUserLocation(uint32_t userPk_, ServerType serverType_) {
    std::string key = "user:" + std::to_string(userPk_);

    try {
        std::unordered_map<std::string, std::string> fields = {
            {"server", GetServerName(serverType_)},  // enum 숫자 대신 "LobbyServer01"로 명시적으로 저장하기
            {"state",  "lobby"}
        };

        redis->hset(key, fields.begin(), fields.end());
        redis->expire(key, std::chrono::seconds(300));

        std::cout << "[SetUserLocation] userPk: " << userPk_
            << " / " << GetServerName(serverType_) << '\n';
    }
    catch (const std::exception& e) {
        std::cerr << "[SetUserLocation] Redis error. userPk: "
            << userPk_ << " / " << e.what() << '\n';
    }
}

// 토큰 생성 + Redis 저장
bool RedisManager::SetUserToken(const std::string& userId_, uint32_t userPk_, char* tokenOut_, size_t tokenSize_) {
    try {
        // JWT 생성
        std::string token = jwt::create()
            .set_issuer("LoginServer")
            .set_subject("LobbyAccess")
            .set_expires_at(std::chrono::system_clock::now() +
                std::chrono::seconds{ 300 })
            .sign(jwt::algorithm::hs256{ JWT_SECRET });

        // Redis 
        // key: "jwtcheck:{userId}"
        // field: token, value: userPk
        std::string key = "jwtcheck:{" + userId_ + "}";
        auto pipe = redis->pipeline();
        pipe.hset(key, token, std::to_string(userPk_))
            .expire(key, 300);
        pipe.exec();

        strncpy_s(tokenOut_, tokenSize_, token.c_str(), _TRUNCATE);

        std::cout << "[SetUserToken] Token issued. userId: " << userId_ << '\n';
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "[SetUserToken] Exception: " << e.what() << '\n';
        return false;
    }
}


// ====================== UserState =======================

void RedisManager::ProcessLogin(uint16_t connObjNum_, uint16_t packetSize_, char* pPacket_) {
    auto loginReqPacket = reinterpret_cast<USER_LOGIN_REQUEST*>(pPacket_);

    auto loginResult = MySQLManager::GetInstance().LoginCheck(std::string(loginReqPacket->userId), std::string(loginReqPacket->userPassword));

    USER_LOGIN_RESPONSE loginRes;
    loginRes.PacketId = (uint16_t)PACKET_ID::USER_LOGIN_RESPONSE;
    loginRes.PacketLength = sizeof(USER_LOGIN_RESPONSE);

    USER_INVENTORY_PACKET inventoryRes;
    inventoryRes.PacketId = (uint16_t)PACKET_ID::USER_INVENTORY_PACKET;
    inventoryRes.PacketLength = sizeof(USER_INVENTORY_PACKET);

    USER_FRIEND_PACKET friendRes;
    friendRes.PacketId = (uint16_t)PACKET_ID::USER_FRIEND_PACKET;
    friendRes.PacketLength = sizeof(USER_FRIEND_PACKET);

    if (loginResult.has_value()) {
        ServerType serverType;
        loginRes.isSuccess = ProcessConnect(loginRes, inventoryRes, friendRes, loginResult.value(), serverType);

        if (loginRes.isSuccess) { // 성공했을 때만 레디스 세팅하기
            SetUserCostume(loginResult.value(), loginRes.costume);
            SetUserLocation(loginResult.value(), serverType);

            // JWT 토큰 발급 실패 시 로그인 실패 처리
            if (!SetUserToken(loginReqPacket->userId, loginResult.value(), loginRes.token, sizeof(loginRes.token))) {
                loginRes.isSuccess = false;
            }
        }
    } 
    else {
        // LoginCheck가 nullopt이면 비번 틀리거나 유저 없음
        loginRes.failCode = (uint8_t)LoginFailCode::WrongPassword;
    }

    auto user = connUsersManager->FindUser(connObjNum_);
    user->PushSendMsg(sizeof(USER_LOGIN_RESPONSE), (char*)&loginRes);

    // 인벤과 친구는 성공 시에만 전송
    if (loginRes.isSuccess) {
        user->PushSendMsg(sizeof(USER_INVENTORY_PACKET), (char*)&inventoryRes);
        user->PushSendMsg(sizeof(USER_FRIEND_PACKET), (char*)&friendRes);
    }
}

bool RedisManager::ProcessConnect(USER_LOGIN_RESPONSE& loginRes, USER_INVENTORY_PACKET& inventoryRes, USER_FRIEND_PACKET& friendRes, uint32_t userPk_, ServerType& serverType_) {

    // 유저 정보 불러와서 전달해주기 (DB)
    auto tempUserInfo = MySQLManager::GetInstance().GetUserInfo(userPk_);
    if (tempUserInfo == std::nullopt) {
        std::cerr << "[ProcessConnect] GetUserInfo failed. userPk: " << userPk_ << '\n';
        loginRes.failCode = (uint8_t)LoginFailCode::ServerError;
        return false;
    }
    loginRes.userinfo = tempUserInfo.value();


    // 유저 화폐 불러와서 전달 (DB)
    auto tempUserCurrency = MySQLManager::GetInstance().GetUserCurrency(userPk_);
    if (tempUserCurrency == std::nullopt) {
        std::cerr << "[ProcessConnect] GetUserCurrency failed. userPk: " << userPk_ << '\n';
        loginRes.failCode = (uint8_t)LoginFailCode::ServerError;
        return false;
    }
    loginRes.currency = tempUserCurrency.value();


    // 현재 착용중인 코스튬 불러와서 전달 (DB) + 불러온 데이터 레디스에 올려두기 (Redis)
    auto tempUserCostume = MySQLManager::GetInstance().GetUserCostume(userPk_);
    if (tempUserCostume == std::nullopt) {
        std::cerr << "[ProcessConnect] GetUserCostume failed. userPk: " << userPk_ << '\n';
        loginRes.failCode = (uint8_t)LoginFailCode::ServerError;
        return false;
    }
    loginRes.costume = tempUserCostume.value();


    // 유저 인벤토리 불러와서 전달 (DB)
    auto tempUserInventory = MySQLManager::GetInstance().GetUserInventory(userPk_);
    if (tempUserInventory == std::nullopt) {
        std::cerr << "[ProcessConnect] GetUserInventory failed. userPk: " << userPk_ << '\n';
        loginRes.failCode = (uint8_t)LoginFailCode::ServerError;
        return false;
    }
    auto& inventoryVec = tempUserInventory.value();
    inventoryRes.itemCount = static_cast<uint16_t>(inventoryVec.size()); // 아이템 수 설정
    for (int i = 0; i < (int)inventoryVec.size(); i++) {
        inventoryRes.items[i] = inventoryVec[i];
    }


    // 친구 목록 불러와서 전달 (DB) + 불러온 pk 기반 친구 접속 여부 확인
    auto tempFriendsDB = MySQLManager::GetInstance().GetUserFriendsDB(userPk_);
    if (!tempFriendsDB.has_value()) {
        std::cerr << "[ProcessConnect] GetUserFriendsDB failed. userPk: " << userPk_ << '\n';
        loginRes.failCode = (uint8_t)LoginFailCode::ServerError;
        return false;
    }

    // Redis에서 접속 상태 붙여서 변환
    auto friendList = BuildFriendList(tempFriendsDB.value());
    friendRes.friendCount = static_cast<uint16_t>(friendList.size());
    for (int i = 0; i < (int)friendList.size(); i++) {
        friendRes.friends[i] = friendList[i];
    }


    // 유저 서버 로드 밸런싱 후 해당 서버 정보 전달 + 서버 정보 레디스에 올려두기 (Redis)
    auto tempServer = loadBalancer.SelectServer();
    if (tempServer.ip == "") {
        std::cerr << "[ProcessConnect] No available lobby servers. userPk: " << userPk_ << '\n';
        loginRes.failCode = (uint8_t)LoginFailCode::NoServer;
        return false;
    }
    strncpy_s(loginRes.ip, sizeof(loginRes.ip), tempServer.ip, _TRUNCATE);
    loginRes.port = tempServer.port;
    serverType_ = tempServer.serverType;

    return true;
}

std::vector<FriendInfo> RedisManager::BuildFriendList(const std::vector<FriendInfoDB>& dbFriends_) {
    std::vector<FriendInfo> result;
    if (dbFriends_.empty()) return result;

    // 친구인 것들만 pipeline에 추가
    // 요청중(friendStatus=0)은 어차피 오프라인 취급이라 Redis 조회 불필요
    std::vector<int> friendIdx;  // pipeline에 넣은 친구의 인덱스
    auto pipe = redis->pipeline();

    for (int i = 0; i < (int)dbFriends_.size(); i++) {
        if (dbFriends_[i].friendStatus == 1) {  // 친구인 경우만
            std::string key = "user:" + std::to_string(dbFriends_[i].friendPk);
            pipe.hget(key, "state");
            friendIdx.push_back(i);
        }
    }

    // pipeline 한 번에 실행 후 Redis 왕복 1회로 전체 상태 조회
    // (친구 수만큼 개별 조회하면 N번 왕복, pipeline이면 1번)
    std::vector<sw::redis::OptionalString> states;
    try {
        auto replies = pipe.exec();
        for (int i = 0; i < (int)friendIdx.size(); i++) {
            try { // get<타입>(인덱스)로 하나씩 꺼내기
                states.push_back(replies.get<sw::redis::OptionalString>(i));
            }
            catch (...) {
                states.push_back(std::nullopt);
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[BuildFriendList] Pipeline Error: " << e.what() << '\n';
    }

    // DB 결과 + Redis 상태를 합쳐서 패킷용 FriendInfo로 변환
    // pk는 여기서 제외하기
    int pipeIdx = 0;
    for (int i = 0; i < (int)dbFriends_.size(); i++) {
        FriendInfo info;
        strncpy_s(info.friendId, sizeof(info.friendId), dbFriends_[i].friendId, _TRUNCATE);
        info.friendStatus = dbFriends_[i].friendStatus;
        info.onlineStatus = 0;  // 기본값은 오프라인

        // 요청중인 경우 onlineStatus는 0(오프라인)으로 그대로
        if (dbFriends_[i].friendStatus == 1) {
            // pipeline 결과에서 상태 꺼내기
            if (pipeIdx < (int)states.size() &&
                states[pipeIdx].has_value()) {
                const auto& state = *states[pipeIdx];
                if (state == "lobby")  info.onlineStatus = 1;
                else if (state == "ingame") info.onlineStatus = 2;
            }
            pipeIdx++;
        }

        result.push_back(info);
    }

    return result;
}