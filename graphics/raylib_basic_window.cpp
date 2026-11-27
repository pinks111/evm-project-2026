#include "client_network.hpp"
#include "raylib.h"
#include "common/protocol.hpp"
#include "client/client_state.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

static Font load_name_font() {
  const char* windows = std::getenv("WINDIR");
  if (!windows) {
    return GetFontDefault();
  }

  const std::string path = std::string(windows) + "/Fonts/segoeui.ttf";
  std::vector<int> glyphs;
  for (int ch = 32; ch <= 126; ++ch) glyphs.push_back(ch);
  for (int ch = 0x400; ch <= 0x4ff; ++ch) glyphs.push_back(ch);
  return LoadFontEx(path.c_str(), 24, glyphs.data(), static_cast<int>(glyphs.size()));
}

static std::string read_nickname(Font font, const char* argument) {
  std::string name;
  if (argument) {
    name = argument;
    if (name.find_first_not_of(' ') != std::string::npos && name.size() <= 1019 &&
      name.find_first_of("\r\n") == std::string::npos) {
      return name;
    }
    name.clear();
  }
  while (!WindowShouldClose()) {
    for (int ch = GetCharPressed(); ch; ch = GetCharPressed()) {
      int bytes = 0;
      const char* encoded = CodepointToUTF8(ch, &bytes);
      if (ch >= 32 && name.size() + bytes <= 1019) {
        name.append(encoded, bytes);
      }
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !name.empty()) {
      size_t i = name.size() - 1;
      while (i > 0 && (static_cast<unsigned char>(name[i]) & 0xc0) == 0x80) --i;
      name.erase(i);
    }
    if (IsKeyPressed(KEY_ENTER) && name.find_first_not_of(' ') != std::string::npos) {
      return name;
    }
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawText("Enter your nickname and press Enter", 40, 40, 20, DARKGRAY);
    DrawTextEx(font, name.c_str(), {40, 80}, 24, 1, BLACK);
    EndDrawing();
  }
  return {};
}

