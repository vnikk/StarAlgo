#include "stdafx.h"

#include "MCTSCD.h"
#include "GameState.h"
#include "ActionGenerator.h"
#include "AbstractOrder.h"
#include "ActionProbabilities.h"

#include <BWTA.h>
#include <iostream>

using namespace BWAPI;

// © me & Alberto Uriarte
ActionGenerator::ActionGenerator(const GameState* gs, bool player)
: gameState(gs), actionsSize(1), isFriendly(player)
{
    // set myUnitsList and enemyUnitList depends on the player
    if (isFriendly) {
        myUnitsList = &gameState->army.friendly; // ref?
        enemyUnitsList = &gameState->army.enemy;
    } else {
        myUnitsList = &gameState->army.enemy;
        enemyUnitsList = &gameState->army.friendly;
    }

    // Generate all possible choices
    choices.clear();
    for (size_t i = 0; i < myUnitsList->size(); ++i) {
        if ((*myUnitsList)[i]->endFrame <= gameState->time) {
            unitGroup_t* unit = (*myUnitsList)[i];
            std::vector<action_t> &actions = getUnitActions(unit);
#ifdef DEBUG_ORDERS
            choices.push_back(choice_t(i, actions, unit->unitTypeId, unit->regionId, player));
#else
            choices.push_back(choice_t(i, actions));
#endif
            actionsSize *= actions.size();
        }
    }

    if (!choices.empty()) {
        moreActions = true;
        choiceSizes.resize(choices.size());
        for (size_t i = 0; i < choices.size(); ++i) {
            choiceSizes[i] = choices[i].actions.size();
        }
        currentChoice.resize(choices.size(), 0);
    } else {
        moreActions = false;
        actionsSize = 0;
    }
}

// © Alberto Uriarte
// Warning!! This will clean the actions of the original game state!
void ActionGenerator::cleanActions()
{
    for (auto& unitGroup : *myUnitsList) {
        unitGroup->endFrame = -1;
        unitGroup->orderId = abstractOrder::Unknown;
    }
}

// © me & Alberto Uriarte
std::vector<action_t> ActionGenerator::getUnitActions(unitGroup_t* unit)
{
    std::vector<action_t> possibleActions;

    int typeId = unit->unitTypeId;
    // auto-unsiege
    if (typeId == UnitTypes::Terran_Siege_Tank_Siege_Mode) {
        typeId = UnitTypes::Terran_Siege_Tank_Tank_Mode;
    }
    BWAPI::UnitType unitType(typeId);

    if (unitType.isBuilding()) {
        possibleActions.push_back(action_t(abstractOrder::Nothing, unit->regionId));
        return possibleActions;
    }

    // Move
    // --------------------------------
    // get neighbors from a region or chokepoint
    if (unitType.canMove()) {
        auto region = gameState->regman->regionFromID.find(unit->regionId);
        if (region != gameState->regman->regionFromID.end()) {
            if (!gameState->regman->onlyRegions) {
                for (const auto& c : region->second->getChokepoints()) {
                    possibleActions.push_back(action_t(abstractOrder::Move, gameState->regman->chokePointID.at(c)));
                }
            } else {
                std::vector<int> duplicates; // sorted vector to lookup duplicate regionID
                std::vector<int>::iterator i;
                int regionIDtoAdd;
                for (const auto& c : region->second->getChokepoints()) {
                    const auto regions = c->getRegions();
                    regionIDtoAdd = gameState->regman->regionID.at(regions.first);
                    if (gameState->regman->regionID.at(regions.first) == unit->regionId) {
                        regionIDtoAdd = gameState->regman->regionID.at(regions.second);
                    }
                    i = std::lower_bound(duplicates.begin(), duplicates.end(), regionIDtoAdd);
                    if (i == duplicates.end() || regionIDtoAdd < *i) {
                        // insert if no duplicates
                        duplicates.insert(i, regionIDtoAdd);
                        possibleActions.push_back(action_t(abstractOrder::Move, regionIDtoAdd));
                    }
                }
            }
        } else {
            auto cp = gameState->regman->chokePointFromID.find(unit->regionId);
            if (cp != gameState->regman->chokePointFromID.end()) {
                const auto regions = cp->second->getRegions();
                possibleActions.push_back(action_t(abstractOrder::Move, gameState->regman->regionID.at(regions.first)));
                possibleActions.push_back(action_t(abstractOrder::Move, gameState->regman->regionID.at(regions.second)));
            }
        }
    }

    // Attack
    // --------------------------------
    // only if there are enemies in the region that we can attack
    if (unitType.canAttack()) {
        // if there are enemies in the same region, ATTACK is the only option
        for (const auto& enemyGroup : *enemyUnitsList) {
            if (enemyGroup->regionId == unit->regionId && canAttackType(unitType, UnitType(enemyGroup->unitTypeId))) {
                std::vector<action_t> onlyAttack{ action_t(abstractOrder::Attack, unit->regionId) };
                return onlyAttack;
            }
        }
    }

    // Nothing
    // --------------------------------
    possibleActions.push_back(action_t(abstractOrder::Idle, unit->regionId));

    return possibleActions;
}

