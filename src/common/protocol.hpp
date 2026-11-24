#pragma once

#include "utils.hpp"

#include <string>
#include <vector>

enum class UpgradeStat {
    Hp,
    Damage,
    Range
};

enum class TurnAction {
    Move,
    Attack,
    Pass
};

struct CharacterState {
    int id;
    std::string name;
    int hp;
    int maxHp;
    int damage;
    int range;
    Position position;
};

struct MoveRequest {
    int characterId;
    Position destination;
};

struct MoveResponse {
    bool accepted;
    CharacterState state;
    std::string reason;
};

struct AttackResponse {
    bool accepted;
    CharacterState attacker;
    CharacterState target;
    std::string reason;
};

struct UseItemResponse {
    bool accepted;
    CharacterState state;
    std::string reason;
};

struct UpgradeResponse {
    bool accepted;
    CharacterState state;
    int remainingPoints;
    std::string reason;
};

struct SurrenderResponse {
    bool accepted;
    std::string reason;
};

struct UpgradeRequest {
    int characterId;
    UpgradeStat stat;
};

struct AttackRequest {
    int characterId;
    Position target;
};

struct EndTurnRequest {
    int playerId;
};

struct UseItemRequest {
    int characterId;
    int itemId;
};

struct SurrenderRequest {
    int characterId;
};

struct LobbyPlayerState {
    int id;
    std::string name;
    bool ready;
};

struct LobbyState {
    int playerId;
    std::vector<LobbyPlayerState> players;
};

struct GamePlayerState {
    int id;
    Position position;
    int hp;
};

struct GameState {
    int width;
    int height;
    std::vector<Position> blocked;
    int turn;
    bool canMove;
    bool canAttack;
    std::vector<GamePlayerState> players;
};
