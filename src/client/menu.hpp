#pragma once

#include "../common/protocol.hpp"

#include <string>
#include <vector>

enum class MenuScreen {
    Main,
    Inventory,
    Actions
};

enum class MainButton {
    Inventory,
    Action,
    Surrender
};

struct InventoryItem {
    int id;
    std::string name;
};

class Menu {
public:
    Menu();

    MenuScreen getScreen() const;
    std::size_t getSelectedOption() const;
    const std::vector<std::string>& getVisibleOptions() const;

    void setInventory(std::vector<InventoryItem> items);
    void openMain();
    void moveSelectionUp();
    void moveSelectionDown();

    void selectMainButton(MainButton button);
    void openInventory();
    void openActions();
    void closeSubmenu();

    UseItemRequest createUseItemRequest(int characterId) const;
    TurnAction getSelectedAction() const;
    MoveRequest createMoveRequest(int characterId, Position destination) const;
    AttackRequest createAttackRequest(int characterId, Position target) const;
    SurrenderRequest createSurrenderRequest(int characterId) const;

private:
    MenuScreen screen;
    std::size_t selectedOption;
    std::vector<InventoryItem> inventory;
    std::vector<std::string> visibleOptions;
};
