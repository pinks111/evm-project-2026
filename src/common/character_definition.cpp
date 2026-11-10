#include "character_definition.hpp"

const std::vector<CharacterDefinition>& CharacterCatalog::getAll()
{
    static const std::vector<CharacterDefinition> definitions{
        {1, "Warrior", 150, 20, 1},
        {2, "Archer", 80, 30, 5},
        {3, "Mage", 100, 40, 4}
    };

    return definitions;
}

const CharacterDefinition* CharacterCatalog::findById(int id)
{
    for (const CharacterDefinition& definition : getAll()) {
        if (definition.id == id) {
            return &definition;
        }
    }

    return nullptr;
}
