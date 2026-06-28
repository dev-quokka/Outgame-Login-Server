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


// ======================= CENTER SERVER =======================

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

struct SYNCRONIZE_LEVEL_REQUEST : PACKET_HEADER {
	uint16_t	 level;
	uint16_t	 userPk;
	unsigned int currentExp;
};

struct SYNCRONIZE_LOGOUT_REQUEST : PACKET_HEADER {
	uint16_t userPk;
};

struct SERVER_USER_COUNTS_REQUEST : PACKET_HEADER {

};

struct SERVER_USER_COUNTS_RESPONSE : PACKET_HEADER {
	uint16_t serverCount;
	uint16_t serverUserCnt[MAX_SERVER_USERS + 1];
};

struct SHOP_DATA_REQUEST : PACKET_HEADER {

};

struct SHOP_DATA_RESPONSE : PACKET_HEADER {
	uint16_t shopItemSize;
};

struct PASS_DATA_REQUEST : PACKET_HEADER {

};

struct PASS_DATA_RESPONSE : PACKET_HEADER {
	uint16_t passDataSize;
};

struct MOVE_SERVER_REQUEST : PACKET_HEADER {
	uint16_t serverNum;
};

struct MOVE_SERVER_RESPONSE : PACKET_HEADER {
	char serverToken[MAX_JWT_TOKEN_LEN + 1];
	char ip[MAX_IP_LEN + 1];
	uint16_t port;
};

struct RAID_READY_REQUEST : PACKET_HEADER {
	char serverToken[MAX_JWT_TOKEN_LEN + 1];
	char ip[MAX_IP_LEN + 1];
	uint16_t port;
	uint16_t roomNum;
};

struct RAID_READY_FAIL : PACKET_HEADER {
	uint16_t userCenterObjNum;
	uint16_t roomNum;
};

struct RAID_RANKING_REQUEST : PACKET_HEADER {
	uint16_t startRank;
};

struct RAID_RANKING_RESPONSE : PACKET_HEADER {
	uint16_t rkCount;
	char reqScore[MAX_SCORE_SIZE + 1];
};

struct SHOP_BUY_ITEM_REQUEST : PACKET_HEADER {
	uint16_t itemCode = 0;
	uint16_t daysOrCount = 0; // [장비: 유저가 원하는 아이템의 사용 기간, 소비: 유저가 원하는 아이템 개수 묶음] 
	uint16_t itemType; // 0: 장비, 1: 소비, 2: 재료
	uint16_t position;
};


// ======================= CHANNEL SERVER =======================

struct CHANNEL_SERVER_CONNECT_REQUEST : PACKET_HEADER {
	uint16_t channelServerNum;
};

struct CHANNEL_SERVER_CONNECT_RESPONSE : PACKET_HEADER {
	bool isSuccess = false;
};

struct USER_DISCONNECT_AT_CHANNEL_REQUEST : PACKET_HEADER {
	uint16_t channelServerNum;
};

struct SYNC_EQUIPMENT_ENHANCE_REQUEST : PACKET_HEADER {
	uint16_t itemPosition;
	uint16_t enhancement;
	uint16_t userPk;
};


// ======================= MATCHING SERVER =======================

struct MATCHING_SERVER_CONNECT_REQUEST : PACKET_HEADER {

};

struct MATCHING_SERVER_CONNECT_RESPONSE : PACKET_HEADER {
	bool isSuccess = false;
};

struct MATCHING_REQUEST_TO_MATCHING_SERVER : PACKET_HEADER {
	uint16_t userPk;
	uint16_t userCenterObjNum;
	uint16_t userGroupNum;
};

struct MATCHING_RESPONSE_FROM_MATCHING_SERVER : PACKET_HEADER {
	uint16_t userCenterObjNum;
	bool isSuccess = false;
};

struct MATCHING_SUCCESS_RESPONSE_TO_CENTER_SERVER : PACKET_HEADER {
	uint16_t userCenterObjNum;
	uint16_t roomNum;
};

struct RAID_START_FAIL_REQUEST_TO_MATCHING_SERVER : PACKET_HEADER {
	uint16_t roomNum;
};

struct MATCHING_CANCEL_REQUEST : PACKET_HEADER {

};

struct MATCHING_CANCEL_RESPONSE : PACKET_HEADER {
	bool isSuccess = false;
};

struct MATCHING_CANCEL_REQUEST_TO_MATCHING_SERVER : PACKET_HEADER {
	uint16_t userCenterObjNum;
	uint16_t userGroupNum;
};

struct MATCHING_CANCEL_RESPONSE_FROM_MATCHING_SERVER : PACKET_HEADER {
	uint16_t userCenterObjNum;
	bool isSuccess = false;
};


enum class PACKET_ID : uint16_t {

	// ======================= LOGIN SERVER (1~ ) =======================

	// SYSTEM (1~)
	USER_LOGIN_REQUEST= 1,
	USER_LOGIN_RESPONSE = 2,

	USER_INVENTORY_PACKET = 3,
	USER_FRIEND_PACKET = 4,
};