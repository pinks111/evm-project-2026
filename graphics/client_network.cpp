#include "client_network.hpp"
#include <ws2tcpip.h>

#include <array>
#include <utility>

bool open_client_connection(SOCKET& client_socket, const wchar_t* server_ip, std::string& error) {
  WSADATA winsock_data{};
  const int startup_result = WSAStartup(MAKEWORD(2, 2), &winsock_data);
  if (startup_result != 0) {
    error = "WSAStartup failed: " + std::to_string(startup_result);
    return false;
  }

  sockaddr_in server_address{};
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(2024);
  if (InetPtonW(AF_INET, server_ip, &server_address.sin_addr) != 1) {
    error = "Invalid server IPv4 address";
    WSACleanup();
    return false;
  }

  client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (client_socket == INVALID_SOCKET) {
    error = "Socket creation failed: " + std::to_string(WSAGetLastError());
    WSACleanup();
    return false;
  }
  if (connect(client_socket, reinterpret_cast<sockaddr*>(&server_address),
              sizeof(server_address)) == SOCKET_ERROR) {
    error = "Connection failed: " + std::to_string(WSAGetLastError());
    close_client_connection(client_socket);
    WSACleanup();
    return false;
  }
  return true;
}

bool send_all(SOCKET clientSocket, const std::string& message, std::string& error)
{
    std::size_t totalSent = 0;
    while (totalSent < message.size()) {
        const int sent = send(
            clientSocket,
            message.data() + totalSent,
            static_cast<int>(message.size() - totalSent),
            0
        );
        if (sent == SOCKET_ERROR) {
            error = "Send failed: " + std::to_string(WSAGetLastError());
            return false;
        }
        if (sent == 0) {
            error = "Connection closed while sending";
            return false;
        }
        totalSent += static_cast<std::size_t>(sent);
    }
    return true;
}

bool receive_available(
    SOCKET clientSocket,
    std::string& pending,
    std::vector<std::string>& lines,
    bool& disconnected,
    std::string& error
)
{
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(clientSocket, &readSet);
    timeval timeout{};

    const int ready = select(0, &readSet, nullptr, nullptr, &timeout);
    if (ready == SOCKET_ERROR) {
        error = "Select failed: " + std::to_string(WSAGetLastError());
        return false;
    }
    if (ready == 0) {
        return true;
    }

    std::array<char, 1024> buffer{};
    const int received = recv(
        clientSocket,
        buffer.data(),
        static_cast<int>(buffer.size()),
        0
    );
    if (received == SOCKET_ERROR) {
        const int socketError = WSAGetLastError();
        if (socketError == WSAEWOULDBLOCK) {
            return true;
        }
        error = "Receive failed: " + std::to_string(socketError);
        return false;
    }
    if (received == 0) {
        disconnected = true;
        return true;
    }

    pending.append(buffer.data(), static_cast<std::size_t>(received));
    if (pending.size() > 4096) {
        error = "Server sent too much buffered data";
        return false;
    }

    while (true) {
        const std::size_t lineEnd = pending.find('\n');
        if (lineEnd == std::string::npos) {
            if (pending.size() > 1024) {
                error = "Server sent a line longer than 1024 bytes";
                return false;
            }
            break;
        }
        if (lineEnd > 1024) {
            error = "Server sent a line longer than 1024 bytes";
            return false;
        }

        std::string line = pending.substr(0, lineEnd);
        pending.erase(0, lineEnd + 1);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(std::move(line));
    }

    return true;
}

void close_client_connection(SOCKET& clientSocket)
{
    if (clientSocket != INVALID_SOCKET) {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }
}

void cleanup_client_network()
{
    WSACleanup();
}
