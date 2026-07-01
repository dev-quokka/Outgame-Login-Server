#pragma once
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <string>
#include <chrono>

#include "UserTypes.h"

constexpr uint16_t MAX_IP_LEN = 32;
constexpr uint16_t MAX_SERVER_USERS = 128;  
constexpr uint16_t MAX_JWT_TOKEN_LEN = 257;

struct DataPacket {
	uint32_t dataSize;
	uint16_t connObjNum;
	DataPacket(uint32_t dataSize_, uint16_t connObjNum_) : dataSize(dataSize_), connObjNum(connObjNum_) {}
	DataPacket() = default;
};

struct PacketInfo
{
	uint16_t packetId = 0;
	uint16_t dataSize = 0;
	uint16_t connObjNum = 0;
	char* pData = nullptr;
};

struct PACKET_HEADER
{
	uint16_t PacketLength;
	uint16_t PacketId;
};

enum class LoginFailCode : uint8_t {
	None = 0,
	WrongPassword = 1,  // 비번 틀림
	ServerError = 2,  // DB/Redis 오류
	NoServer = 3,  // 로비 서버 없음
};


// ======================= LOGIN SERVER =======================

struct USER_LOGIN_REQUEST : PACKET_HEADER { // 로그인 요청 패킷
	char userPassword[MAX_USER_PASSWORD_LEN + 1];
	char userId[MAX_USER_ID_LEN + 1];
};

struct USER_LOGIN_RESPONSE : PACKET_HEADER { // 로그인 요청 응답 패킷
	char      token[257];   // JWT 토큰
	UserInfo      userinfo;
	char userId[MAX_USER_ID_LEN + 1];
	Currency      currency;
	Costume       costume;
	char          ip[16];
	uint16_t      port;
	bool          isSuccess = false;
	uint8_t  failCode = (uint8_t)LoginFailCode::None;
};

struct USER_INVENTORY_PACKET : PACKET_HEADER {
	uint16_t		itemCount = 0;
	InventoryItem   items[MAX_INVENTORY_SIZE];
};

struct USER_FRIEND_PACKET : PACKET_HEADER {
	uint16_t   friendCount = 0;
	FriendInfo friends[MAX_FRIEND_SIZE];
};


enum class PACKET_ID : uint16_t {

	// ======================= LOGIN SERVER (1~ ) =======================

	// SYSTEM (1~)
	USER_LOGIN_REQUEST= 1,
	USER_LOGIN_RESPONSE = 2,

	USER_INVENTORY_PACKET = 3,
	USER_FRIEND_PACKET = 4,
};