// © Alberto Uriarte
void ActionGenerator::incrementChoice(std::vector<int>& currentChoice, size_t startPosition)
{
    for (size_t i = 0; i < startPosition; ++i) { currentChoice[i] = 0; }
    currentChoice[startPosition]++;
    if (currentChoice[startPosition] >= choiceSizes[startPosition]) {
        if (startPosition < currentChoice.size()-1) {
            incrementChoice(currentChoice, startPosition + 1);
        } else {
            moreActions = false;
            for (size_t i = 0; i<currentChoice.size(); ++i) currentChoice[i] = 0;
        }
    }
}

// © me & Alberto Uriarte
playerActions_t ActionGenerator::getNextAction()
{
    if (moreActions) {
        lastActions = getActionsFromChoices(currentChoice);
        incrementChoice(currentChoice);
        return lastActions;
    }
    return playerActions_t();
}

// © Alberto Uriarte
playerActions_t ActionGenerator::getActionsFromChoices(const std::vector<int>& choiceSelected)
{
    playerActions_t playerActions;
    playerActions.reserve(choices.size());
    for (size_t i = 0; i < choices.size(); ++i) {
        playerActions.emplace_back(playerAction_t(choices[i], choiceSelected[i]));
    }
    return playerActions;
}

// © Alberto Uriarte
playerActions_t ActionGenerator::getRandomAction()
{
    std::vector<int> randomChoices(getRandomChoices());
    lastActions = getActionsFromChoices(randomChoices);
    return lastActions;
}

// © Alberto Uriarte
std::vector<int> ActionGenerator::getRandomChoices()
{
    std::vector<int> randomChoices;
    randomChoices.reserve(choices.size());
    for (size_t i = 0; i < choices.size(); ++i) {
        std::uniform_int_distribution<int> uniformDist(0, choiceSizes[i] - 1);
        randomChoices.emplace_back(uniformDist(gen));
    }
    return randomChoices;
}

// © Alberto Uriarte
playerActions_t ActionGenerator::getBiasAction()
{
    std::vector<int> randomChoices = getBiasChoices();
    lastActions = getActionsFromChoices(randomChoices);
    return lastActions;
}

// © me & Alberto Uriarte
std::vector<int> ActionGenerator::getBiasChoices()
{
    std::vector<int> randomChoices;
    randomChoices.reserve(choices.size());
    for (size_t i = 0; i < choices.size(); ++i) {

        std::map<probName, uint8_t> posibleActions = getAllPosibleActions(choices[i].actions, myUnitsList->at(choices[i].pos));
        // select an action with probabilities learned
        std::vector<double> probability(choiceSizes[i]);
        for (int j = 0; j < choiceSizes[i]; ++j) {
            probability[j] = getActionProbability(choices[i].actions[j], posibleActions, myUnitsList->at(choices[i].pos));
        }

        // get choice from normalized probabilities
        double totalProb = std::accumulate(probability.begin(), probability.end(), 0.0);

        std::uniform_real_distribution<double> uniformDist(0, 1);
        double randomProb = uniformDist(gen);
        double accumProb = 0.0;
        for (size_t j = 0; j < choiceSizes[i]; ++j) {
            accumProb += probability[j];
            if (randomProb <= accumProb / totalProb) {
                randomChoices.emplace_back(j);
                break;
            }
        }
    }
    return randomChoices;
}

// © Alberto Uriarte
playerActions_t ActionGenerator::getMostProbAction()
{
    std::vector<int> bestChoices(getBestChoices());
    lastActions = getActionsFromChoices(bestChoices);
    return lastActions;
}

