#include "MySQLManager.h"

MySQLManager& MySQLManager::GetInstance() {
    static MySQLManager instance;
    return instance;
}

MYSQL* MySQLManager::GetConnection() {
    MYSQL* ConnPtr;
    {
        std::lock_guard<std::mutex> lock(dbPoolMutex);
        if (dbPool.empty()) {
            return nullptr;
        }
        ConnPtr = dbPool.front();
        dbPool.pop();
    }
    return ConnPtr;
};

bool MySQLManager::init() {
    for (int i = 0; i < dbConnectionCount; i++) {
        MYSQL* Conn = mysql_init(nullptr);
        if (!Conn) {
            std::cerr << "Mysql Init Fail" << std::endl;
            return false;
        }

        MYSQL* ConnPtr = mysql_real_connect(Conn, DB_HOST, DB_USER, DB_PASSWORD, DB_NAME, DB_PORT, (char*)NULL, 0);
        if (!ConnPtr) {
            std::cerr << "Mysql Connection Fail : " << mysql_error(Conn) << '\n';
            return false;
        }

        dbPool.push(ConnPtr);
    }

    std::cout << "Mysql Connection Success" << std::endl;
    return true;
}

void MySQLManager::Shutdown() {
    while (!dbPool.empty()) {
        MYSQL* conn = dbPool.front();
        dbPool.pop();
        mysql_close(conn);
    }
}




std::optional<uint32_t> MySQLManager::LoginCheck(const std::string& id, const std::string& password) {
    semaphore.acquire();

    MYSQL* ConnPtr = GetConnection();
    if (!ConnPtr) {
        std::cerr << "[LoginCheck] dbPool is empty.\n";
        return std::nullopt;
    }
    auto tempAutoConn = AutoConn(ConnPtr, dbPool, dbPoolMutex, semaphore);

    try {
        MYSQL_STMT* stmt = mysql_stmt_init(ConnPtr);

        std::string query = "SELECT user_pk, user_password, salt FROM user WHERE user_id = ?";

        if (mysql_stmt_prepare(stmt, query.c_str(), query.length()) != 0) {
            std::cerr << "[LoginCheck] Prepare Error: "
                << mysql_stmt_error(stmt) << '\n';
            mysql_stmt_close(stmt);
            return std::nullopt;
        }

        MYSQL_BIND bind[1];
        memset(bind, 0, sizeof(bind));
        unsigned long idLen = id.length();
        bind[0].buffer_type = MYSQL_TYPE_STRING;
        bind[0].buffer = (void*)id.c_str();
        bind[0].buffer_length = id.length();
        bind[0].length = &idLen;

        if (mysql_stmt_bind_param(stmt, bind) != 0) {
            std::cerr << "[LoginCheck] Bind Error: " << mysql_stmt_error(stmt) << '\n';
            mysql_stmt_close(stmt);
            return std::nullopt;
        }

        if (mysql_stmt_execute(stmt) != 0) {
            std::cerr << "[LoginCheck] Execute Error: " << mysql_stmt_error(stmt) << '\n';
            mysql_stmt_close(stmt);
            return std::nullopt;
        }

        // 결과 받는 부분
        uint32_t      userPk = 0;
        char          storedHash[65] = {};
        char          salt[65] = {};
        unsigned long hashLen = 0, saltLen = 0;

        MYSQL_BIND result[3];
        memset(result, 0, sizeof(result));

        result[0].buffer_type = MYSQL_TYPE_LONG;
        result[0].buffer = &userPk;
        result[0].is_unsigned = true;

        result[1].buffer_type = MYSQL_TYPE_STRING;
        result[1].buffer = storedHash;
        result[1].buffer_length = sizeof(storedHash);
        result[1].length = &hashLen;

        result[2].buffer_type = MYSQL_TYPE_STRING;
        result[2].buffer = salt;
        result[2].buffer_length = sizeof(salt);
        result[2].length = &saltLen;

        mysql_stmt_bind_result(stmt, result);
        mysql_stmt_store_result(stmt);

        if (mysql_stmt_fetch(stmt) != 0) { // 일치하는 유저 없음
            mysql_stmt_close(stmt);
            return std::nullopt;
        }

        mysql_stmt_close(stmt);

        // 비번 + salt를 SHA256로 비교
        std::string inputHash = SHA256Hash(password + std::string(salt, saltLen));
        if (inputHash != std::string(storedHash, hashLen)) {
            return std::nullopt;  // 비번 불일치
        }

        return userPk;  // 성공하면 pk 반환
    }
    catch (const std::exception& e) {
        std::cerr << "[LoginCheck] Exception: " << e.what() << '\n';
        return std::nullopt;
    }
}



