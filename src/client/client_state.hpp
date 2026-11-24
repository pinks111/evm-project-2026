#pragma once

#include "../common/protocol.hpp"

#include <string>

bool parseLobbyState(const std::string& jsonText, LobbyState& state);
bool parseGameState(const std::string& jsonText, GameState& state);
bool validateBattleCommand(
    const std::string& input,
    const LobbyState& lobby,
    const GameState& game,
    bool matchActive,
    std::string& error
);
std::string formatServerResponse(const std::string& line);

void printLobbyState(const LobbyState& state);
void printGameState(const GameState& state);
