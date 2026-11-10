#include "menu.hpp"

#include <stdexcept>
#include <utility>

namespace {

const std::vector<std::string> mainOptions{
    "Inventory",
    "Action",
    "Surrender"
};

const std::vector<std::string> actionOptions{
    "Move",
    "Attack",
    "Pass"
};

}

Menu::Menu()
    : screen(MenuScreen::Main),
      selectedOption(0),
      visibleOptions(mainOptions)
{
}

MenuScreen Menu::getScreen() const
{
    return screen;
}

std::size_t Menu::getSelectedOption() const
{
    return selectedOption;
}

const std::vector<std::string>& Menu::getVisibleOptions() const
{
    return visibleOptions;
}

void Menu::setInventory(std::vector<InventoryItem> items)
{
    inventory = std::move(items);
}

void Menu::openMain()
{
    screen = MenuScreen::Main;
    selectedOption = 0;
    visibleOptions = mainOptions;
}

void Menu::moveSelectionUp()
{
    if (visibleOptions.empty()) {
        return;
    }

    selectedOption = selectedOption == 0
        ? visibleOptions.size() - 1
        : selectedOption - 1;
}

void Menu::moveSelectionDown()
{
    if (visibleOptions.empty()) {
        return;
    }

    selectedOption = (selectedOption + 1) % visibleOptions.size();
}

void Menu::selectMainButton(MainButton button)
{
    switch (button) {
    case MainButton::Inventory:
        openInventory();
        break;
    case MainButton::Action:
        openActions();
        break;
    case MainButton::Surrender:
        break;
    }
}

void Menu::openInventory()
{
    screen = MenuScreen::Inventory;
    selectedOption = 0;
    visibleOptions.clear();
    for (const InventoryItem& item : inventory) {
        visibleOptions.push_back(item.name);
    }
}

void Menu::openActions()
{
    screen = MenuScreen::Actions;
    selectedOption = 0;
    visibleOptions = actionOptions;
}

void Menu::closeSubmenu()
{
    openMain();
}

UseItemRequest Menu::createUseItemRequest(int characterId) const
{
    if (screen != MenuScreen::Inventory
        || selectedOption >= inventory.size()) {
        throw std::logic_error("No inventory item is selected");
    }

    return {characterId, inventory[selectedOption].id};
}

TurnAction Menu::getSelectedAction() const
{
    if (screen != MenuScreen::Actions || selectedOption >= actionOptions.size()) {
        throw std::logic_error("No action is selected");
    }

    return static_cast<TurnAction>(selectedOption);
}

MoveRequest Menu::createMoveRequest(int characterId, Position destination) const
{
    if (getSelectedAction() != TurnAction::Move) {
        throw std::logic_error("Move action is not selected");
    }

    return {characterId, destination};
}

AttackRequest Menu::createAttackRequest(int characterId, Position target) const
{
    if (getSelectedAction() != TurnAction::Attack) {
        throw std::logic_error("Attack action is not selected");
    }

    return {characterId, target};
}

SurrenderRequest Menu::createSurrenderRequest(int characterId) const
{
    if (screen != MenuScreen::Main
        || selectedOption != static_cast<std::size_t>(MainButton::Surrender)) {
        throw std::logic_error("Surrender is not selected");
    }

    return {characterId};
}