std::optional<UserInfo> MySQLManager::GetUserInfo(uint32_t userPk_) {
    semaphore.acquire();

    MYSQL* ConnPtr = GetConnection();
    if (!ConnPtr) {
        std::cerr << "[GetUserInfo] dbPool is empty.\n";
        return std::nullopt;
    }
    auto tempAutoConn = AutoConn(ConnPtr, dbPool, dbPoolMutex, semaphore);

    try {
        MYSQL_STMT* stmt = mysql_stmt_init(ConnPtr);
        std::string query =
            "SELECT user_id, user_level, user_exp FROM user WHERE user_pk = ?";

        if (mysql_stmt_prepare(stmt, query.c_str(), query.length()) != 0) {
            std::cerr << "[GetUserInfo] Prepare Error: "<< mysql_stmt_error(stmt) << '\n';
            mysql_stmt_close(stmt);
            return std::nullopt;
        }

        // 입력 바인딩
        MYSQL_BIND param[1];
        memset(param, 0, sizeof(param));
        param[0].buffer_type = MYSQL_TYPE_LONG;
        param[0].buffer = &userPk_;
        param[0].is_unsigned = true;

        if (mysql_stmt_bind_param(stmt, param) != 0) {
            std::cerr << "[GetUserInfo] Bind Error: " << mysql_stmt_error(stmt) << '\n';
            mysql_stmt_close(stmt);
            return std::nullopt;
        }

        if (mysql_stmt_execute(stmt) != 0) {
            std::cerr << "[GetUserInfo] Execute Error: " << mysql_stmt_error(stmt) << '\n';
            mysql_stmt_close(stmt);
            return std::nullopt;
        }

        // 결과 받는 부분
        char     userId[21] = {};
        uint16_t level = 0;
        uint32_t exp = 0;
        unsigned long userIdLen = 0;

        MYSQL_BIND result[3];
        memset(result, 0, sizeof(result));

        result[0].buffer_type = MYSQL_TYPE_STRING;
        result[0].buffer = userId;
        result[0].buffer_length = sizeof(userId);
        result[0].length = &userIdLen;

        result[1].buffer_type = MYSQL_TYPE_SHORT;
        result[1].buffer = &level;
        result[1].is_unsigned = true;

        result[2].buffer_type = MYSQL_TYPE_LONG;
        result[2].buffer = &exp;
        result[2].is_unsigned = true;

        mysql_stmt_bind_result(stmt, result);
        mysql_stmt_store_result(stmt);

        if (mysql_stmt_fetch(stmt) != 0) { // 일치하는 유저 없음
            mysql_stmt_close(stmt);
            return std::nullopt;
        }

        mysql_stmt_close(stmt);

        UserInfo info;
        info.userId = std::string(userId, userIdLen);
        info.level = level;
        info.exp = exp;
        return info;
    }
    catch (const std::exception& e) {
        std::cerr << "[GetUserInfo] Exception: " << e.what() << '\n';
        return std::nullopt;
    }
}

