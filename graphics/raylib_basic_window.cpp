#include "raylib.h"
#include "common/protocol.hpp"

int main()
{
    const int screenWidth = 800;
    const int screenHeight = 450;
  
    InitWindow(screenWidth, screenHeight, "raylib basic window");
    SetTargetFPS(60);

    const int cell_size = 64;
    const int offset = 40;

    int selected_x = -1;
    int selected_y = -1;

    GameState game_state;
    game_state.width = 5;
    game_state.height = 5;
    game_state.players = {
        {1, {0, 0}, 10},
        {2, {4, 4}, 10}
    };

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        int mouse_x = GetMouseX();
        int mouse_y = GetMouseY();
        if ((mouse_x >= 40 && mouse_x < 360) && (mouse_y >= 40 && mouse_y < 360)) {
          int cell_x = (mouse_x - offset) / cell_size;
          int cell_y = (mouse_y - offset) / cell_size;
          IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ? (selected_x = cell_x, selected_y = cell_y) : 0;
          DrawRectangleLinesEx(
            Rectangle{
              static_cast<float>(offset + cell_x * cell_size),
              static_cast<float>(offset + cell_y * cell_size),
              static_cast<float>(cell_size),
              static_cast<float>(cell_size)
            },
            3.0f,
            GREEN);
          if (selected_x != -1 && selected_y != -1) {
            DrawRectangleLinesEx(
              Rectangle{
                static_cast<float>(offset + selected_x * cell_size),
                static_cast<float>(offset + selected_y * cell_size),
                static_cast<float>(cell_size),
                static_cast<float>(cell_size)
              },
              3.0f,
              ORANGE);
          }
        }
        for (int y = 0; y < 5; ++y) {
          for (int x = 0; x < 5; ++x) {
            DrawRectangleLines(offset + x * cell_size, offset + y * cell_size, cell_size, cell_size, LIGHTGRAY);
          }
        }
        int player_row = 0;
        for (const auto& player : game_state.players) {
          const bool is_first_player = player.id == 1;
          DrawCircle(
            offset + player.position.x * cell_size + cell_size / 2,
            offset + player.position.y * cell_size + cell_size / 2,
            cell_size / 2 - 5,
            is_first_player ? RED : BLUE);
          DrawText(
            TextFormat(
              "%s (%s): HP: %d",
              is_first_player ? "ARTEM" : "NIKITA",
              is_first_player ? "RED" : "BLUE",
              player.hp),
            offset + game_state.width * cell_size + 24,
            offset + player_row * 30,
            20,
            BLACK);
          ++player_row;
        }
        EndDrawing();
    }
  
    CloseWindow();
    return 0;
}