#pragma once

#include <bitset>
#include <map>
#include <numeric>

#include <BWAPI.h>

#include "AbstractOrder.h"
#include "GameState.h"
//#include "InformationManager.h"

static enum probName {
    Idle, Attack,
    Move0000, Move0001, Move0010, Move0011,
    Move0100, Move0101, Move0110, Move0111,
    Move1000, Move1001, Move1010, Move1011,
    Move1100, Move1101, Move1110, Move1111
};

class ActionGenerator
{
public:
    const GameState* _gs;
    double _size;
    // TODO change to _isFriendly
    bool _player; // true==friendly, false==enemy
    playerActions_t _lastAction;

    ActionGenerator();
    ActionGenerator(const GameState* gs, bool player = true);
    void cleanActions();
    playerActions_t getNextAction();
    playerActions_t getRandomAction();
    playerActions_t getUniqueRandomAction();
    playerActions_t getBiasAction();
    playerActions_t getMostProbAction();
    playerActions_t getNextActionProbability(double& prob);
    std::string toString();
    static std::string toString(playerActions_t playerActions);
    bool hasMoreActions() { return (_moreActions && !_choices.empty()); }

    double getHighLevelFriendlyActions();
    double getHighLevelEnemyActions();
    double getLowLevelFriendlyActions();
    double getLowLevelEnemyActions();
    // TODO why sparcraft? is it from there?
    double getSparcraftFriendlyActions();
    double getSparcraftEnemyActions();
    double getLowLevelActions(BWAPI::Unitset units);
    double getSparcraftActions(BWAPI::Unitset units);

private:
    const std::vector<unitGroup_t*>* _myUnitsList;
    const std::vector<unitGroup_t*>* _enemyUnitsList;
    choices_t _choices;
    std::vector<int> _choiceSizes;
    std::vector<int> _currentChoice;
    bool _moreActions;
    std::set<size_t> _choicesGenerated;

    std::vector<action_t> getUnitActions(unitGroup_t* unit);
    void incrementChoice(std::vector<int>& currentChoice, size_t startPosition = 0);

    double getHighLevelActions(std::vector<unitGroup_t*> unitList);
    playerActions_t getActionsFromChoices(std::vector<int> choiceSelected);
    size_t getHashFromChoicesSelected(std::vector<int> choiceSelected);
    std::vector<int> getRandomChoices();
    std::vector<int> getBiasChoices();
    std::vector<int> getBestChoices();
    double getActionProbability(action_t action, std::map<probName, uint8_t> possibleActions, unitGroup_t* currentGroup);
    std::map<probName, uint8_t> getAllPosibleActions(std::vector<action_t> actions, unitGroup_t* currentGroup);
    double getProbabilityGivenState(probName action, std::map<probName, uint8_t> possibleActions);
    bool isMovingTowardsBase(const std::vector<unitGroup_t*>* groupList, uint8_t currentRegion, uint8_t targetRegion);
    std::string getMoveFeatures(uint8_t regionID, unitGroup_t* currentGroup);
};