std::optional<Costume> MySQLManager::GetUserCostume(uint32_t userPk_) {
    semaphore.acquire();

    MYSQL* ConnPtr = GetConnection();
    if (!ConnPtr) {
        std::cerr << "[GetUserCostume] dbPool is empty.\n";
        return std::nullopt;
    }
    auto tempAutoConn = AutoConn(ConnPtr, dbPool, dbPoolMutex, semaphore);

    try {
        MYSQL_STMT* stmt = mysql_stmt_init(ConnPtr);
        std::string query =
            "SELECT slot_type, item_code FROM user_equip_slot WHERE user_pk = ?";

        if (mysql_stmt_prepare(stmt, query.c_str(), query.length()) != 0) {
            std::cerr << "[GetUserCostume] Prepare Error: " << mysql_stmt_error(stmt) << '\n';
            mysql_stmt_close(stmt);
            return std::nullopt;
        }

        // 입력 바인딩 (userPk)
        MYSQL_BIND param[1];
        memset(param, 0, sizeof(param));
        param[0].buffer_type = MYSQL_TYPE_LONG;
        param[0].buffer = &userPk_;
        param[0].is_unsigned = true;

        if (mysql_stmt_bind_param(stmt, param) != 0) {
            std::cerr << "[GetUserCostume] Bind Error: " << mysql_stmt_error(stmt) << '\n';
            mysql_stmt_close(stmt);
            return std::nullopt;
        }

        if (mysql_stmt_execute(stmt) != 0) {
            std::cerr << "[GetUserCostume] Execute Error: " << mysql_stmt_error(stmt) << '\n';
            mysql_stmt_close(stmt);
            return std::nullopt;
        }

        // 결과 바인딩
        uint8_t  slotType = 0;
        uint32_t itemCode = 0;

        MYSQL_BIND result[2];
        memset(result, 0, sizeof(result));

        result[0].buffer_type = MYSQL_TYPE_TINY;
        result[0].buffer = &slotType;
        result[0].is_unsigned = true;

        result[1].buffer_type = MYSQL_TYPE_LONG;
        result[1].buffer = &itemCode;
        result[1].is_unsigned = true;

        mysql_stmt_bind_result(stmt, result);
        mysql_stmt_store_result(stmt);

        Costume costume;
        while (mysql_stmt_fetch(stmt) == 0) {
            switch (slotType) {
            case 1: costume.head = itemCode; break;
            case 2: costume.body = itemCode; break;
            case 3: costume.legs = itemCode; break;
            case 4: costume.feet = itemCode; break;
            }
        }

        mysql_stmt_close(stmt);
        return costume;
    }
    catch (const std::exception& e) {
        std::cerr << "[GetUserCostume] Exception: " << e.what() << '\n';
        return std::nullopt;
    }
}

std::optional<Currency> MySQLManager::GetUserCurrency(uint32_t userPk_) {
    semaphore.acquire();

    MYSQL* ConnPtr = GetConnection();
    if (!ConnPtr) {
        std::cerr << "[GetUserCurrency] dbPool is empty.\n";
        return std::nullopt;
    }
    auto tempAutoConn = AutoConn(ConnPtr, dbPool, dbPoolMutex, semaphore);

    try {
        MYSQL_STMT* stmt = mysql_stmt_init(ConnPtr);
        std::string query =
            "SELECT bp, gcoin FROM user_currency WHERE user_pk = ?";

        if (mysql_stmt_prepare(stmt, query.c_str(), query.length()) != 0) {
            std::cerr << "[GetUserCurrency] Prepare Error: " << mysql_stmt_error(stmt) << '\n';
            mysql_stmt_close(stmt);
            return std::nullopt;
        }

        // 입력 바인딩
        MYSQL_BIND param[1];
        memset(param, 0, sizeof(param));
        param[0].buffer_type = MYSQL_TYPE_LONG;
        param[0].buffer = &userPk_;
        param[0].is_unsigned = true;

        if (mysql_stmt_bind_param(stmt, param) != 0) {
            std::cerr << "[GetUserCurrency] Bind Error: " << mysql_stmt_error(stmt) << '\n';
            mysql_stmt_close(stmt);
            return std::nullopt;
        }

        if (mysql_stmt_execute(stmt) != 0) {
            std::cerr << "[GetUserCurrency] Execute Error: " << mysql_stmt_error(stmt) << '\n';
            mysql_stmt_close(stmt);
            return std::nullopt;
        }

        // 결과 바인딩
        uint32_t bp = 0;
        uint32_t gcoin = 0;

        MYSQL_BIND result[2];
        memset(result, 0, sizeof(result));

        result[0].buffer_type = MYSQL_TYPE_LONG;
        result[0].buffer = &bp;
        result[0].is_unsigned = true;

        result[1].buffer_type = MYSQL_TYPE_LONG;
        result[1].buffer = &gcoin;
        result[1].is_unsigned = true;

        mysql_stmt_bind_result(stmt, result);
        mysql_stmt_store_result(stmt);

        // fetch 실패
        if (mysql_stmt_fetch(stmt) != 0) {
            std::cerr << "[GetUserCurrency] Fetch Error\n";
            mysql_stmt_close(stmt);
            return std::nullopt;
        }

        mysql_stmt_close(stmt);

        Currency currency;
        currency.bp = bp;
        currency.gcoin = gcoin;
        return currency;
    }
    catch (const std::exception& e) {
        std::cerr << "[GetUserCurrency] Exception: " << e.what() << '\n';
        return std::nullopt;
    }
}

