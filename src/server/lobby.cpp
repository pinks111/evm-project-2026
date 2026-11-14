#include "lobby.hpp"
#include "net.hpp"

#include <iostream>
#include <nlohmann/json.hpp>

namespace {

void disconnect_client(Client& client) {
    closesocket(client.socket);
    client = Client{};
    std::cout << "Connection closed\n";
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

} // namespace

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

void send_state(Lobby& lobby) {
  bool retry;

  do {
    retry = false;
    for (size_t i = 0; i < lobby.clients.size(); ++i) {
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