// © me & Alberto Uriarte
std::vector<int> ActionGenerator::getBestChoices()
{
    std::vector<int> bestChoices;
    bestChoices.reserve(choices.size());
    for (size_t i = 0; i < choices.size(); ++i) {
        std::map<probName, uint8_t> posibleActions = getAllPosibleActions(choices[i].actions, myUnitsList->at(choices[i].pos));
        // select an action with probabilities learned
        std::vector<double> probability(choiceSizes[i]);
        for (int j = 0; j < choiceSizes[i]; ++j) {
            probability[j] = getActionProbability(choices[i].actions[j], posibleActions, myUnitsList->at(choices[i].pos));
        }

        // get max probable choice
        double maxProb = 0.0;
        int maxChoiceIndex = -1;
        for (size_t j = 0; j < choiceSizes[i]; ++j) {
            if (probability[j] > maxProb) {
                maxProb = probability[j];
                maxChoiceIndex = j;
            }
        }
        bestChoices.emplace_back(maxChoiceIndex);
    }
    return bestChoices;
}

// © Alberto Uriarte
bool isBase(uint8_t typeID)
{
    return (typeID == BWAPI::UnitTypes::Terran_Command_Center
         || typeID == BWAPI::UnitTypes::Protoss_Nexus
         || typeID == BWAPI::UnitTypes::Zerg_Hatchery
         || typeID == BWAPI::UnitTypes::Zerg_Hive
         || typeID == BWAPI::UnitTypes::Zerg_Lair);
}

// © me & Alberto Uriarte
bool ActionGenerator::isMovingTowardsBase(const std::vector<unitGroup_t*>* groupList, uint8_t currentRegion, uint8_t targetRegion)
{
    int minDistToBaseActualReg = std::numeric_limits<int>::max();
    int minDistToBaseTargetReg = std::numeric_limits<int>::max();
    for (const auto& g : *groupList) {
        if (isBase(g->unitTypeId)) {
            int distFromActualReg = gameState->regman->distanceBetweenRegions[currentRegion][g->regionId];
            int distFromTargetReg = gameState->regman->distanceBetweenRegions[targetRegion][g->regionId];
            minDistToBaseActualReg = std::min(minDistToBaseActualReg, distFromActualReg);
            minDistToBaseTargetReg = std::min(minDistToBaseTargetReg, distFromTargetReg);
        }
    }
    return (minDistToBaseTargetReg < minDistToBaseActualReg);
}

// © me & Alberto Uriarte
std::string ActionGenerator::getMoveFeatures(uint8_t regionID, unitGroup_t* currentGroup)
{
    std::bitset<4> features;
    // toFriend
    for (const auto& friendGroup : *myUnitsList) {
        if (currentGroup == friendGroup) { continue; } // ignore ourself
        if (friendGroup->regionId == regionID) {
            features.set(3);
            break;
        }
    }
    // toEnemy
    for (const auto& enemyGroup : *enemyUnitsList) {
        if (enemyGroup->regionId == regionID) {
            features.set(2);
            break;
        }
    }
    // towardsFriend
    if (isMovingTowardsBase(myUnitsList, currentGroup->regionId, regionID)) {
        features.set(1);
    }
    // towardsEnemy
    if (isMovingTowardsBase(enemyUnitsList, currentGroup->regionId, regionID)) {
        features.set(0);
    }
    return features.to_string();
}

// © me & Alberto Uriarte
std::map<probName, uint8_t> ActionGenerator::getAllPosibleActions(const std::vector<action_t>& actions, unitGroup_t* currentGroup)
{
    std::map<probName, uint8_t> actionsFeatures;
    for (const auto& action : actions) {
        if (action.orderID == abstractOrder::order::Attack) {
            actionsFeatures[probName::Attack] += 1;
        } else if (action.orderID == abstractOrder::order::Idle) {
            actionsFeatures[probName::Idle] += 1;
        } else { // move
            std::string features = getMoveFeatures(action.targetRegion, currentGroup);
            actionsFeatures[stringToProbName["Move" + features]] += 1;
        }
    }
    return actionsFeatures;
}

// © me & Alberto Uriarte
double ActionGenerator::getProbabilityGivenState(probName action, const std::map<probName, uint8_t>& possibleActions)
{
    double prob = priorProb[action];
    uint8_t numActions = 1;
    std::vector<probName> notLegalActions = {
        Attack,
        Move0000, Move0001, Move0010, Move0011,
        Move0100, Move0101, Move0110, Move0111,
        Move1000, Move1001, Move1010, Move1011,
        Move1100, Move1101, Move1110, Move1111 };

    for (const auto& possibility : possibleActions) {
        if (action == possibility.first) { numActions = possibility.second; }
        prob *= likelihoodProb[action][possibility.first];
        // remove action from notLegalActions
        notLegalActions.erase(std::remove(notLegalActions.begin(), notLegalActions.end(), possibility.first), notLegalActions.end());
    }

    for (const auto& notLegalAction : notLegalActions) {
        prob *= 1.0 - likelihoodProb[action][notLegalAction];
    }
    return prob / numActions;
}

