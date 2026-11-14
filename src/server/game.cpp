#include "game.hpp"

#include <string>
#include <nlohmann/json.hpp>

std::string make_game_state(const GameState& game_state) {
  using nlohmann::json;

  json blocked = json::array();

  for (int y = 0; y < BOARD_SIZE; ++y) {
    for (int x = 0; x < BOARD_SIZE; ++x) {
      if (game_state.board.cells[y][x] != 0) {
        blocked.push_back({x, y});
      }
    }
  }

  json players = json::array();

  for (std::size_t i = 0; i < game_state.characters.size(); ++i) {
    const auto& character = game_state.characters[i];

    players.push_back({
      {"id", i + 1},
      {"x", character.x},
      {"y", character.y},
      {"hp", character.health}
    });
  }

  json state = {
    {"width", BOARD_SIZE},
    {"height", BOARD_SIZE},
    {"blocked", blocked},
    {"turn", game_state.turn},
    {"can_move", true},
    {"can_attack", true},
    {"players", players}
  };

  return "GAME_STATE " + state.dump() + "\n";
}