int main(int argc, char* argv[]) {
  InitWindow(800, 450, "ToDA 3");
  SetTargetFPS(60);
  InitAudioDevice();
  Music music = LoadMusicStream("music/music.mp3");
  Font name_font = load_name_font();
  const std::string nickname = read_nickname(name_font, argc > 1 ? argv[1] : nullptr);
  const std::string server_ip = argc > 2 ? argv[2] : "127.0.0.1";
  constexpr int cell_size = 64;
  constexpr int offset = 40;

  SOCKET client_socket = INVALID_SOCKET;
  std::string status, server_pending;
  LobbyState lobby{};
  GameState game{};
  bool ready_sent = false;
  bool match_active = false;
  int selected_x = -1, selected_y = -1;
  const bool network_started = !nickname.empty() &&
    open_client_connection(client_socket, server_ip, status);
  auto send_command = [&](const std::string& command) {
    if (!send_all(client_socket, command + "\n", status)) {
      close_client_connection(client_socket);
      return false;
    }
    return true;
  };
  if (network_started && send_command("JOIN " + nickname)) {
    status = "Joining lobby...";
  }

  while (!nickname.empty() && !WindowShouldClose()) {
    if (client_socket != INVALID_SOCKET) {
      std::vector<std::string> lines;
      bool disconnected = false;
      if (!receive_available(client_socket, server_pending, lines, disconnected, status) || disconnected) {
        if (disconnected) {
          status = "Server disconnected";
        }
        close_client_connection(client_socket);
      }
      for (const auto& line : lines) {
        if (line.starts_with("LOBBY_STATE ")) {
          if (!parseLobbyState(line.substr(12), lobby)) {
            status = "Invalid lobby state";
          }
        } else if (line == "MATCH_START") {
          PlayMusicStream(music);
          match_active = true;
          game = {};
          selected_x = -1;
          status = "Match started";
        } else if (line.starts_with("GAME_STATE ")) {
          if (!parseGameState(line.substr(11), game)) {
            status = "Invalid game state";
          }
        } else if (line.starts_with("GAME_OVER ") || line == "MATCH_ABORTED") {
          StopMusicStream(music);
          match_active = false;
          ready_sent = false;
          status = line == "MATCH_ABORTED" ? "Opponent disconnected" :
            line.substr(10) == std::to_string(lobby.playerId) ? "You won!" : "You lost.";
        } else {
          status = formatServerResponse(line);
        }
      }
    }
    const bool online = client_socket != INVALID_SOCKET;
    if (online && !match_active && lobby.playerId && !ready_sent && IsKeyPressed(KEY_R)) {
      ready_sent = send_command("READY");
    }
    if (online && match_active && IsKeyPressed(KEY_T) &&
      validateBattleCommand("END_TURN", lobby, game, match_active, status)) {
      send_command("END_TURN");
    }

    const int mx = GetMouseX() - offset, my = GetMouseY() - offset;
    const bool over_board = match_active && mx >= 0 && my >= 0 &&
      mx < game.width * cell_size && my < game.height * cell_size;
    const int x = mx / cell_size, y = my / cell_size;
    if (online && over_board) {
      const bool attack = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
      if (attack || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        selected_x = x;
        selected_y = y;
        const std::string action = attack ? "ATTACK " : "MOVE ";
        const std::string command = action + std::to_string(x) + " " + std::to_string(y);
        if (validateBattleCommand(command, lobby, game, match_active, status)) {
          send_command(command);
        }
      }
    }

    if (match_active && client_socket != INVALID_SOCKET) {
      UpdateMusicStream(music);
    } else if (IsMusicStreamPlaying(music)) {
      StopMusicStream(music);
    }
    BeginDrawing();
    ClearBackground(RAYWHITE);
    int panel = offset;
    if (match_active) {
      panel += game.width * cell_size + 24;
      for (int row = 0; row < game.height; ++row)
        for (int col = 0; col < game.width; ++col)
          DrawRectangleLines(offset + col * cell_size, offset + row * cell_size,
                               cell_size, cell_size, LIGHTGRAY);
      auto outline = [&](int col, int row, Color color) {
        DrawRectangleLinesEx({float(offset + col * cell_size), float(offset + row * cell_size),
                              float(cell_size), float(cell_size)}, 3, color);
      };
      if (over_board) {
        outline(x, y, GREEN);
      }
      if (selected_x >= 0) {
        outline(selected_x, selected_y, ORANGE);
        DrawText(TextFormat("Cell: (%d, %d)", selected_x, selected_y), offset, 400, 20, BLACK);
      }
      for (const auto& player : game.players)
        DrawCircle(offset + player.position.x * cell_size + cell_size / 2,
                   offset + player.position.y * cell_size + cell_size / 2,
                   cell_size / 2 - 5, player.id == 1 ? RED : BLUE);
    }
    int row_y = offset;
    for (const auto& member : lobby.players) {
      std::string label = member.name + (member.id == 1 ? " (RED)" : " (BLUE)");
      if (match_active) {
        auto player = std::find_if(game.players.begin(), game.players.end(),
          [&](const auto& p) { return p.id == member.id; });
        if (player != game.players.end()) {
          label += ": HP: " + std::to_string(player->hp);
        }
        if (game.turn == member.id) {
          label += " [TURN]";
        }
      } else if (member.ready) {
        label += " [READY]";
      }
      if (member.id == lobby.playerId) {
        label += " [YOU]";
      }
      DrawTextEx(name_font, label.c_str(), {float(panel), float(row_y)}, 20, 1, BLACK);
      row_y += 30;
    }
    DrawText(status.c_str(), panel, 115, 16, DARKGRAY);
    if (online && match_active) {
      DrawText("Left click: move   Right click: attack", panel, 150, 16, BLACK);
      DrawText("T: end turn", panel, 180, 16, BLACK);
    } else if (online && lobby.playerId) {
      DrawText(ready_sent ? "Waiting for opponent..." : "R: ready", panel, 150, 16, BLACK);
    }
    EndDrawing();
  }
  close_client_connection(client_socket);
  if (network_started) {
    cleanup_client_network();
  }
  if (name_font.texture.id != GetFontDefault().texture.id) {
    UnloadFont(name_font);
  }
  UnloadMusicStream(music);
  CloseAudioDevice();
  CloseWindow();
  return 0;
}
