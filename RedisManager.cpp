#include "RedisManager.h"

RedisManager& RedisManager::GetInstance() {
    static RedisManager instance;  // 싱글톤 초기화
    return instance;
}

//void RedisManager::Connect(const std::string& host, int port) {
//    sw::redis::ConnectionOptions opts;
//    opts.host = host;
//    opts.port = port;
//    opts.socket_timeout = std::chrono::seconds(10);
//    opts.keep_alive = true;
//
//    try {
//        redis = std::make_unique<sw::redis::Redis>(opts);
//        redis->ping();
//        std::cout << "[Redis] Connected to " << host << ":" << port << std::endl;
//    }
//    catch (const sw::redis::Error& e) {
//        std::cerr << "[Redis] Connection failed: " << e.what() << std::endl;
//        throw;  // 연결 실패하면 서버 시작 중단
//    }
//}

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


    RedisRun(RedisThreadCnt_);
}

// ===================== PACKET MANAGEMENT =====================

void RedisManager::PushRedisPacket(const uint16_t connObjNum_, const uint32_t size_, char* recvData_) {
    ConnUser* TempConnUser = connUsersManager->FindUser(connObjNum_);
    TempConnUser->WriteRecvData(recvData_, size_); // Push Data in Circualr Buffer
    DataPacket tempD(size_, connObjNum_);
    procSktQueue.push(tempD);
}


// ==================== CONNECTION INTERFACE ===================

void RedisManager::Disconnect(uint16_t connObjNum_) {
    if (connUsersManager->FindUser(connObjNum_)->GetPk() == 0) return; // Check the server closed
    
    auto tempUser = inGameUserManager->GetInGameUserByObjNum(connObjNum_);
    auto tempPk = tempUser->GetPk();

    std::string tag = "{" + std::to_string(tempPk) + "}";
    std::string userInfokey = "userinfo:" + tag;
    std::string equipmentkey = "equipment:" + tag;
    std::string consumablekey = "consumables:" + tag;
    std::string materialkey = "materials:" + tag;

    try {
        auto pipe = redis->pipeline(tag);

        redis->hset(userInfokey, "userstate", "offline"); // Set user status to "offline" in Redis Cluster
        redis->expire(equipmentkey, std::chrono::seconds(180)); // ttl 3분 설정
        redis->expire(consumablekey, std::chrono::seconds(180)); // ttl 3분 설정
        redis->expire(materialkey, std::chrono::seconds(180)); // ttl 3분 설정

        pipe.exec();
    }
    catch (const sw::redis::Error& e) {
        std::cerr << "Redis error : " << e.what() << std::endl;
        return;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception error : " << e.what() << std::endl;
    }

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
    catch (const  sw::redis::Error& err) {
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

    if (loginResult.has_value()) {
        ServerType serverType;
        loginRes.isSuccess = ProcessConnect(loginRes, inventoryRes, loginResult.value(), serverType);

        if (loginRes.isSuccess) { // 성공했을 때만 레디스 세팅하기
            SetUserCostume(loginResult.value(), loginRes.costume);
            SetUserLocation(loginResult.value(), serverType);
        }
    } 

    auto user = connUsersManager->FindUser(connObjNum_);
    user->PushSendMsg(sizeof(USER_LOGIN_RESPONSE), (char*)&loginRes);

    // 인벤은 성공 시에만 전송
    if (loginRes.isSuccess) {
        user->PushSendMsg(sizeof(USER_INVENTORY_PACKET), (char*)&inventoryRes);
    }
}

bool RedisManager::ProcessConnect(USER_LOGIN_RESPONSE& loginRes, USER_INVENTORY_PACKET& inventoryRes, uint32_t userPk_, ServerType& serverType_) {

    // 유저 정보 불러와서 전달해주기 (DB)
    auto tempUserInfo = MySQLManager::GetInstance().GetUserInfo(userPk_);
    if (tempUserInfo == std::nullopt) {
        std::cerr << "[ProcessConnect] GetUserInfo failed. userPk: " << userPk_ << '\n';
        return false;
    }
    loginRes.userinfo = tempUserInfo.value();


    // 유저 화폐 불러와서 전달 (DB)
    auto tempUserCurrency = MySQLManager::GetInstance().GetUserCurrency(userPk_);
    if (tempUserCurrency == std::nullopt) {
        std::cerr << "[ProcessConnect] GetUserCurrency failed. userPk: " << userPk_ << '\n';
        return false;
    }
    loginRes.currency = tempUserCurrency.value();


    // 현재 착용중인 코스튬 불러와서 전달 (DB) + 불러온 데이터 레디스에 올려두기 (Redis)
    auto tempUserCostume = MySQLManager::GetInstance().GetUserCostume(userPk_);
    if (tempUserCostume == std::nullopt) {
        std::cerr << "[ProcessConnect] GetUserCostume failed. userPk: " << userPk_ << '\n';
        return false;
    }
    loginRes.costume = tempUserCostume.value();


    // 유저 인벤토리 불러와서 전달 (DB)
    auto tempUserInventory = MySQLManager::GetInstance().GetUserInventory(userPk_);
    if (tempUserInventory == std::nullopt) {
        std::cerr << "[ProcessConnect] GetUserInventory failed. userPk: " << userPk_ << '\n';
        return false;
    }
    auto& inventoryVec = tempUserInventory.value();
    inventoryRes.itemCount = static_cast<uint16_t>(inventoryVec.size()); // 아이템 수 설정
    for (int i = 0; i < (int)inventoryVec.size(); i++) {
        inventoryRes.items[i] = inventoryVec[i];
    }


    // 유저 서버 로드 밸런싱 후 해당 서버 정보 전달 + 서버 정보 레디스에 올려두기 (Redis)
    auto tempServer = loadBalancer.SelectServer();
    if (tempServer.ip == "") {
        std::cerr << "[ProcessConnect] No available lobby servers. userPk: "
            << userPk_ << '\n';
        return false;
    }
    strncpy_s(loginRes.ip, sizeof(loginRes.ip), tempServer.ip, _TRUNCATE);
    loginRes.port = tempServer.port;
    serverType_ = tempServer.serverType;


    // JWT 토큰 생성 후 레디스에 올리기 (Redis)
    

    return true;
}