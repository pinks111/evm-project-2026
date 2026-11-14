#include "game.hpp"

#include <string>
#include <nlohmann/json.hpp>
#include <cstdlib>

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
    {"can_move", game_state.characters[game_state.turn - 1].can_move},
    {"can_attack", game_state.characters[game_state.turn - 1].can_attack},
    {"players", players}
  };

  return "GAME_STATE " + state.dump() + "\n";
}

bool move(GameState& game_state, int player_id, int new_x, int new_y) {
  if (player_id < 1 || player_id > static_cast<int>(game_state.characters.size())) {
    return false;
  }
  if (game_state.game_over || game_state.turn != player_id) {
    return false;
  }

  Character& character = game_state.characters[player_id - 1];

  if (!character.can_move) {
    return false;
  }

  if (new_x < 0 || new_x >= BOARD_SIZE || new_y < 0 || new_y >= BOARD_SIZE) {
    return false;
  }

  if (game_state.board.cells[new_y][new_x] != 0) {
    return false;
  }

  const int distance = std::abs(new_x - character.x) + std::abs(new_y - character.y);
  if (distance != 1) {
    return false;
  }

  for (const Character& other : game_state.characters) {
    if (other.x == new_x && other.y == new_y) {
        return false;
    }
  }

  character.x = new_x;
  character.y = new_y;
  character.can_move = false;

  return true;
}