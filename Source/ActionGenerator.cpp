#include "stdafx.h"

#include <BWTA.h>

#include "ActionGenerator.h"
#include "AbstractOrder.h"
#include "ActionProbabilities.h"

using namespace BWAPI;

ActionGenerator::ActionGenerator()
    :_moreActions(false)
{}

ActionGenerator::ActionGenerator(const GameState* gs, bool player)
    : _gs(gs),
    _size(1),
    _player(player)
{
    // set myUnitsList and enemyUnitList depends on the player
    if (_player) {
        _myUnitsList = &_gs->_army.friendly;
        _enemyUnitsList = &_gs->_army.enemy;
    } else {
        _myUnitsList = &_gs->_army.enemy;
        _enemyUnitsList = &_gs->_army.friendly;
    }
    
    //cleanActions(); // TODO temporal for debuging

    // Generate all possible choices
    _choices.clear();
    for (size_t i = 0; i < _myUnitsList->size(); ++i) {
        if ((*_myUnitsList)[i]->endFrame <= _gs->_time) {
            unitGroup_t* unit = (*_myUnitsList)[i];
            std::vector<action_t> &actions = getUnitActions(unit);
#ifdef DEBUG_ORDERS
            _choices.push_back(choice_t(i, actions, unit->unitTypeId, unit->regionId, player));
#else
            _choices.push_back(choice_t(i, actions));
#endif
            _size *= actions.size();
            if (_size < 0) DEBUG("Number of actions overflow!!!");
        }
    }

    if (!_choices.empty()) {
        _moreActions = true;
        _choiceSizes.resize(_choices.size());
        for (size_t i = 0; i < _choices.size(); ++i) {
            _choiceSizes[i] = _choices[i].actions.size();
        }
        _currentChoice.resize(_choices.size(), 0);
    } else {
        _moreActions = false;
        _size = 0;
    }
}

