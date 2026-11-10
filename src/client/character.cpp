#include "character.hpp"

Character::Character(CharacterState state)
    : state(state)
{
}

int Character::getId() const
{
    return state.id;
}

const std::string& Character::getName() const
{
    return state.name;
}

int Character::getHp() const
{
    return state.hp;
}

int Character::getMaxHp() const
{
    return state.maxHp;
}

int Character::getDamage() const
{
    return state.damage;
}

int Character::getRange() const
{
    return state.range;
}

Position Character::getPosition() const
{
    return state.position;
}

MoveRequest Character::createMoveRequest(Position destination) const
{
    return {state.id, destination};
}

UpgradeRequest Character::createUpgradeRequest(UpgradeStat stat) const
{
    return {state.id, stat};
}

AttackRequest Character::createAttackRequest(Position target) const
{
    return {state.id, target};
}

void Character::applyServerState(CharacterState newState)
{
    state = newState;
}