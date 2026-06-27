#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

constexpr uint16_t CHANNEL_SERVER_START_NUMBER = 0;
constexpr uint16_t GAME_SERVER_START_NUMBER = 2;

//  =========================== SERVER INFO  ===========================

enum class ServerType : uint16_t {
	// Login Server (0)
	LoginServer = 0,

	// Lobby Server (1~)
	LobbyServer01 = 1,
	LobbyServer02 = 2
};

struct ServerAddress {
	char ip[16];
	uint16_t port;
};

struct ReturnServerAddress {
	char ip[16];
	uint16_t port;
	ServerType serverType;
};

inline std::unordered_map<ServerType, ServerAddress> ServerAddressMap = {
	{ ServerType::LoginServer,   { "127.0.0.1", 9001 } },
	{ ServerType::LobbyServer01, { "127.0.0.1", 9011 } },
	{ ServerType::LobbyServer02, { "127.0.0.1", 9012 } }
};

inline std::string GetServerName(ServerType serverType_) {
	switch (serverType_) {
	case ServerType::LobbyServer01: return "LobbyServer01";
	case ServerType::LobbyServer02: return "LobbyServer02";
	default: return "Unknown";
	}
}