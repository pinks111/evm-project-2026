#include <arpa/inet.h>
#include "client_state.hpp"
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>

namespace {

constexpr unsigned short PORT = 2024;

bool sendAll(int socket, const std::string& message)
{
    std::size_t sentTotal = 0;
    while (sentTotal < message.size()) {
        const ssize_t sent = send(
            socket,
            message.data() + sentTotal,
            message.size() - sentTotal,
            0
        );
        if (sent <= 0) {
            return false;
        }
        sentTotal += static_cast<std::size_t>(sent);
    }
    return true;
}

void processLines(
    std::string& pending,
    LobbyState& lobby,
    GameState& game,
    bool& matchActive
)
{
    while (true) {
        const std::size_t lineEnd = pending.find('\n');
        if (lineEnd == std::string::npos) {
            break;
        }

        std::string line = pending.substr(0, lineEnd);
        pending.erase(0, lineEnd + 1);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.rfind("LOBBY_STATE ", 0) == 0) {
            if (parseLobbyState(line.substr(12), lobby)) {
                printLobbyState(lobby);
            } else {
                std::cout << "< invalid LOBBY_STATE\n";
            }
        } else if (line.rfind("GAME_STATE ", 0) == 0) {
            if (parseGameState(line.substr(11), game)) {
                printGameState(game);
            } else {
                std::cout << "< invalid GAME_STATE\n";
            }
        } else if (line == "MATCH_START") {
            matchActive = true;
            std::cout << "\nMatch started\n";
        } else if (line.rfind("GAME_OVER ", 0) == 0) {
            matchActive = false;
            std::cout << "\nGame over. Winner: player "
                      << line.substr(10) << '\n';
        } else if (line == "MATCH_ABORTED") {
            matchActive = false;
            std::cout << "\nMatch aborted. Opponent disconnected\n";
        } else if (line.rfind("OK ", 0) == 0
                   || line.rfind("ERR_", 0) == 0) {
            std::cout << formatServerResponse(line) << '\n';
        } else if (!line.empty()) {
            std::cout << "< " << line << '\n';
        }
    }
}

}

int main(int argc, char* argv[])
{
    std::string host = "127.0.0.1";
    if (argc > 1) {
        host = argv[1];
    }

    std::cout << "Nickname: ";
    std::string nickname;
    std::getline(std::cin, nickname);
    if (nickname.empty()) {
        std::cerr << "Nickname cannot be empty\n";
        return 1;
    }

    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        std::cerr << "Socket error: " << std::strerror(errno) << '\n';
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    if (inet_pton(AF_INET, host.c_str(), &serverAddress.sin_addr) != 1) {
        std::cerr << "Invalid server IPv4 address: " << host << '\n';
        close(clientSocket);
        return 1;
    }

    if (connect(
            clientSocket,
            reinterpret_cast<sockaddr*>(&serverAddress),
            sizeof(serverAddress)
        ) < 0) {
        std::cerr << "Connection failed: " << std::strerror(errno) << '\n';
        close(clientSocket);
        return 1;
    }

    std::cout << "Connected to " << host << ':' << PORT << '\n';

    if (!sendAll(clientSocket, "JOIN " + nickname + "\n")) {
        std::cerr << "Failed to send JOIN\n";
        close(clientSocket);
        return 1;
    }

    std::string serverPending;
    LobbyState lobby{};
    GameState game{};
    bool matchActive = false;
    std::string input;
    std::cout << "Commands: READY, MOVE x y, ATTACK x y, END_TURN, quit\n";
    std::cout << "> ";
    std::cout.flush();

    while (true) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(STDIN_FILENO, &readSet);
        FD_SET(clientSocket, &readSet);

        const int maxDescriptor =
            clientSocket > STDIN_FILENO ? clientSocket : STDIN_FILENO;
        if (select(maxDescriptor + 1, &readSet, nullptr, nullptr, nullptr) < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "Select error: " << std::strerror(errno) << '\n';
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &readSet)) {
            if (!std::getline(std::cin, input)) {
                break;
            }

            if (input == "quit" || input == "exit") {
                break;
            }

            if (input.empty()) {
                std::cout << "> ";
                std::cout.flush();
                continue;
            }

            std::string commandError;
            if (!validateBattleCommand(
                    input,
                    lobby,
                    game,
                    matchActive,
                    commandError
                )) {
                std::cout << commandError << '\n';
                std::cout << "> ";
                std::cout.flush();
                continue;
            }

            if (!sendAll(clientSocket, input + "\n")) {
                std::cerr << "Failed to send command\n";
                break;
            }
            std::cout << "> ";
            std::cout.flush();
        }

        if (FD_ISSET(clientSocket, &readSet)) {
            char buffer[1024];
            const ssize_t received = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (received < 0) {
                std::cerr << "Receive error: " << std::strerror(errno) << '\n';
                break;
            }
            if (received == 0) {
                std::cout << "Server closed the connection\n";
                break;
            }

            serverPending.append(buffer, static_cast<std::size_t>(received));
            if (serverPending.size() > 4096) {
                std::cerr << "Server sent too much buffered data\n";
                break;
            }
            processLines(serverPending, lobby, game, matchActive);
            std::cout << "> ";
            std::cout.flush();
        }
    }

    close(clientSocket);
    return 0;
}
