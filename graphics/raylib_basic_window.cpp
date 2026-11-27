#include "client_network.hpp"
#include "raylib.h"
#include "common/protocol.hpp"
#include "client/client_state.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

Font loadNameFont()
{
#ifdef _WIN32
    const char* windows = std::getenv("WINDIR");
    if (windows) {
        const std::string path = std::string(windows) + "/Fonts/segoeui.ttf";
        std::vector<int> glyphs;
        for (int ch = 32; ch <= 126; ++ch) {
            glyphs.push_back(ch);
        }
        for (int ch = 0x400; ch <= 0x4ff; ++ch) {
            glyphs.push_back(ch);
        }
        const Font font = LoadFontEx(
            path.c_str(),
            24,
            glyphs.data(),
            static_cast<int>(glyphs.size())
        );
        if (font.texture.id != 0) {
            return font;
        }
    }
#endif
    return GetFontDefault();
}

bool validNickname(const std::string& name)
{
    return name.find_first_not_of(' ') != std::string::npos
        && name.size() <= 1019
        && name.find_first_of("\r\n") == std::string::npos;
}

std::string readNickname(Font font, const char* argument)
{
    std::string name;
    if (argument && validNickname(argument)) {
        return argument;
    }

    while (!WindowShouldClose()) {
        for (int codepoint = GetCharPressed(); codepoint != 0;
             codepoint = GetCharPressed()) {
            int byteCount = 0;
            const char* encoded = CodepointToUTF8(codepoint, &byteCount);
            if (codepoint >= 32 && name.size() + byteCount <= 1019) {
                name.append(encoded, static_cast<std::size_t>(byteCount));
            }
        }

        if (IsKeyPressed(KEY_BACKSPACE) && !name.empty()) {
            std::size_t lastCodepoint = name.size() - 1;
            while (lastCodepoint > 0
                   && (static_cast<unsigned char>(name[lastCodepoint]) & 0xc0) == 0x80) {
                --lastCodepoint;
            }
            name.erase(lastCodepoint);
        }

        if (IsKeyPressed(KEY_ENTER) && validNickname(name)) {
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

}

int main(int argc, char* argv[])
{
    constexpr int screenWidth = 800;
    constexpr int screenHeight = 450;
    constexpr int cellSize = 64;
    constexpr int offset = 40;
    const std::string serverHost = argc > 2 ? argv[2] : "127.0.0.1";

    InitWindow(screenWidth, screenHeight, "ToDA 3");
    SetTargetFPS(60);

    const Font nameFont = loadNameFont();
    const std::string nickname = readNickname(
        nameFont,
        argc > 1 ? argv[1] : nullptr
    );

    ClientSocket clientSocket = INVALID_CLIENT_SOCKET;
    std::string status = nickname.empty()
        ? "Nickname is required"
        : "Connecting to " + serverHost + ":2024...";
    std::string serverPending;
    LobbyState lobby{};
    GameState game{};
    bool readySent = false;
    bool matchActive = false;
    int selectedX = -1;
    int selectedY = -1;

    const bool networkStarted = !nickname.empty()
        && open_client_connection(clientSocket, serverHost, status);
    auto sendCommand = [&](const std::string& command) {
        if (!send_all(clientSocket, command + "\n", status)) {
            close_client_connection(clientSocket);
            return false;
        }
        return true;
    };
    if (networkStarted && sendCommand("JOIN " + nickname)) {
        status = "Joining lobby...";
    }

    while (!nickname.empty() && !WindowShouldClose()) {
        if (clientSocket != INVALID_CLIENT_SOCKET) {
            std::vector<std::string> lines;
            bool disconnected = false;
            if (!receive_available(
                    clientSocket,
                    serverPending,
                    lines,
                    disconnected,
                    status
                ) || disconnected) {
                if (disconnected) {
                    status = "Server disconnected";
                }
                close_client_connection(clientSocket);
            }

            for (const std::string& line : lines) {
                if (line.rfind("LOBBY_STATE ", 0) == 0) {
                    if (!parseLobbyState(line.substr(12), lobby)) {
                        status = "Invalid lobby state";
                    }
                } else if (line == "MATCH_START") {
                    matchActive = true;
                    game = {};
                    selectedX = -1;
                    selectedY = -1;
                    status = "Match started";
                } else if (line.rfind("GAME_STATE ", 0) == 0) {
                    if (!parseGameState(line.substr(11), game)) {
                        status = "Invalid game state";
                    }
                } else if (line.rfind("GAME_OVER ", 0) == 0
                           || line == "MATCH_ABORTED") {
                    matchActive = false;
                    readySent = false;
                    status = line == "MATCH_ABORTED"
                        ? "Opponent disconnected"
                        : line.substr(10) == std::to_string(lobby.playerId)
                            ? "You won!"
                            : "You lost.";
                } else {
                    const std::string response = formatServerResponse(line);
                    status = response.empty() ? line : response;
                }
            }
        }

        const bool online = clientSocket != INVALID_CLIENT_SOCKET;
        if (online && !matchActive && lobby.playerId != 0
            && !readySent && IsKeyPressed(KEY_R)) {
            readySent = sendCommand("READY");
        }
        if (online && matchActive && IsKeyPressed(KEY_T)
            && validateBattleCommand("END_TURN", lobby, game, matchActive, status)) {
            sendCommand("END_TURN");
        }

        const int mouseX = GetMouseX() - offset;
        const int mouseY = GetMouseY() - offset;
        const bool overBoard = matchActive
            && mouseX >= 0
            && mouseY >= 0
            && mouseX < game.width * cellSize
            && mouseY < game.height * cellSize;
        const int cellX = mouseX / cellSize;
        const int cellY = mouseY / cellSize;
        if (online && overBoard) {
            const bool attack = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
            if (attack || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                selectedX = cellX;
                selectedY = cellY;
                const std::string command = std::string(attack ? "ATTACK " : "MOVE ")
                    + std::to_string(cellX) + " " + std::to_string(cellY);
                if (validateBattleCommand(
                        command,
                        lobby,
                        game,
                        matchActive,
                        status
                    )) {
                    sendCommand(command);
                }
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        int panelX = offset;
        if (matchActive) {
            panelX += game.width * cellSize + 24;
            for (int row = 0; row < game.height; ++row) {
                for (int col = 0; col < game.width; ++col) {
                    DrawRectangleLines(
                        offset + col * cellSize,
                        offset + row * cellSize,
                        cellSize,
                        cellSize,
                        LIGHTGRAY
                    );
                }
            }
            for (const Position& blocked : game.blocked) {
                DrawRectangle(
                    offset + blocked.x * cellSize + 1,
                    offset + blocked.y * cellSize + 1,
                    cellSize - 2,
                    cellSize - 2,
                    LIGHTGRAY
                );
            }
            const auto outlineCell = [&](int x, int y, Color color) {
                DrawRectangleLinesEx(
                    {
                        static_cast<float>(offset + x * cellSize),
                        static_cast<float>(offset + y * cellSize),
                        static_cast<float>(cellSize),
                        static_cast<float>(cellSize)
                    },
                    3,
                    color
                );
            };
            if (overBoard) {
                outlineCell(cellX, cellY, GREEN);
            }
            if (selectedX >= 0) {
                outlineCell(selectedX, selectedY, ORANGE);
                DrawText(
                    TextFormat("Cell: (%d, %d)", selectedX, selectedY),
                    offset,
                    400,
                    20,
                    BLACK
                );
            }
            for (const GamePlayerState& player : game.players) {
                DrawCircle(
                    offset + player.position.x * cellSize + cellSize / 2,
                    offset + player.position.y * cellSize + cellSize / 2,
                    cellSize / 2 - 5,
                    player.id == 1 ? RED : BLUE
                );
            }
        }

        int rowY = offset;
        for (const LobbyPlayerState& member : lobby.players) {
            std::string label = member.name + (member.id == 1 ? " (RED)" : " (BLUE)");
            if (matchActive) {
                const auto player = std::find_if(
                    game.players.begin(),
                    game.players.end(),
                    [&](const GamePlayerState& candidate) {
                        return candidate.id == member.id;
                    }
                );
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
            DrawTextEx(
                nameFont,
                label.c_str(),
                {static_cast<float>(panelX), static_cast<float>(rowY)},
                20,
                1,
                BLACK
            );
            rowY += 30;
        }
        DrawText(status.c_str(), panelX, 115, 16, DARKGRAY);
        if (online && matchActive) {
            DrawText("Left click: move   Right click: attack", panelX, 150, 16, BLACK);
            DrawText("T: end turn", panelX, 180, 16, BLACK);
        } else if (online && lobby.playerId != 0) {
            DrawText(
                readySent ? "Waiting for opponent..." : "R: ready",
                panelX,
                150,
                16,
                BLACK
            );
        }
        EndDrawing();
    }

    close_client_connection(clientSocket);
    if (networkStarted) {
        cleanup_client_network();
    }
    if (nameFont.texture.id != GetFontDefault().texture.id) {
        UnloadFont(nameFont);
    }
    CloseWindow();
    return 0;
}
