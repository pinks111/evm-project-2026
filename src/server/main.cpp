#include<winsock2.h>
#include <iostream>
#include <string>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <array>

#pragma comment(lib,"ws2_32.lib")
const int PORT = 2024;

void handleError(bool err, const char* msg) {
  if (!err) {
    return;
  }
  std::cerr << msg << WSAGetLastError() << std::endl;
  exit(EXIT_FAILURE);
}

bool send_all(SOCKET socket, const std::string& reply) {
  const int reply_size = static_cast<int>(reply.size());
  int total_sent = 0;

  while (total_sent < reply_size) {
    int sent = send(socket,
                    reply.data() + total_sent,
                    reply_size - total_sent,
                    0);

    if (sent == SOCKET_ERROR) {
      std::cerr << "Send failed: " << WSAGetLastError() << '\n';
      return false;
    }

    if (sent == 0) {
      std::cerr << "Could not send the remaining bytes\n";
      return false;
    }

    total_sent += sent;
  }
  return true;
}

struct Client {
  SOCKET socket = INVALID_SOCKET;
  std::string player_name;
  bool ready = false;
  std::string pending;
};

enum class lobby_phase { WAITING, MATCH };

struct Lobby {
  std::array<Client, 2> clients{};
  lobby_phase phase = lobby_phase::WAITING;

  bool start() const {
    if (phase != lobby_phase::WAITING) {
      return false;
    }

    for (const auto& client : clients) {
      if (client.socket == INVALID_SOCKET || client.player_name.empty() || !client.ready) {
        return false;
      }
    }
    return true;
  }
};

void disconnect_client(Client& client) {
    closesocket(client.socket);
    client = Client{};
    std::cout << "Connection closed\n";
}

void remove_client(Lobby& lobby, Client& leaving) {
    disconnect_client(leaving);

    if (lobby.phase != lobby_phase::MATCH) {
        return;
    }

    lobby.phase = lobby_phase::WAITING;

    for (Client& player : lobby.clients) {
        if (player.socket == INVALID_SOCKET) {
            continue;
        }

        player.ready = false;

        if (!send_all(player.socket, "MATCH_ABORTED\n")) {
            disconnect_client(player);
        }
    }
}

std::string make_state(const Lobby& lobby, int id) {
    using nlohmann::json;

    json players = json::array();

    for (std::size_t i = 0; i < lobby.clients.size(); ++i) {
        const Client& client = lobby.clients[i];

        if (client.socket == INVALID_SOCKET || client.player_name.empty()) {
            continue;
        }

        players.push_back({
            {"id", i + 1},
            {"name", client.player_name},
            {"ready", client.ready}
        });
    }

    json state = {
        {"you", id},
        {"players", players}
    };

    return "LOBBY_STATE " + state.dump() + "\n";
}

void send_state(Lobby& lobby) {
  bool retry;

  do {
    retry = false;
    for (std::size_t i = 0; i < lobby.clients.size(); ++i) {
      Client& client = lobby.clients[i];

      if (client.socket == INVALID_SOCKET ||
        client.player_name.empty()) {
        continue;
      }

      std::string message = make_state(lobby, static_cast<int>(i + 1));

      if (!send_all(client.socket, message)) {
        remove_client(lobby, client);
        retry = true;
        break;
      }
    }
  } while (retry);
}

