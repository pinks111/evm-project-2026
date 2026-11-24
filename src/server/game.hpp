#pragma once

#include <array>
#include <string>

inline constexpr int BOARD_SIZE = 5;

struct Board {
  int cells[BOARD_SIZE][BOARD_SIZE]{}; // 0 - свободно, 1 - препятствие

};

struct Character {
  std::string name;
  int x = 0;
  int y = 0;
  int health = 10;
  int dmg = 1;
  bool can_move = true;
  bool can_attack = true;
};

struct GameState {
  Board board;
  std::array<Character, 2> characters{};
  int turn = 1; // по id
  bool game_over = false;
};

std::string make_game_state(const GameState& game_state);
bool move(GameState& game_state, int player_id, int new_x, int new_y);
bool end_turn(GameState& game_state, int player_id);
bool attack(GameState& game_state, int player_id, int target_x, int target_y);