// © Alberto Uriarte
double ActionGenerator::getActionProbability(action_t action, const std::map<probName, uint8_t>& possibleActions, unitGroup_t* currentGroup)
{
    if (action.orderID == abstractOrder::order::Attack) {
        return getProbabilityGivenState(probName::Attack, possibleActions);
    } else if (action.orderID == abstractOrder::order::Idle) {
        return getProbabilityGivenState(probName::Idle, possibleActions);
    } else { // move
        std::string features = getMoveFeatures(action.targetRegion, currentGroup);
        return getProbabilityGivenState(stringToProbName["Move" + features], possibleActions);
    }
}

// © me & Alberto Uriarte
playerActions_t ActionGenerator::getUniqueRandomAction()
{
    if (choicesGenerated.size() == actionsSize) { return lastActions; }

    std::vector<int> randomChoices(getRandomChoices());
    size_t hashChoice(getHashFromChoicesSelected(randomChoices));

    // check if the action has been generated
    bool unique = choicesGenerated.find(hashChoice) == choicesGenerated.end();

    while (!unique) {
        // increment the choice until find a unique one
        incrementChoice(randomChoices);
        hashChoice = getHashFromChoicesSelected(randomChoices);
        unique = choicesGenerated.find(hashChoice) == choicesGenerated.end();
    }
    lastActions = getActionsFromChoices(randomChoices);
    choicesGenerated.insert(hashChoice);
    return lastActions;
}

// © Alberto Uriarte
// Warning, it assumes that a choice cannot be bigger than 9
size_t ActionGenerator::getHashFromChoicesSelected(const std::vector<int>& choiceSelected)
{
    size_t hashChoice = 0;
    hashChoice += choiceSelected[0];
    for (size_t i = 1; i < choices.size(); ++i) {
        hashChoice *= 10;
        hashChoice += choiceSelected[i];
    }
    return hashChoice;
}

// © me & Alberto Uriarte
std::string ActionGenerator::toString(const playerActions_t& playerActions)
{
    std::string tmp = "";
    for (const auto& playerAction : playerActions) {
#ifdef DEBUG_ORDERS
//         tmp += "Vector pos: " + intToString(playerAction.pos) + " -> " + BWAPI::UnitType(playerAction.unitTypeId).getName() + " at " + intToString(playerAction.regionId);
        tmp += BWAPI::UnitType(playerAction.unitTypeId).getName() + " at " + intToString(playerAction.regionId);
#endif
        tmp += " " + abstractOrder::name[playerAction.action.orderID] + " to " + intToString(playerAction.action.targetRegion) + "\n";
    }
    return tmp;
}

// © Alberto Uriarte
std::string ActionGenerator::toString()
{
    std::ostringstream tmp;
    tmp << "Possible Actions:\n";
    for (size_t i = 0; i < choices.size(); i++) {
#ifdef DEBUG_ORDERS
        tmp << BWAPI::UnitType(choices[i].unitTypeId).getName() << " (" << intToString(choices[i].regionId) << ")\n";
#endif
        for (const auto& action : choices[i].actions) {
            tmp << "  - " << gameState->getAbstractOrderName(action.orderID) << " (" << intToString(action.targetRegion) << ")\n";
        }
    }
    tmp << "Number of children: " << actionsSize << "\n";
    return tmp.str();
}

// © Alberto Uriarte
int ActionGenerator::getHighLevelFriendlyActions()
{
    return getHighLevelActions(gameState->army.friendly);
}

// © Alberto Uriarte
int ActionGenerator::getHighLevelEnemyActions()
{
    return getHighLevelActions(gameState->army.enemy);
}

// © me & Alberto Uriarte
int ActionGenerator::getHighLevelActions(const unitGroupVector& unitList)
{
    int size = 1;
    for (const auto& unit : unitList) {
        size *= getUnitActions(unit).size();
    }
    return size;
}

// © Alberto Uriarte
double ActionGenerator::getLowLevelFriendlyActions()
{
    return getLowLevelActions(Broodwar->self()->getUnits());
}

// © Alberto Uriarte
double ActionGenerator::getLowLevelEnemyActions()
{
    return getLowLevelActions(Broodwar->enemy()->getUnits());
}