int main(int argc, char* const argv[]) {
  WSADATA wsa;
  int res = WSAStartup(MAKEWORD(2, 2), &wsa);
  handleError(res != 0, "WSAStartup failed with error: ");

  SOCKET server_sock = socket(AF_INET, SOCK_STREAM, 0);
  handleError(server_sock == INVALID_SOCKET, "Socket creation failed with error: ");

  std::cout << "Socket created successfully" << std::endl;

  struct sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  res = bind(server_sock, (struct sockaddr*)&address, sizeof(address));
  handleError(res == SOCKET_ERROR, "Bind failed with error: ");
  std::cout << "Bind done" << std::endl;

  res = listen(server_sock, 3);
  if (res == SOCKET_ERROR) {
    std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
    closesocket(server_sock);
    WSACleanup();
    return 1;
  }

  Lobby lobby{};

  while (true) {
    std::cout << "Waiting for a client...\n";

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(server_sock, &readfds);

    for (const auto& client : lobby.clients) {
      if (client.socket != INVALID_SOCKET) {
        FD_SET(client.socket, &readfds);
      }
    }

    if (select(0, &readfds, nullptr, nullptr, nullptr) == SOCKET_ERROR) {
      std::cerr << "Select failed with error: " << WSAGetLastError() << std::endl;
      break;
    }

    if (FD_ISSET(server_sock, &readfds)) {
      std::cout << "New client connection\n";
      SOCKET client_socket = accept(server_sock, nullptr, nullptr);
      if (client_socket == INVALID_SOCKET) {
        std::cerr << "Accept failed: " << WSAGetLastError() << '\n';
      } else {
        auto it = std::find_if(lobby.clients.begin(), lobby.clients.end(),
                              [](const Client& c) { return c.socket == INVALID_SOCKET; });
        if (it == lobby.clients.end()) {
          std::cout << "Lobby is full\n";
          closesocket(client_socket);
        } else {
          it -> socket = client_socket;
          std::cout << "Client connected\n";
        }
      }
    }

    for (Client& client : lobby.clients) {
      if (client.socket == INVALID_SOCKET || !FD_ISSET(client.socket, &readfds)) {
        continue;
      }
      char buf[256];
      int received = recv(client.socket,
                          buf,
                          sizeof(buf),
                          0  
                        );
      if (received == SOCKET_ERROR) {
        const int error = WSAGetLastError();
        std::cerr << "Receive failed: " << error << '\n';
        remove_client(lobby, client);
        send_state(lobby);
        continue;
      }
      if (received == 0) {
        if (client.pending.empty()) {
          std::cout << "Client disconnected\n";
        } else {
          std::cout << "Client disconnected before completing the message\n";
        }
        remove_client(lobby, client);
        send_state(lobby);
        continue;
      }

      client.pending.append(buf, received);
      bool connected = true;
      while (true) {
        size_t pos = client.pending.find('\n');
        size_t line_size = (pos == std::string::npos) ? client.pending.size() : pos;

        if (line_size > 1024) {
          std::cerr << "Message too long, closing connection\n";
          connected = false;
          break;
        }
        if (pos == std::string::npos) {
          break;
        }

        std::string line = client.pending.substr(0, pos);
        client.pending.erase(0, pos + 1);
        std::cout << "Received message: " << line << std::endl;

        std::string reply;
        bool lobby_changed = false;

        if (line.starts_with("JOIN ") && line.size() > 5) {
          if (client.player_name.empty()) {
            client.player_name = line.substr(5);
            reply = "OK JOIN\n";
            lobby_changed = true;
          } else {
            reply = "ERR_ALREADY_JOINED\n";
          }
        } else if (line == "READY") {
          if (client.player_name.empty()) {
            reply = "ERR_JOIN_FIRST\n";
          } else {
            client.ready = true;
            reply = "OK READY\n";
            lobby_changed = true;
          }
        } else {
          reply = "ERR_UNKNOWN_COMMAND\n";
        }

        if (!send_all(client.socket, reply)) {
          connected = false;
          break;
        }
        std::cout << "Reply sent\n";
        
        if (lobby_changed && lobby.phase == lobby_phase::WAITING && !lobby.start()) {
          send_state(lobby);
        }

        if (client.socket == INVALID_SOCKET) {
          break;
        }

        if (lobby.start()) {
          lobby.phase = lobby_phase::MATCH;

          for (Client& player : lobby.clients) {
            if (!send_all(player.socket, "MATCH_START\n")) {
              remove_client(lobby, player);
              send_state(lobby);
              break;
            }
          }

          if (client.socket == INVALID_SOCKET) {
            break;
          }
        } 
      }

      if (!connected) {
        remove_client(lobby, client);
        send_state(lobby);
      }
    }
  }
  
  closesocket(server_sock);
  WSACleanup();
  return 0;
}