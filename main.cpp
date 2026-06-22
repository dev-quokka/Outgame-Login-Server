#include "OutGameLoginServer.h"

int main() {
	OutGameLoginServer server(maxClientCount);

	server.init();
	server.StartWork();

    std::cout << "========== OutGame 로그인 서버 ==========" << std::endl;
    std::string k = "";

    while (1) {
        std::cin >> k;
        if (k == "Login") break;
    }

    server.ServerEnd();
	return 0;
}