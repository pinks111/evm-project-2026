#pragma once

#include <string>
#include <vector>

struct CharacterDefinition {
    int id;
    std::string name;
    int maxHp;
    int damage;
    int range;
};

class CharacterCatalog {
public:
    static const std::vector<CharacterDefinition>& getAll();
    static const CharacterDefinition* findById(int id);
};
