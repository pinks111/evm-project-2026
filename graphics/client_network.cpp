#include "client_network.hpp"

#include <array>
#include <cerrno>
#include <utility>

bool open_client_connection(
    ClientSocket& clientSocket,
    const std::string& serverHost,
    std::string& error
)
{
    const int startupResult = initialize_socket_runtime();
    if (startupResult != 0) {
        error = "Socket initialization failed: "
            + std::to_string(startupResult);
        return false;
    }

    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_CLIENT_SOCKET) {
        error = "Socket creation failed: "
            + std::to_string(socket_last_error());
        cleanup_socket_runtime();
        return false;
    }

#ifdef SO_NOSIGPIPE
    if (!configure_socket_no_sigpipe(clientSocket)) {
        error = "Could not configure socket: "
            + std::to_string(socket_last_error());
        close_client_connection(clientSocket);
        cleanup_socket_runtime();
        return false;
    }
#endif

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(2024);
    if (inet_pton(AF_INET, serverHost.c_str(), &serverAddress.sin_addr) != 1) {
        error = "Invalid server IPv4 address: " + serverHost;
        close_client_connection(clientSocket);
        cleanup_socket_runtime();
        return false;
    }

    if (connect(
            clientSocket,
            reinterpret_cast<sockaddr*>(&serverAddress),
            sizeof(serverAddress)
        ) < 0) {
        error = "Connection failed: "
            + std::to_string(socket_last_error());
        close_client_connection(clientSocket);
        cleanup_socket_runtime();
        return false;
    }

    return true;
}

bool send_all(
    ClientSocket clientSocket,
    const std::string& message,
    std::string& error
)
{
    std::size_t totalSent = 0;
    while (totalSent < message.size()) {
        const std::ptrdiff_t sent = send_socket(
            clientSocket,
            message.data() + totalSent,
            message.size() - totalSent
        );
        if (sent < 0) {
#ifdef _WIN32
            if (socket_last_error() == WSAEINTR) {
                continue;
            }
#else
            if (errno == EINTR) {
                continue;
            }
#endif
            error = "Send failed: " + std::to_string(socket_last_error());
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
    ClientSocket clientSocket,
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

    const int ready = wait_for_socket(clientSocket, &readSet, &timeout);
    if (ready < 0) {
#ifdef _WIN32
        if (socket_last_error() == WSAEINTR) {
            return true;
        }
#else
        if (errno == EINTR) {
            return true;
        }
#endif
        error = "Select failed: " + std::to_string(socket_last_error());
        return false;
    }
    if (ready == 0) {
        return true;
    }

    std::array<char, 1024> buffer{};
    const std::ptrdiff_t received =
        receive_socket(clientSocket, buffer.data(), buffer.size());
    if (received < 0) {
#ifdef _WIN32
        const int socketError = socket_last_error();
        if (socketError == WSAEWOULDBLOCK) {
            return true;
        }
        error = "Receive failed: " + std::to_string(socketError);
#else
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;
        }
        error = "Receive failed: " + std::to_string(socket_last_error());
#endif
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

void close_client_connection(ClientSocket& clientSocket)
{
    if (clientSocket != INVALID_CLIENT_SOCKET) {
        close_socket(clientSocket);
        clientSocket = INVALID_CLIENT_SOCKET;
    }
}

void cleanup_client_network()
{
    cleanup_socket_runtime();
}