// Warning!! This will clean the actions of the original game state!
void ActionGenerator::cleanActions()
{
    for (auto& unitGroup : *_myUnitsList) {
        unitGroup->endFrame = -1;
        unitGroup->orderId = abstractOrder::Unknown;
    }
}

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
        auto region = _gs->_regionFromID.find(unit->regionId);
        if (region != _gs->_regionFromID.end()) {
            if (!_gs->_onlyRegions) {
                for (const auto& c : region->second->getChokepoints()) {
                    possibleActions.push_back(action_t(abstractOrder::Move, _gs->_chokePointID.at(c)));
                }
            } else {
                std::vector<int> duplicates; // sorted vector to lookup duplicate regionID
                std::vector<int>::iterator i;
                int regionIDtoAdd;
                for (const auto& c : region->second->getChokepoints()) {
                    const auto regions = c->getRegions();
                    regionIDtoAdd = _gs->_regionID.at(regions.first);
                    if (_gs->_regionID.at(regions.first) == unit->regionId) {
                        regionIDtoAdd = _gs->_regionID.at(regions.second);
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
            auto cp = _gs->_chokePointFromID.find(unit->regionId);
            if (cp != _gs->_chokePointFromID.end()) {
                const auto regions = cp->second->getRegions();
                possibleActions.push_back(action_t(abstractOrder::Move, _gs->_regionID.at(regions.first)));
                possibleActions.push_back(action_t(abstractOrder::Move, _gs->_regionID.at(regions.second)));
            }
        }
    }

    // Attack
    // --------------------------------
    // only if there are enemies in the region that we can attack
    // TODO we cannot simulate spellCasters!!!
    if (unitType.canAttack()) {
        // if there are enemies in the same region, ATTACK is the only option
        for (const auto& enemyGroup : *_enemyUnitsList) {
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

void ActionGenerator::incrementChoice(std::vector<int>& currentChoice, size_t startPosition) {
    for (size_t i = 0; i<startPosition; ++i) currentChoice[i] = 0;
    currentChoice[startPosition]++;
    if (currentChoice[startPosition] >= _choiceSizes[startPosition]) {
        if (startPosition < currentChoice.size()-1) {
            incrementChoice(currentChoice, startPosition + 1);
        } else {
            _moreActions = false;
            for (size_t i = 0; i<currentChoice.size(); ++i) currentChoice[i] = 0;
        }
    }

//     std::ostringstream oss;
//     std::copy(currentChoice.begin(), currentChoice.end() - 1, std::ostream_iterator<int>(oss, ","));
//     oss << currentChoice.back();
//     LOG("current choice = [" << oss.str() << "]");
}

playerActions_t ActionGenerator::getNextAction() {
    if (_moreActions) {
        _lastAction = getActionsFromChoices(_currentChoice);
        incrementChoice(_currentChoice);
        return _lastAction;
    }
    //return _lastAction;
    return playerActions_t();
}

playerActions_t ActionGenerator::getActionsFromChoices(std::vector<int> choiceSelected)
{
    playerActions_t playerActions;
    playerActions.reserve(_choices.size());
    for (size_t i = 0; i < _choices.size(); ++i) {
        playerActions.emplace_back(playerAction_t(_choices[i], choiceSelected[i]));
    }
    return playerActions;
}

playerActions_t ActionGenerator::getRandomAction() {
    std::vector<int> randomChoices(getRandomChoices());
    _lastAction = getActionsFromChoices(randomChoices);
    return _lastAction;
}

std::vector<int> ActionGenerator::getRandomChoices()
{
    std::vector<int> randomChoices;
    randomChoices.reserve(_choices.size());
    for (size_t i = 0; i < _choices.size(); ++i) {
        std::uniform_int_distribution<int> uniformDist(0, _choiceSizes[i] - 1);
        randomChoices.emplace_back(uniformDist(gen));
    }
    return randomChoices;
}

playerActions_t ActionGenerator::getBiasAction() {
    std::vector<int> randomChoices(getBiasChoices());
    _lastAction = getActionsFromChoices(randomChoices);
    return _lastAction;
}

std::vector<int> ActionGenerator::getBiasChoices()
{
    std::vector<int> randomChoices;
    randomChoices.reserve(_choices.size());
    for (size_t i = 0; i < _choices.size(); ++i) {
        
        std::map<probName, uint8_t> posibleActions = getAllPosibleActions(_choices[i].actions, _myUnitsList->at(_choices[i].pos));
        // select an action with probabilities learned
        std::vector<double> probability(_choiceSizes[i]);
        for (int j = 0; j < _choiceSizes[i]; ++j) {
            probability[j] = getActionProbability(_choices[i].actions[j], posibleActions, _myUnitsList->at(_choices[i].pos));
        }

        // get choice from normalized probabilities
        double totalProb = std::accumulate(probability.begin(), probability.end(), 0.0);
//         LOG("total Prob: " << totalProb);
//         std::stringstream outputTest;
//         for (int j = 0; j < _choiceSizes[i]; ++j) {
//             outputTest << probability[j] / totalProb << ",";
//         }
//         LOG("Prob: [" << outputTest.str() << "]");

        std::uniform_real_distribution<double> uniformDist(0, 1);
        double randomProb = uniformDist(gen);
        double accumProb = 0.0;
        for (int j = 0; j < _choiceSizes[i]; ++j) {
            accumProb += probability[j];
            if (randomProb <= accumProb / totalProb) {
                randomChoices.emplace_back(j);
//                 LOG("Random: " << randomProb << " choice: " << j);
                break;
            }
        }
    }
    return randomChoices;
}

playerActions_t ActionGenerator::getMostProbAction() {
    std::vector<int> bestChoices(getBestChoices());
    _lastAction = getActionsFromChoices(bestChoices);
    return _lastAction;
}

std::vector<int> ActionGenerator::getBestChoices()
{
    std::vector<int> bestChoices;
    bestChoices.reserve(_choices.size());
    for (size_t i = 0; i < _choices.size(); ++i) {

        std::map<probName, uint8_t> posibleActions = getAllPosibleActions(_choices[i].actions, _myUnitsList->at(_choices[i].pos));
        // select an action with probabilities learned
        std::vector<double> probability(_choiceSizes[i]);
        for (int j = 0; j < _choiceSizes[i]; ++j) {
            probability[j] = getActionProbability(_choices[i].actions[j], posibleActions, _myUnitsList->at(_choices[i].pos));
        }

        // get max probable choice
        double maxProb = 0.0;
        int maxChoiceIndex = -1;
        for (int j = 0; j < _choiceSizes[i]; ++j) {
            if (probability[j] > maxProb) {
                maxProb = probability[j];
                maxChoiceIndex = j;
            }
        }
//         LOG("Best choice: " << maxProb << " choice: " << maxChoiceIndex);
        bestChoices.emplace_back(maxChoiceIndex);

    }
    return bestChoices;
}

bool isBase(uint8_t typeID)
{
    return (typeID == BWAPI::UnitTypes::Terran_Command_Center
        || typeID == BWAPI::UnitTypes::Protoss_Nexus
        || typeID == BWAPI::UnitTypes::Zerg_Hatchery
        || typeID == BWAPI::UnitTypes::Zerg_Hive
        || typeID == BWAPI::UnitTypes::Zerg_Lair);
}

bool ActionGenerator::isMovingTowardsBase(const std::vector<unitGroup_t*>* groupList, uint8_t currentRegion, uint8_t targetRegion)
{
    int minDistToBaseActualReg = std::numeric_limits<int>::max();
    int minDistToBaseTargetReg = std::numeric_limits<int>::max();
    for (const auto& g : *groupList) {
        if (isBase(g->unitTypeId)) {
            int distFromActualReg = _gs->_distanceBetweenRegions[currentRegion][g->regionId];
            int distFromTargetReg = _gs->_distanceBetweenRegions[targetRegion][g->regionId];
            minDistToBaseActualReg = std::min(minDistToBaseActualReg, distFromActualReg);
            minDistToBaseTargetReg = std::min(minDistToBaseTargetReg, distFromTargetReg);
        }
    }
//     LOG("Current region (" << (int)currentRegion << ") to base = " << minDistToBaseActualReg << " target region (" << (int)targetRegion << ") to base = " << minDistToBaseTargetReg);
    return (minDistToBaseTargetReg < minDistToBaseActualReg);
}

std::string ActionGenerator::getMoveFeatures(uint8_t regionID, unitGroup_t* currentGroup)
{
    std::bitset<4> features;
    // toFriend
    for (const auto& friendGroup : *_myUnitsList) {
        if (currentGroup == friendGroup) continue; // ignore ourself
        if (friendGroup->regionId == regionID) {
            features.set(3);
            break;
        }
    }
    // toEnemy
    for (const auto& enemyGroup : *_enemyUnitsList) {
        if (enemyGroup->regionId == regionID) {
            features.set(2);
            break;
        }
    }
    // towardsFriend
    if (isMovingTowardsBase(_myUnitsList, currentGroup->regionId, regionID)) {
        features.set(1);
    }
    // towardsEnemy
    if (isMovingTowardsBase(_enemyUnitsList, currentGroup->regionId, regionID)) {
        features.set(0);
    }
    return features.to_string();
}

std::map<probName, uint8_t> ActionGenerator::getAllPosibleActions(std::vector<action_t> actions, unitGroup_t* currentGroup)
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

double ActionGenerator::getProbabilityGivenState(probName action, std::map<probName, uint8_t> possibleActions)
{
    double prob = priorProb[action];
    uint8_t numActions = 1;

//     std::stringstream text;
//     text << "Prob(" << porbNameToString[action] << "|";
//     for (const auto& possibility : possibleActions) text << porbNameToString[possibility.first] << ',';
//     text << ") = " << priorProb[action] << "*";

    std::vector<probName> notLegalActions = { Attack,
        Move0000, Move0001, Move0010, Move0011,
        Move0100, Move0101, Move0110, Move0111,
        Move1000, Move1001, Move1010, Move1011,
        Move1100, Move1101, Move1110, Move1111 };

    for (const auto& possibility : possibleActions) {
        if (action == possibility.first) numActions = possibility.second;
//         text << likelihoodProb[action][possibility.first] << "*";
        prob *= likelihoodProb[action][possibility.first];
        // remove action from notLegalActions
        notLegalActions.erase(std::remove(notLegalActions.begin(), notLegalActions.end(), possibility.first), notLegalActions.end());
    }

    for (const auto& notLegalAction : notLegalActions) {
//         text << notLegalAction << "=>" << 1.0 - likelihoodProb[action][notLegalAction] << "*";
        prob *= 1.0 - likelihoodProb[action][notLegalAction];
    }

//     text << " / " << (int)numActions << " = " << prob / numActions;
//     LOG(text.str());
    return prob / numActions;
}

double ActionGenerator::getActionProbability(action_t action, std::map<probName, uint8_t> possibleActions, unitGroup_t* currentGroup)
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

playerActions_t ActionGenerator::getUniqueRandomAction() {
//     LOG(_choicesGenerated.size() << " of " << _size << " random action generated");
    if (_choicesGenerated.size() == _size) return _lastAction;

    std::vector<int> randomChoices(getRandomChoices());
    size_t hashChoice(getHashFromChoicesSelected(randomChoices));

    // check if the action has been generated
    bool unique = _choicesGenerated.find(hashChoice) == _choicesGenerated.end();

    while (!unique) {
        // increment the choice until find a unique one
        incrementChoice(randomChoices);
        hashChoice = getHashFromChoicesSelected(randomChoices);
        unique = _choicesGenerated.find(hashChoice) == _choicesGenerated.end();
    }
    _lastAction = getActionsFromChoices(randomChoices);
    _choicesGenerated.insert(hashChoice);
//     LOG("Added new random action");
    return _lastAction;
}

// Warning, it assumes that a choice cannot be bigger than 9
size_t ActionGenerator::getHashFromChoicesSelected(std::vector<int> choiceSelected)
{
    size_t hashChoice = 0;
    hashChoice += choiceSelected[0];
    for (size_t i = 1; i < _choices.size(); ++i) {
        hashChoice *= 10;
        hashChoice += choiceSelected[i];
    }

//     std::ostringstream oss;
//     std::copy(choiceSelected.begin(), choiceSelected.end() - 1, std::ostream_iterator<int>(oss, ","));
//     oss << choiceSelected.back();
//     LOG("current choice = [" << oss.str() << "] hash=" << hashChoice);

    return hashChoice;
}

std::string ActionGenerator::toString(playerActions_t playerActions)
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

std::string ActionGenerator::toString()
{
    std::ostringstream tmp;
    tmp << "Possible Actions:\n";
    for (unsigned int i=0; i < _choices.size(); i++) {
#ifdef DEBUG_ORDERS
        tmp << BWAPI::UnitType(_choices[i].unitTypeId).getName() << " (" << intToString(_choices[i].regionId) << ")\n";
#endif
        for (const auto& action : _choices[i].actions) {
            tmp << "  - " << _gs->getAbstractOrderName(action.orderID) << " (" << intToString(action.targetRegion) << ")\n";
        }
    }
    tmp << "Number of children: " << _size << "\n";

    return tmp.str();
}

double ActionGenerator::getHighLevelFriendlyActions()
{
    return getHighLevelActions(_gs->_army.friendly);
}

double ActionGenerator::getHighLevelEnemyActions()
{
    return getHighLevelActions(_gs->_army.enemy);
}

double ActionGenerator::getHighLevelActions(unitGroupVector unitList)
{
    double size = 1;
    for (const auto& unit : unitList) {
        size *= getUnitActions(unit).size();
    }
    return size;
}

double ActionGenerator::getLowLevelFriendlyActions()
{
    return getLowLevelActions(Broodwar->self()->getUnits());
}

double ActionGenerator::getLowLevelEnemyActions()
{
    return getLowLevelActions(Broodwar->enemy()->getUnits());
}

double ActionGenerator::getLowLevelActions(BWAPI::Unitset units)
{
    double size = 1;
    for (const auto &unit : units) {
        int unitActions = 1; // do nothing
        
        // building case
        if (unit->getType().isBuilding()) {
            // train order
            if (unit->getType().canProduce() && !unit->isTraining()) unitActions++;
            // cancel order
            if (unit->isTraining()) unitActions++; 

        // worker case
        } else if (unit->getType().isWorker()) {
            // move
            unitActions += 8; // move directions
            unitActions++; // cancel order
            unitActions++; // halt

            BWAPI::Unitset unitsInSeekRange = Broodwar->getUnitsInRadius(unit->getPosition(), unit->getType().seekRange());
            for (const auto &unit2 : unitsInSeekRange) {
                if (unit->isInWeaponRange(unit2)) unitActions++; // attack on units on range
                if (unit2->getType().isResourceContainer()) unitActions++; // harvest
                if (unit->getType() == BWAPI::UnitTypes::Terran_SCV && unit2->getType().isMechanical()) {
                    unitActions++; // repair
                }
            }

            unitActions += 5; // considering only the 5 basic Terran buildings
            unitActions++; // cancel order

        // combat unit case
        } else {
            BWAPI::Unitset unitsInSeekRange = Broodwar->getUnitsInRadius(unit->getPosition(), unit->getType().seekRange());
            if (unit->getType().canMove()) {
                unitActions += 8; // move directions
                unitActions++; // cancel order
                unitActions++; // halt
                unitActions += 8; // patrol
            }
            // abilities on units on range
            if (!unit->getType().abilities().empty()) {
                for (const auto &ability : unit->getType().abilities()) {
                    // some abilities affect target units
                    if (ability.targetsUnit() || ability.targetsPosition()) {
                        unitActions += (unit->getType().abilities().size()*unitsInSeekRange.size());
                    } else { // other abilities affect only yourself
                        unitActions++;
                    }
                }
            }

            if (unit->getType().canAttack()) {
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

double ActionGenerator::getSparcraftFriendlyActions()
{
    return getSparcraftActions(Broodwar->self()->getUnits());
}
double ActionGenerator::getSparcraftEnemyActions()
{
    return getSparcraftActions(Broodwar->enemy()->getUnits());
}

// TODO int return type
double ActionGenerator::getSparcraftActions(BWAPI::Unitset units)
{
    double size = 1;
    for (const auto &unit : units) {
        int unitActions = 1; // do nothing

        // skip non combat units
        if (unit->getType().canAttack()) {
            BWAPI::Unitset unitsInSeekRange = Broodwar->getUnitsInRadius(unit->getPosition(), unit->getType().seekRange());
            // attack on units on range
            for (const auto &unit2 : unitsInSeekRange) {
                if (unit->isInWeaponRange(unit2)) unitActions++;
            }
        }

        if (unit->getType().canMove()) {
            unitActions += 4; // move directions
        }

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

playerActions_t ActionGenerator::getNextActionProbability(double& prob)
{
    if (_moreActions) {

        for (size_t i = 0; i < _choices.size(); ++i) {
            // get probability of all possible actions
            std::map<probName, uint8_t> posibleActions = getAllPosibleActions(_choices[i].actions, _myUnitsList->at(_choices[i].pos));
            
            std::vector<double> probability(_choiceSizes[i]);
            for (int j = 0; j < _choiceSizes[i]; ++j) {
                probability[j] = getActionProbability(_choices[i].actions[j], posibleActions, _myUnitsList->at(_choices[i].pos));
            }
            
            // compute probability of selected choice
            double totalProb = std::accumulate(probability.begin(), probability.end(), 0.0);
            prob = probability.at(_currentChoice.at(i)) / totalProb;
        }

        _lastAction = getActionsFromChoices(_currentChoice);
        incrementChoice(_currentChoice);
    }
    return _lastAction;
}