std::optional<std::vector<InventoryItem>> MySQLManager::GetUserInventory(uint32_t userPk_) {
    semaphore.acquire();

    MYSQL* ConnPtr = GetConnection();
    if (!ConnPtr) {
        std::cerr << "[GetUserInventory] dbPool is empty.\n";
        return std::nullopt;
    }
    auto tempAutoConn = AutoConn(ConnPtr, dbPool, dbPoolMutex, semaphore);

    try {
        MYSQL_STMT* stmt = mysql_stmt_init(ConnPtr);
        std::string query =
            "SELECT item_code, item_type, quantity FROM user_inventory WHERE user_pk = ?";

        if (mysql_stmt_prepare(stmt, query.c_str(), query.length()) != 0) {
            std::cerr << "[GetUserInventory] Prepare Error: " << mysql_stmt_error(stmt) << '\n';
            mysql_stmt_close(stmt);
            return std::nullopt;
        }

        // 입력 바인딩
        MYSQL_BIND param[1];
        memset(param, 0, sizeof(param));
        param[0].buffer_type = MYSQL_TYPE_LONG;
        param[0].buffer = &userPk_;
        param[0].is_unsigned = true;

        if (mysql_stmt_bind_param(stmt, param) != 0) {
            std::cerr << "[GetUserInventory] Bind Error: " << mysql_stmt_error(stmt) << '\n';
            mysql_stmt_close(stmt);
            return std::nullopt;
        }

        if (mysql_stmt_execute(stmt) != 0) {
            std::cerr << "[GetUserInventory] Execute Error: " << mysql_stmt_error(stmt) << '\n';
            mysql_stmt_close(stmt);
            return std::nullopt;
        }

        // 결과 바인딩
        uint32_t itemCode = 0;
        uint8_t  itemType = 0;
        uint32_t quantity = 0;

        MYSQL_BIND result[3];
        memset(result, 0, sizeof(result));

        result[0].buffer_type = MYSQL_TYPE_LONG;
        result[0].buffer = &itemCode;
        result[0].is_unsigned = true;

        result[1].buffer_type = MYSQL_TYPE_TINY;
        result[1].buffer = &itemType;
        result[1].is_unsigned = true;

        result[2].buffer_type = MYSQL_TYPE_LONG;
        result[2].buffer = &quantity;
        result[2].is_unsigned = true;

        mysql_stmt_bind_result(stmt, result);
        mysql_stmt_store_result(stmt);

        std::vector<InventoryItem> inventory;
        while (mysql_stmt_fetch(stmt) == 0) {
            InventoryItem item;
            item.itemCode = itemCode;
            item.itemType = itemType;
            item.quantity = quantity;
            inventory.push_back(item);
        }

        mysql_stmt_close(stmt);
        return inventory;
    }
    catch (const std::exception& e) {
        std::cerr << "[GetUserInventory] Exception: " << e.what() << '\n';
        return std::nullopt;
    }
}