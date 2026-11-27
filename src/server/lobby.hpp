#pragma once

#include "../common/socket_compat.hpp"

#include <array>
#include <string>

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

void remove_client(Lobby& lobby, Client& leaving);
void send_state(Lobby& lobby);
