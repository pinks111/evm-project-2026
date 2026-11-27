#pragma once

#include "../src/common/socket_compat.hpp"

#include <string>
#include <vector>

using ClientSocket = SOCKET;
inline constexpr ClientSocket INVALID_CLIENT_SOCKET = INVALID_SOCKET;

bool open_client_connection(
    ClientSocket& clientSocket,
    const std::string& serverHost,
    std::string& error
);
bool send_all(
    ClientSocket clientSocket,
    const std::string& message,
    std::string& error
);
bool receive_available(
    ClientSocket clientSocket,
    std::string& pending,
    std::vector<std::string>& lines,
    bool& disconnected,
    std::string& error
);
void close_client_connection(ClientSocket& clientSocket);
void cleanup_client_network();
