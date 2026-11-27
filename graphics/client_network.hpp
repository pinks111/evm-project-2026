#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOUSER
#define NOUSER
#endif
#include <winsock2.h>

#include <string>
#include <vector>

bool open_client_connection(SOCKET& client_socket, const wchar_t* server_ip, std::string& error);
bool send_all(SOCKET clientSocket, const std::string& message, std::string& error);
bool receive_available(
    SOCKET clientSocket,
    std::string& pending,
    std::vector<std::string>& lines,
    bool& disconnected,
    std::string& error
);
void close_client_connection(SOCKET& clientSocket);
void cleanup_client_network();