// © me & Alberto Uriarte
double ActionGenerator::getLowLevelActions(const BWAPI::Unitset& units)
{
    int size = 1;
    for (const auto &unit : units) {
        int unitActions = 1; // do nothing
        const auto& unitType = unit->getType();

        if (unitType.isBuilding()) { // building case
            // train order
            if (unitType.canProduce() && !unit->isTraining()) { unitActions++; }
            // cancel order
            if (unit->isTraining()) { unitActions++; }

        } else if (unitType.isWorker()) { // worker case
            // move
            unitActions += 8; // move directions
            unitActions++; // cancel order
            unitActions++; // halt

            BWAPI::Unitset unitsInSeekRange = Broodwar->getUnitsInRadius(unit->getPosition(), unitType.seekRange());
            for (const auto &unit2 : unitsInSeekRange) {
                if (unit->isInWeaponRange(unit2)) unitActions++; // attack on units on range
                if (unit2->getType().isResourceContainer()) unitActions++; // harvest
                if (unitType == BWAPI::UnitTypes::Terran_SCV && unit2->getType().isMechanical()) {
                    unitActions++; // repair
                }
            }

            unitActions += 5; // considering only the 5 basic Terran buildings
            unitActions++; // cancel order
        }
        else { // combat unit case
            BWAPI::Unitset unitsInSeekRange = Broodwar->getUnitsInRadius(unit->getPosition(), unitType.seekRange());
            if (unitType.canMove()) {
                unitActions += 8; // move directions
                unitActions++; // cancel order
                unitActions++; // halt
                unitActions += 8; // patrol
            }
            // abilities on units on range
            if (!unitType.abilities().empty()) {
                for (const auto &ability : unitType.abilities()) {
                    // some abilities affect target units
                    if (ability.targetsUnit() || ability.targetsPosition()) {
                        unitActions += (unitType.abilities().size() * unitsInSeekRange.size());
                    } else { // other abilities affect only yourself
                        unitActions++;
                    }
                }
            }

            if (unitType.canAttack()) {
                // attack on units on range
                for (const auto &unit2 : unitsInSeekRange) {
                    if (unit->isInWeaponRange(unit2)) unitActions++;
                }
            }
        }

        // update total actions
        size *= unitActions;
    }
    return size;
}

// © Alberto Uriarte
int ActionGenerator::getSparcraftFriendlyActions()
{
    return getSparcraftActions(Broodwar->self()->getUnits());
}

// © Alberto Uriarte
int ActionGenerator::getSparcraftEnemyActions()
{
    return getSparcraftActions(Broodwar->enemy()->getUnits());
}

// © me & Alberto Uriarte
int ActionGenerator::getSparcraftActions(const BWAPI::Unitset& units)
{
    int size = 1;
    for (const auto &unit : units) {
        int unitActions = 1; // do nothing

        // skip non combat units
        if (unit->getType().canAttack()) {
            BWAPI::Unitset unitsInSeekRange = Broodwar->getUnitsInRadius(unit->getPosition(), unit->getType().seekRange());
            // attack on units on range
            for (const auto &unit2 : unitsInSeekRange) {
                if (unit->isInWeaponRange(unit2)) { unitActions++; }
            }
        }

        if (unit->getType().canMove()) { unitActions += 4; } // move directions

        if (unit->getType() == UnitTypes::Terran_Medic) {
            BWAPI::Unitset unitsInSeekRange = Broodwar->getUnitsInRadius(unit->getPosition(), unit->getType().seekRange());
            // attack on units on range
            for (const auto &unit2 : unitsInSeekRange) {
                if (unit2->getType().isOrganic()) unitActions++;
            }
        }
        size *= unitActions;
    }
    return size;
}

// © me & Alberto Uriarte
playerActions_t ActionGenerator::getNextActionProbability(double& prob)
{
    if (moreActions) {

        for (size_t i = 0; i < choices.size(); ++i) {
            // get probability of all possible actions
            std::map<probName, uint8_t> posibleActions = getAllPosibleActions(choices[i].actions, myUnitsList->at(choices[i].pos));

            std::vector<double> probability(choiceSizes[i]);
            for (int j = 0; j < choiceSizes[i]; ++j) {
                probability[j] = getActionProbability(choices[i].actions[j], posibleActions, myUnitsList->at(choices[i].pos));
            }

            // compute probability of selected choice
            double totalProb = std::accumulate(probability.begin(), probability.end(), 0.0);
            prob = probability.at(currentChoice.at(i)) / totalProb;
        }

        lastActions = getActionsFromChoices(currentChoice);
        incrementChoice(currentChoice);
    }
    return lastActions;
}
