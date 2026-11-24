#include "client_state.hpp"

#include <nlohmann/json.hpp>

#include <iostream>
#include <sstream>

bool parseLobbyState(const std::string& jsonText, LobbyState& state)
{
    try {
        const nlohmann::json json = nlohmann::json::parse(jsonText);

        state.playerId = json.at("you").get<int>();
        state.players.clear();

        for (const nlohmann::json& playerJson : json.at("players")) {
            state.players.push_back({
                playerJson.at("id").get<int>(),
                playerJson.at("name").get<std::string>(),
                playerJson.at("ready").get<bool>()
            });
        }
        return true;
    } catch (const nlohmann::json::exception&) {
        return false;
    }
}

bool parseGameState(const std::string& jsonText, GameState& state)
{
    try {
        const nlohmann::json json = nlohmann::json::parse(jsonText);

        state.width = json.at("width").get<int>();
        state.height = json.at("height").get<int>();
        state.turn = json.at("turn").get<int>();
        state.canMove = json.at("can_move").get<bool>();
        state.canAttack = json.at("can_attack").get<bool>();

        state.blocked.clear();
        for (const nlohmann::json& blocked : json.at("blocked")) {
            state.blocked.push_back({
                blocked.at(0).get<int>(),
                blocked.at(1).get<int>()
            });
        }

        state.players.clear();
        for (const nlohmann::json& playerJson : json.at("players")) {
            state.players.push_back({
                playerJson.at("id").get<int>(),
                {
                    playerJson.at("x").get<int>(),
                    playerJson.at("y").get<int>()
                },
                playerJson.at("hp").get<int>()
            });
        }
        return true;
    } catch (const nlohmann::json::exception&) {
        return false;
    }
}

    bool validateBattleCommand(
        const std::string& input,
        const LobbyState& lobby,
        const GameState& game,
        bool matchActive,
        std::string& error
    )
    {
        std::istringstream stream(input);
        std::string command;
        stream >> command;

        if (command == "READY") {
            std::string extra;
            if (stream >> extra) {
                error = "Usage: READY";
                return false;
            }
            return true;
        }

        if (command == "END_TURN") {
            std::string extra;
            if (stream >> extra) {
                error = "Usage: END_TURN";
                return false;
            }
            if (!matchActive) {
                error = "You are not in a match";
                return false;
            }
            if (lobby.playerId == 0 || game.turn != lobby.playerId) {
                error = "It is not your turn";
                return false;
            }
            return true;
        }

        if (command == "JOIN") {
            std::string name;
            if (!(stream >> name)) {
                error = "Usage: JOIN name";
                return false;
            }
            return true;
        }

        if (command != "MOVE" && command != "ATTACK") {
            return true;
        }

        int x;
        int y;
        std::string extra;
        if (!(stream >> x >> y) || (stream >> extra)) {
            error = "Usage: " + command + " x y";
            return false;
        }

        if (!matchActive) {
            error = "You are not in a match";
            return false;
        }
        if (lobby.playerId == 0 || game.turn != lobby.playerId) {
            error = "It is not your turn";
            return false;
        }
        if (x < 0 || x >= game.width || y < 0 || y >= game.height) {
            error = "Coordinates must be inside the field";
            return false;
        }
        if (command == "MOVE" && !game.canMove) {
            error = "You have already moved this turn";
            return false;
        }
        if (command == "ATTACK" && !game.canAttack) {
            error = "You have already attacked this turn";
            return false;
        }

        return true;
    }

    std::string formatServerResponse(const std::string& line)
    {
        if (line.rfind("OK ", 0) == 0) {
            return "Accepted: " + line.substr(3);
        }
        if (line.rfind("ERR_", 0) != 0) {
            return "";
        }

        if (line == "ERR_ALREADY_JOINED") {
            return "Error: this connection has already joined";
        }
        if (line == "ERR_JOIN_FIRST") {
            return "Error: join with JOIN name first";
        }
        if (line == "ERR_NOT_IN_MATCH") {
            return "Error: match has not started";
        }
        if (line == "ERR_INVALID_MOVE") {
            return "Error: invalid move";
        }
        if (line == "ERR_INVALID_ATTACK") {
            return "Error: invalid attack";
        }
        if (line == "ERR_NOT_YOUR_TURN") {
            return "Error: it is not your turn";
        }
        if (line == "ERR_UNKNOWN_COMMAND") {
            return "Error: unknown command";
        }
        return "Error: " + line;
    }

void printLobbyState(const LobbyState& state)
{
    std::cout << "\nLobby (you = " << state.playerId << ")\n";
    if (state.players.empty()) {
        std::cout << "No players joined\n";
        return;
    }

    for (const LobbyPlayerState& player : state.players) {
        std::cout << player.id << ": " << player.name
                  << " - " << (player.ready ? "READY" : "NOT READY") << '\n';
    }
}

void printGameState(const GameState& state)
{
    std::cout << "\nGame field\n";
    for (int y = 0; y < state.height; ++y) {
        for (int x = 0; x < state.width; ++x) {
            bool blocked = false;
            for (const Position& position : state.blocked) {
                if (position.x == x && position.y == y) {
                    blocked = true;
                    break;
                }
            }

            if (blocked) {
                std::cout << " X ";
                continue;
            }

            bool playerPrinted = false;
            for (const GamePlayerState& player : state.players) {
                if (player.position.x == x && player.position.y == y) {
                    std::cout << " P" << player.id << " ";
                    playerPrinted = true;
                    break;
                }
            }

            if (!playerPrinted) {
                std::cout << " . ";
            }
        }
        std::cout << '\n';
    }

    std::cout << "Turn: " << state.turn
              << ", move: " << (state.canMove ? "yes" : "no")
              << ", attack: " << (state.canAttack ? "yes" : "no") << '\n';
    for (const GamePlayerState& player : state.players) {
        std::cout << "P" << player.id << " HP: " << player.hp
                  << " ( " << player.position.x
                  << ", " << player.position.y << " )\n";
    }
}
