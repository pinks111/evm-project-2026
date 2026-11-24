#include "client_network.hpp"
#include "raylib.h"
#include "common/protocol.hpp"
#include "client/client_state.hpp"

#include <string>
#include <vector>

int main(int argc, char* argv[])
{
    const int screenWidth = 800;
    const int screenHeight = 450;
    const std::string nickname = argc > 1 ? argv[1] : "Artem";
  
    InitWindow(screenWidth, screenHeight, "raylib basic window");
    SetTargetFPS(60);

    const int cell_size = 64;
    const int offset = 40;

    int selected_x = -1;
    int selected_y = -1;

    SOCKET clientSocket = INVALID_SOCKET;
    std::string networkError;
    std::string status = "Connecting to 127.0.0.1:2024...";
    std::string serverPending;
    LobbyState lobby{};
    bool joined = false;
    bool readySent = false;
    bool matchActive = false;

    const bool networkStarted = open_client_connection(clientSocket, networkError);
    if (networkStarted) {
      status = "Connected. Sending JOIN...";
      if (send_all(clientSocket, "JOIN " + nickname + "\n", networkError)) {
        status = "JOIN sent. Waiting for server.";
      } else {
        status = networkError;
        close_client_connection(clientSocket);
      }
    } else {
      status = networkError;
    }

    // состояние игры
    GameState game_state;
    game_state.width = 5;
    game_state.height = 5;
    game_state.players = {
        {1, {0, 0}, 10},
        {2, {4, 4}, 10}
    };
    game_state.turn = 1; // ход игрока 1

    while (!WindowShouldClose())
    {
        if (clientSocket != INVALID_SOCKET) {
          std::vector<std::string> lines;
          bool disconnected = false;
          if (!receive_available(
                clientSocket,
                serverPending,
                lines,
                disconnected,
                networkError)) {
            status = networkError;
            close_client_connection(clientSocket);
          } else if (disconnected) {
            status = "Server disconnected";
            close_client_connection(clientSocket);
          } else {
            for (const std::string& line : lines) {
              if (line == "OK JOIN") {
                joined = true;
                status = "Joined. Press R when ready.";
              } else if (line == "OK READY") {
                status = "Ready. Waiting for the other player.";
              } else if (line.rfind("LOBBY_STATE ", 0) == 0) {
                if (parseLobbyState(line.substr(12), lobby)) {
                  status = "Lobby updated. Press R to ready.";
                } else {
                  status = "Invalid LOBBY_STATE from server";
                }
              } else if (line == "MATCH_START") {
                matchActive = true;
                status = "Match started";
              } else if (line.rfind("GAME_STATE ", 0) == 0) {
                if (parseGameState(line.substr(11), game_state)) {
                  status = "Game state updated";
                } else {
                  status = "Invalid GAME_STATE from server";
                }
              } else if (line.rfind("GAME_OVER ", 0) == 0) {
                matchActive = false;
                readySent = false;
                status = "Game over. Press R for another match.";
              } else if (line == "MATCH_ABORTED") {
                matchActive = false;
                readySent = false;
                status = "Match aborted: opponent disconnected";
              } else if (line.rfind("OK ", 0) == 0 || line.rfind("ERR_", 0) == 0) {
                status = formatServerResponse(line);
              } else if (!line.empty()) {
                status = "Server: " + line;
              }
            }
          }
        }

        if (IsKeyPressed(KEY_R) && clientSocket != INVALID_SOCKET && joined
            && !readySent && !matchActive) {
          if (send_all(clientSocket, "READY\n", networkError)) {
            readySent = true;
            status = "READY sent. Waiting for the other player.";
          } else {
            status = networkError;
            close_client_connection(clientSocket);
          }
        }

        if (IsKeyPressed(KEY_T) && clientSocket != INVALID_SOCKET) {
          if (!matchActive) {
            status = "Wait for MATCH_START before ending the turn";
          } else if (lobby.playerId == 0 || game_state.turn != lobby.playerId) {
            status = "It is not your turn";
          } else if (send_all(clientSocket, "END_TURN\n", networkError)) {
            status = "END_TURN sent. Waiting for server state.";
          } else {
            status = networkError;
            close_client_connection(clientSocket);
          }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        // выделение клетки при наведении мыши
        int mouse_x = GetMouseX();
        int mouse_y = GetMouseY();
        if ((mouse_x >= 40 && mouse_x < 360) && (mouse_y >= 40 && mouse_y < 360)) {
          int cell_x = (mouse_x - offset) / cell_size;
          int cell_y = (mouse_y - offset) / cell_size;
          if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            selected_x = cell_x;
            selected_y = cell_y;
            if (!matchActive) {
              status = "Wait for MATCH_START before moving";
            } else if (lobby.playerId == 0) {
              status = "Waiting for your player ID from server";
            } else if (game_state.turn != lobby.playerId) {
              status = TextFormat(
                "Not your turn: P%d has the turn",
                game_state.turn);
            } else if (!game_state.canMove) {
              status = "You have already moved this turn";
            } else if (clientSocket != INVALID_SOCKET) {
              const std::string command =
                  "MOVE " + std::to_string(cell_x) + " "
                  + std::to_string(cell_y) + "\n";
              if (send_all(clientSocket, command, networkError)) {
                status = "MOVE sent. Waiting for server state.";
              } else {
                status = networkError;
                close_client_connection(clientSocket);
              }
            }
          }
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
            DrawText(TextFormat("MOVE: (%d, %d)", selected_x, selected_y), 40, 400, 20, BLACK);
          }
        }
        // поле
        for (int y = 0; y < 5; ++y) {
          for (int x = 0; x < 5; ++x) {
            DrawRectangleLines(offset + x * cell_size, offset + y * cell_size, cell_size, cell_size, LIGHTGRAY);
          }
        }
        // игроки и информация о них
        int player_row = 0;
        for (const auto& player : game_state.players) {
          const bool is_first_player = player.id == 1;
          DrawCircle(
            offset + player.position.x * cell_size + cell_size / 2,
            offset + player.position.y * cell_size + cell_size / 2,
            cell_size / 2 - 5,
            is_first_player ? RED : BLUE);
          const std::string* playerName = nullptr;
          for (const auto& lobbyPlayer : lobby.players) {
            if (lobbyPlayer.id == player.id) {
              playerName = &lobbyPlayer.name;
              break;
            }
          }
          DrawText(
            TextFormat(
              "%s (%s): HP: %d%s%s",
              playerName ? playerName->c_str() : (is_first_player ? "ARTEM" : "NIKITA"),
              is_first_player ? "RED" : "BLUE",
              player.hp,
              player.id == game_state.turn ? " [TURN]" : "",
              player.id == lobby.playerId ? " [YOU]" : ""),
            offset + game_state.width * cell_size + 24,
            offset + player_row * 30,
            20,
            BLACK);
          ++player_row;
        }
        DrawText(status.c_str(), offset + game_state.width * cell_size + 24, 100, 16, BLACK);
        DrawText("Press R to ready", offset + game_state.width * cell_size + 24, 130, 16, DARKGRAY);
        DrawText("Press T to end turn", offset + game_state.width * cell_size + 24, 160, 16, DARKGRAY);
        DrawText(TextFormat("YOU: P%d", lobby.playerId), offset + game_state.width * cell_size + 24, 190, 16, DARKGRAY);
        DrawText(TextFormat("TURN: P%d", game_state.turn), offset + game_state.width * cell_size + 24, 220, 16, DARKGRAY);
        EndDrawing();
    }
  
    close_client_connection(clientSocket);
    if (networkStarted) {
      cleanup_client_network();
    }
    CloseWindow();
    return 0;
}