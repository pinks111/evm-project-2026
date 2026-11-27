#include "lobby.hpp"
#include "net.hpp"
#include "game.hpp"
#include "../common/socket_compat.hpp"

#include <cctype>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

const int PORT = 2024;

void handleError(bool err, const char* msg) {
  if (!err) {
    return;
  }
  std::cerr << msg << socket_last_error() << std::endl;
  exit(EXIT_FAILURE);
}

int main(int argc, char* const argv[]) {
  int res = initialize_socket_runtime();
  handleError(res != 0, "Socket initialization failed with error: ");

  SOCKET server_sock = socket(AF_INET, SOCK_STREAM, 0);
  handleError(server_sock == INVALID_SOCKET, "Socket creation failed with error: ");

  if (!configure_socket_no_sigpipe(server_sock)) {
    std::cerr << "Could not configure server socket: "
              << socket_last_error() << '\n';
    close_socket(server_sock);
    cleanup_socket_runtime();
    return 1;
  }

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
    std::cerr << "Listen failed with error: " << socket_last_error() << std::endl;
    close_socket(server_sock);
    cleanup_socket_runtime();
    return 1;
  }

  Lobby lobby{};
  GameState game_state{};

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

    SOCKET highest_socket = server_sock;
    for (const auto& client : lobby.clients) {
      if (client.socket != INVALID_SOCKET && client.socket > highest_socket) {
        highest_socket = client.socket;
      }
    }

    if (wait_for_socket(highest_socket, &readfds) == SOCKET_ERROR) { // с селекта идёт обработка сокетов(игроков)
      std::cerr << "Select failed with error: " << socket_last_error() << std::endl;
      break;
    }

    if (FD_ISSET(server_sock, &readfds)) { 
      std::cout << "New client connection\n";
      SOCKET client_socket = accept(server_sock, nullptr, nullptr);
      if (client_socket == INVALID_SOCKET) {
        std::cerr << "Accept failed: " << socket_last_error() << '\n';
      } else if (!configure_socket_no_sigpipe(client_socket)) {
        std::cerr << "Could not configure client socket: "
                  << socket_last_error() << '\n';
        close_socket(client_socket);
      } else {
        auto it = std::find_if(lobby.clients.begin(), lobby.clients.end(),
                              [](const Client& c) { return c.socket == INVALID_SOCKET; });
        if (it == lobby.clients.end()) {
          std::cout << "Lobby is full\n";
          close_socket(client_socket);
        } else {
          it -> socket = client_socket;
          std::cout << "Client connected\n";
        }
      }
    }

    for (Client& client : lobby.clients) { // приём сообщений от клиентов
      if (client.socket == INVALID_SOCKET || !FD_ISSET(client.socket, &readfds)) {
        continue;
      }
      char buf[256];
      const std::ptrdiff_t received =
          receive_socket(client.socket, buf, sizeof(buf));
      if (received < 0) {
        const int error = socket_last_error();
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
        bool game_changed = false;

        // Обработка команд клиента
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
        } else if (line.starts_with("MOVE ")) {
          if (lobby.phase != lobby_phase::MATCH) {
            reply = "ERR_NOT_IN_MATCH\n";
          } else if (line.size() != 8 || line[6] != ' ' || 
                     !std::isdigit(static_cast<unsigned char>(line[5]))
                     || !std::isdigit(static_cast<unsigned char>(line[7]))) {
            reply = "ERR_INVALID_MOVE\n";
          } else {
            int player_id = 0;
            for (size_t i = 0; i < lobby.clients.size(); ++i) {
              if (&lobby.clients[i] == &client) {
                player_id = static_cast<int>(i + 1);
                break;
              }
            }

            int new_x = line[5] - '0';
            int new_y = line[7] - '0';

            if (move(game_state, player_id, new_x, new_y)) {
              reply = "OK MOVE\n";
              game_changed = true;
            } else {
              reply = "ERR_INVALID_MOVE\n";
            }
          }
        } else if (line == "END_TURN") {
          if (lobby.phase != lobby_phase::MATCH) {
            reply = "ERR_NOT_IN_MATCH\n";
          } else {
              int player_id = 0;
              for (size_t i = 0; i < lobby.clients.size(); ++i) {
                if (&lobby.clients[i] == &client) {
                  player_id = static_cast<int>(i + 1);
                  break;
                }
              }
            if (end_turn(game_state, player_id)) {
              reply = "OK END_TURN\n";
              game_changed = true;
            } else {
              reply = "ERR_NOT_YOUR_TURN\n";
            }
          }
        } else if (line.starts_with("ATTACK ")) {
          if (lobby.phase != lobby_phase::MATCH) {
            reply = "ERR_NOT_IN_MATCH\n";
          } else if (line.size() != 10 || line[8] != ' ' || 
                     !std::isdigit(static_cast<unsigned char>(line[7]))
                     || !std::isdigit(static_cast<unsigned char>(line[9]))) {
            reply = "ERR_INVALID_ATTACK\n";
          } else {
            int player_id = 0;
            for (size_t i = 0; i < lobby.clients.size(); ++i) {
              if (&lobby.clients[i] == &client) {
                player_id = static_cast<int>(i + 1);
                break;
              }
            }

            int target_x = line[7] - '0';
            int target_y = line[9] - '0';

            if (attack(game_state, player_id, target_x, target_y)) {
              reply = "OK ATTACK\n";
              game_changed = true;
            } else {
              reply = "ERR_INVALID_ATTACK\n";
            }
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

        if (game_changed) {
          const std::string message = make_game_state(game_state);
          for (Client& player : lobby.clients) {
            if (!send_all(player.socket, message)) {
              remove_client(lobby, player);
              send_state(lobby);
              break;
            }
          }
          if (lobby.phase == lobby_phase::MATCH && game_state.game_over) {
            const std::string result = "GAME_OVER " + std::to_string(game_state.turn) + "\n";
            lobby.phase = lobby_phase::WAITING;

            for (Client& player : lobby.clients) {
              player.ready = false;
              if (!send_all(player.socket, result)) {
                remove_client(lobby, player);
              }
            }
            send_state(lobby);
          }
        }

        if (client.socket == INVALID_SOCKET) {
          break;
        }

        if (lobby.start()) {
          lobby.phase = lobby_phase::MATCH;

          for (Client& player : lobby.clients) {
            if (!send_all(player.socket, "MATCH_START\n")) {  // начало матча, отправка состояния игры клиентам
              remove_client(lobby, player);
              send_state(lobby);
              break;
            }
          }
          
          if (lobby.phase == lobby_phase::MATCH) { // инициализация состояния игры
            Board board{};
            Character player1{"Player 1", 0, 0, 10, 1};
            Character player2{"Player 2", 4, 4, 10, 1};
            game_state = {board, {player1, player2}, 1, false};
            const std::string message = make_game_state(game_state);
            for (Client& player : lobby.clients) {
              if (!send_all(player.socket, message)) {
                remove_client(lobby, player);
                send_state(lobby);
                break;
              }
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
  
  close_socket(server_sock);
  cleanup_socket_runtime();
  return 0;
}
