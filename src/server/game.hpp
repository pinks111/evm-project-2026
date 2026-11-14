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
};

struct GameState {
  Board board;
  std::array<Character, 2> characters{};
  int turn = 1; // по id
  bool game_over = false;
};

std::string make_game_state(const GameState& game_state);