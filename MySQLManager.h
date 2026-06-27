#pragma once
#pragma comment (lib, "libmysql.lib")

#include <optional>
#include <openssl/sha.h>
#include <openssl/rand.h>

#include "DBConfig.h"
#include "UserTypes.h"
#include "PasswordUtils.h"

class MySQLManager {
public:              
	static MySQLManager& GetInstance();

	MYSQL* GetConnection();

	bool init();
	void Shutdown();

	std::optional<uint32_t> LoginCheck(const std::string& id, const std::string& password);
	std::optional<UserInfo> GetUserInfo(uint32_t userPk_);
	std::optional<Currency> GetUserCurrency(uint32_t userPk_);
	std::optional<Costume> GetUserCostume(uint32_t userPk_);
	std::optional<std::vector<InventoryItem>> GetUserInventory(uint32_t userPk_);


	MySQLManager(const MySQLManager&) = delete;
	MySQLManager& operator=(const MySQLManager&) = delete;

private:
	MySQLManager() = default;
	~MySQLManager() {
		Shutdown();
	}

	std::mutex dbPoolMutex;
	std::queue<MYSQL*> dbPool;
	std::counting_semaphore<dbConnectionCount> semaphore{dbConnectionCount};

	int MysqlResult;
};