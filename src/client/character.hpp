#include "../common/protocol.hpp"

class Character {
public:
    explicit Character(CharacterState state);

    int getId() const;
    const std::string& getName() const;
    int getHp() const;
    int getMaxHp() const;
    int getDamage() const;
    int getRange() const;
    Position getPosition() const;

    MoveRequest createMoveRequest(Position destination) const;
    UpgradeRequest createUpgradeRequest(UpgradeStat stat) const;
    AttackRequest createAttackRequest(Position target) const;
    void applyServerState(CharacterState state);

private:
    CharacterState state;
};