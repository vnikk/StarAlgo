#pragma once

#include <bitset>
#include <map>
#include <numeric>

#include <BWAPI.h>

#include "AbstractOrder.h"

class GameState;

// © Alberto Uriarte
static enum probName {
    Idle, Attack,
    Move0000, Move0001, Move0010, Move0011,
    Move0100, Move0101, Move0110, Move0111,
    Move1000, Move1001, Move1010, Move1011,
    Move1100, Move1101, Move1110, Move1111
};

// © me & Alberto Uriarte
class ActionGenerator
{
public:
    ActionGenerator(const GameState* gs, bool player = true);
    std::string toString();
    static std::string toString(const playerActions_t& playerActions);

    const GameState* gameState;
    bool             isFriendly;
    size_t           actionsSize;
    playerActions_t  lastActions;

    void            cleanActions();
    playerActions_t getNextAction();
    playerActions_t getRandomAction();
    playerActions_t getUniqueRandomAction();
    playerActions_t getBiasAction();
    playerActions_t getMostProbAction();
    playerActions_t getNextActionProbability(double& prob);
    bool            hasMoreActions() { return (moreActions && !choices.empty()); }

    int    getHighLevelFriendlyActions();
    int    getHighLevelEnemyActions();
    double getLowLevelFriendlyActions();
    double getLowLevelEnemyActions();
    double getLowLevelActions(const BWAPI::Unitset& units);
    int    getSparcraftFriendlyActions();
    int    getSparcraftEnemyActions();
    int    getSparcraftActions(const BWAPI::Unitset& units);

private:
    const std::vector<unitGroup_t*>* myUnitsList;
    const std::vector<unitGroup_t*>* enemyUnitsList;

    bool             moreActions;
    choices_t        choices;
    std::vector<int> choiceSizes;
    std::vector<int> currentChoice;
    std::set<size_t> choicesGenerated;

    std::vector<action_t> getUnitActions(unitGroup_t* unit);
    void incrementChoice(std::vector<int>& currentChoice, size_t startPosition = 0);
    std::vector<int> getRandomChoices();
    std::vector<int> getBiasChoices();
    std::vector<int> getBestChoices();

    int getHighLevelActions(const std::vector<unitGroup_t*>& unitList);
    playerActions_t getActionsFromChoices(const std::vector<int>& choiceSelected);
    size_t getHashFromChoicesSelected(const std::vector<int>& choiceSelected);
    double getActionProbability(action_t action, const std::map<probName, uint8_t>& possibleActions, unitGroup_t* currentGroup);
    std::map<probName, uint8_t> getAllPosibleActions(const std::vector<action_t>& actions, unitGroup_t* currentGroup);
    double getProbabilityGivenState(probName action, const std::map<probName, uint8_t>& possibleActions);
    bool isMovingTowardsBase(const std::vector<unitGroup_t*>* groupList, uint8_t currentRegion, uint8_t targetRegion);
    std::string getMoveFeatures(uint8_t regionID, unitGroup_t* currentGroup);
};
