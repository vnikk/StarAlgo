#include "stdafx.h"
#include "MCTSCD.h"
#include "GameState.h"
#include "RegionManager.h"
#include "CombatSimSustained.h"
#include "CombatSimDecreased.h"

#include <BWTA.h>

#include <iomanip>
#include <utility>

// © Alberto Uriarte
const int IDLE_TIME = 400; //in frames
const int NOTHING_TIME = 24 * 60 * 30; //in frames (30 minutes)

using namespace BWAPI;

// © me & Alberto Uriarte
GameState::GameState(CombatSimulator* combatSim, RegionManager* regman)
: buildingTypes(NoneBuilding), time(0), cs(combatSim),
  friendlySiegeTankResearched(false), enemySiegeTankResearched(false), regman(regman)
{
    army.friendly.clear();
    army.enemy.clear();
}

GameState::~GameState()
{
    cleanArmyData();
}

// © me & Alberto Uriarte
void GameState::cleanArmyData()
{
    for (auto unitGroup : army.friendly) delete unitGroup;
    for (auto unitGroup : army.enemy)    delete unitGroup;
    army.friendly.clear();
    army.enemy.clear();
}

void GameState::changeCombatSimulator(CombatSimulator* newCS)
{
    cs = newCS;
}

GameState::GameState(const GameState &other)
: buildingTypes(other.buildingTypes), regionsInCombat(other.regionsInCombat), time(other.time),
  friendlySiegeTankResearched(other.friendlySiegeTankResearched), enemySiegeTankResearched(other.enemySiegeTankResearched),
  regman(other.regman), cs(other.cs)
{
    for (auto unitGroup : other.army.friendly) army.friendly.push_back(new unitGroup_t(*unitGroup));
    for (auto unitGroup : other.army.enemy   ) army.enemy   .push_back(new unitGroup_t(*unitGroup));
}

// © me & Alberto Uriarte
const GameState& GameState::operator=(const GameState& other)
{
    if (this == &other) return *this;
    // clean previous data
    cleanArmyData();
    // copy new data
    buildingTypes = other.buildingTypes;
    for (auto unitGroup : other.army.friendly) army.friendly.push_back(new unitGroup_t(*unitGroup));
    for (auto unitGroup : other.army.enemy) army.enemy.push_back(new unitGroup_t(*unitGroup));
    regionsInCombat = other.regionsInCombat;
    time = other.time;
    cs   = other.cs;
    friendlySiegeTankResearched = other.friendlySiegeTankResearched;
    enemySiegeTankResearched    = other.enemySiegeTankResearched;
    return *this;
}

// © me & Alberto Uriarte
int GameState::getMilitaryGroupsSize() const
{
    int size = 0;
    for (const auto& unitGroup : army.friendly) {
        BWAPI::UnitType unitType(unitGroup->unitTypeId);
        if (unitType.canAttack() || unitType.isSpellcaster()) { ++size; }
    }
    for (const auto& unitGroup : army.enemy) {
        BWAPI::UnitType unitType(unitGroup->unitTypeId);
        if (unitType.canAttack() || unitType.isSpellcaster()) { ++size; }
    }
    return size;
}

// © Alberto Uriarte
int GameState::getAllGroupsSize() const
{
    return army.friendly.size() + army.enemy.size();
}

// © Alberto Uriarte
int GameState::getFriendlyGroupsSize() const
{
    return army.friendly.size();
}

// © Alberto Uriarte
int GameState::getEnemyGroupsSize() const
{
    return army.enemy.size();
}

// © Alberto Uriarte
int GameState::getFriendlyUnitsSize() const
{
    int size = 0;
    for (const auto& unitGroup : army.friendly) {
        size += unitGroup->numUnits;
    }
    return size;
}

// © Alberto Uriarte
int GameState::getEnemyUnitsSize() const
{
    int size = 0;
    for (const auto& unitGroup : army.enemy) {
        size += unitGroup->numUnits;
    }
    return size;
}

// © Alberto Uriarte
void GameState::loadIniConfig()
{
    std::string iniBuilding = LoadConfigString("high_level_search", "buildings", "RESOURCE_DEPOT");
    if (iniBuilding == "NONE")     { buildingTypes = NoneBuilding; }
    else if (iniBuilding == "ALL") { buildingTypes = AllBuildings; }
    else                           { buildingTypes = ResourceDepot; }
}

/* this will be implemented later to enchance functionality
// © Alberto Uriarte
void GameState::importCurrentGameState()
{
    cleanArmyData();
    time = Broodwar->getFrameCount();
    unitGroupVector* armySide;

    for (auto player : Broodwar->getPlayers()) {
        if (player->isNeutral()) continue;
        else if (army.friendly.empty()) armySide = &army.friendly;
        else armySide = &army.enemy;

        for (auto unit : player->getUnits()) {
            // ignore buildings (except buildingTypes)
            if ((buildingTypes == ResourceDepot) && unit->getType().isBuilding() && !unit->getType().isResourceDepot() ) continue;
            // ignore units training
            if (!unit->isCompleted()) continue;
            if (unit->getType().isWorker()) continue;
            // ignore spells
            if (unit->getType().isSpell()) continue;

            uint8_t regionId = informationManager->regionIdMap[unit->getTilePosition().x][unit->getTilePosition().y];
            uint8_t orderId = abstractOrder::Unknown;
            if (unit->getType().isBuilding()) orderId = abstractOrder::Nothing;
            float unitHP = unit->getHitPoints() + unit->getShields();
            unitGroup_t* unitGroup = new unitGroup_t(unit->getType().getID(), 1, regionId, orderId, 0, time, unitHP);

            addUnitToArmySide(unitGroup, *armySide);
        }
    }

//     calculateExpectedEndFrameForAllGroups();
//     ordersSanityCheck();
}*/

// © me & Alberto Uriarte
// returns the unitGroup_t* where the unit was added
unsigned short GameState::addUnitToArmySide(unitGroup_t* unit, unitGroupVector& armySide)
{
    // simplify unitTypeId
    if (unit->unitTypeId == UnitTypes::Terran_Siege_Tank_Siege_Mode) {
        unit->unitTypeId = UnitTypes::Terran_Siege_Tank_Tank_Mode;
    }

    // check for wrong unit
    UnitType unitType(unit->unitTypeId);
    if (unitType.isSpell()) DEBUG("Trying to add a spell");

    // search if unit is already in the vector
    for (size_t i = 0; i < armySide.size(); ++i) {
        if (unit->unitTypeId == armySide[i]->unitTypeId &&
            unit->regionId == armySide[i]->regionId &&
            unit->orderId == armySide[i]->orderId &&
            unit->targetRegionId == armySide[i]->targetRegionId) {

            armySide[i]->numUnits++;
            // update avg hp
            armySide[i]->HP += (unit->HP - armySide[i]->HP) / armySide[i]->numUnits;
            delete unit;
            return i;
        }
    }
    // if not, add it to the end
    armySide.push_back(unit);
    return armySide.size() - 1;
}

// © me & Alberto Uriarte
// assumes army.enmey is empty
void GameState::addAllEnemyUnits()
{
    time = Broodwar->getFrameCount();
    for (auto unit : Broodwar->enemy()->getUnits()) {
        const auto& unitType = unit->getType();
        // ignore buildings (except buildingTypes)
        if ((buildingTypes == ResourceDepot) && unitType.isBuilding() && !unitType.isResourceDepot() ) continue;
        // ignore units training & worker & spells
        if (!unit->isCompleted() && unitType.isWorker() && unitType.isSpell()) { continue; }

        uint8_t regionId = regman->regionIdMap[unit->getTilePosition().x][unit->getTilePosition().y];
        uint8_t orderId = abstractOrder::Unknown;
        if (unitType.isBuilding()) orderId = abstractOrder::Nothing;
        float unitHP = unit->getHitPoints() + unit->getShields();
        unitGroup_t* unitGroup = new unitGroup_t(unitType.getID(), 1, regionId, orderId, 0, time, unitHP);

        addUnitToArmySide(unitGroup, army.enemy);
    }
}

// © me & Alberto Uriarte
void GameState::addSelfBuildings()
{
    for (auto unit : Broodwar->self()->getUnits()) {
        const auto& unitType = unit->getType();
        if ( !unitType.isBuilding() ) continue;
        if ( buildingTypes == ResourceDepot && !unitType.isResourceDepot() ) continue;

        uint8_t regionId = regman->regionIdMap[unit->getTilePosition().x][unit->getTilePosition().y];
        float unitHP = unit->getHitPoints() + unit->getShields();
        unitGroup_t* unitGroup = new unitGroup_t(unitType.getID(), 1, regionId, abstractOrder::Nothing, 0, time, unitHP);

        addUnitToArmySide(unitGroup, army.friendly);
    }
}

// © me & Alberto Uriarte
unsigned short GameState::addFriendlyUnit(Unit unit)
{
    uint8_t regionId = regman->regionIdMap[unit->getTilePosition().x][unit->getTilePosition().y];
    float unitHP = unit->getHitPoints() + unit->getShields();
    unitGroup_t* unitGroup = new unitGroup_t(unit->getType().getID(), 1, regionId, abstractOrder::Unknown, 0, time, unitHP);

    return addUnitToArmySide(unitGroup, army.friendly);
}

// © me & Alberto Uriarte
void GameState::addGroup(int unitId, int numUnits, int regionId, int listID, float groupHP, int orderId, int targetRegion, int endFrame)
{
    UnitType unitType(unitId);
    if (unitType.isAddon() || unitType.isSpell()) return;

    if (unitType.isBuilding()) orderId = abstractOrder::Nothing;
    unitGroup_t* unitGroup = new unitGroup_t(unitId, numUnits, regionId, orderId, targetRegion, endFrame, time, groupHP);

    if (listID == 1) {
        army.friendly.push_back(unitGroup);
    } else if (listID == 2) {
        army.enemy.push_back(unitGroup);
    }
}

// © me & Alberto Uriarte
void GameState::addUnit(int unitId, int regionId, int listID, int orderId, float unitHP)
{
    UnitType unitType(unitId);
    if (unitType.isAddon() || unitType.isSpell()) return;

    if (unitType.isBuilding()) orderId = abstractOrder::Nothing;
    unitGroup_t* unitGroup = new unitGroup_t(unitId, 1, regionId, orderId, 0, time, unitHP);

    if (listID == 1) {
        addUnitToArmySide(unitGroup, army.friendly);
    } else if (listID == 2) {
        addUnitToArmySide(unitGroup, army.enemy);
    }
}

// © me & Alberto Uriarte
// Calculate the expected end frame for each order
void GameState::calculateExpectedEndFrameForAllGroups()
{
    regionsInCombat.clear();
    unitGroupVector* armySide;

    for (int i = 0; i < 2; ++i) {
        if (i == 0) { armySide = &army.friendly; }
        else { armySide = &army.enemy; }

        for (auto& unitGroup : *armySide) {
            switch (unitGroup->orderId) {
                case abstractOrder::Move:
                    unitGroup->endFrame = time + getMoveTime(unitGroup->unitTypeId, unitGroup->regionId, unitGroup->targetRegionId);
                    break;
                case abstractOrder::Attack:
                    regionsInCombat.insert(unitGroup->regionId); // keep track of the regions with units in attack state
                    break;
                case abstractOrder::Nothing: // it's a building
                    unitGroup->endFrame = time + NOTHING_TIME;
                    break;
                case abstractOrder::Unknown:
                default:
                    unitGroup->endFrame = time + IDLE_TIME;
                    break;
            }
        }
    }
#ifdef _DEBUG
    sanityCheck();
#endif
    for (const auto& regionId : regionsInCombat) { calculateCombatLengthAtRegion(regionId); }
}

// © me & Alberto Uriarte
int GameState::getMoveTime(int unitTypeId, int regionId, int targetRegionId)
{
    if (regionId == targetRegionId) { return 0; }
    // auto-unsiege tanks
    if (unitTypeId == UnitTypes::Terran_Siege_Tank_Siege_Mode) {
        unitTypeId = UnitTypes::Terran_Siege_Tank_Tank_Mode;
    }
    UnitType unitType(unitTypeId);
    double speed = unitType.topSpeed(); //non-upgraded top speed in pixels per frame
    int distance = getRegionDistance(regionId, targetRegionId); // distance in pixels

    int timeToMove = distance / speed;
    if (timeToMove == INT_MAX || timeToMove < 0) {
        DEBUG("Wrong move time: " << timeToMove << " traveling from " << regionId << " to " << targetRegionId);
        DEBUG(toString());
    }
    return timeToMove;
}

int GameState::getRegionDistance(int regId1, int regId2)
{
    return regman->distanceBetweenRegions[regId1][regId2];
}

// © Alberto Uriarte
int GameState::getAbstractOrderID(int orderId, int currentRegion, int targetRegion)
{
    if (orderId == BWAPI::Orders::MoveToMinerals ||
        orderId == BWAPI::Orders::WaitForMinerals ||
        orderId == BWAPI::Orders::MiningMinerals ||
        orderId == BWAPI::Orders::Harvest3 ||
        orderId == BWAPI::Orders::Harvest4 ||
        orderId == BWAPI::Orders::ReturnMinerals)
        return abstractOrder::Unknown;
    else if (orderId == BWAPI::Orders::MoveToGas ||
             orderId == BWAPI::Orders::Harvest1 ||
             orderId == BWAPI::Orders::Harvest2 ||
             orderId == BWAPI::Orders::WaitForGas ||
             orderId == BWAPI::Orders::HarvestGas ||
             orderId == BWAPI::Orders::ReturnGas )
        return abstractOrder::Unknown;
    else if (orderId == BWAPI::Orders::Move ||
             orderId == BWAPI::Orders::Follow ||
             orderId == BWAPI::Orders::ComputerReturn )
         return abstractOrder::Move;
    else if (orderId == BWAPI::Orders::AttackUnit ||
             orderId == BWAPI::Orders::AttackMove ||
             orderId == BWAPI::Orders::AttackTile)
        if (currentRegion == targetRegion) return abstractOrder::Attack;
        else return abstractOrder::Move;
    else if (orderId == BWAPI::Orders::Repair ||
             orderId == BWAPI::Orders::MedicHeal )
        return abstractOrder::Heal;
    else if (orderId == BWAPI::Orders::Nothing)
        return abstractOrder::Nothing;
    else {
        return abstractOrder::Unknown;
    }
}

// © Alberto Uriarte
std::string GameState::getAbstractOrderName(BWAPI::Order order, int currentRegion, int targetRegion)
{
    int abstractId = getAbstractOrderID(order.getID(), currentRegion, targetRegion);
    return abstractOrder::name[abstractId];
}

// © Alberto Uriarte
std::string GameState::getAbstractOrderName(int abstractId) const
{
    return abstractOrder::name[abstractId];
}

// © me & Alberto Uriarte
std::string GameState::toString() const
{
    std::stringstream tmp;
    tmp << "GameState: time " << time << " ====================================\n";
    tmp << "Type Num Reg Order toReg EndFrame HP Unit name" << std::endl;
    for (const auto& group : army.friendly) {
        tmp << std::setw(5) << intToString(group->unitTypeId)
            << std::setw(4) << intToString(group->numUnits)
            << std::setw(4) << intToString(group->regionId)
            << std::setw(8) << getAbstractOrderName(group->orderId)
            << std::setw(6) << intToString(group->targetRegionId)
            << std::setw(9) << intToString(group->endFrame - time)
            << std::setw(5) << intToString(group->HP)
            << " " << BWAPI::UnitType(group->unitTypeId).getName() << std::endl;
    }
    tmp << "(up)FRIENDS---------------ENEMIES(down)\n";
    for (const auto& group : army.enemy) {
        tmp << std::setw(5) << intToString(group->unitTypeId)
            << std::setw(4) << intToString(group->numUnits)
            << std::setw(4) << intToString(group->regionId)
            << std::setw(8) << getAbstractOrderName(group->orderId)
            << std::setw(6) << intToString(group->targetRegionId)
            << std::setw(9) << intToString(group->endFrame - time)
            << std::setw(5) << intToString(group->HP)
            << " " << BWAPI::UnitType(group->unitTypeId).getName() << std::endl;
    }
    return tmp.str();
}

// © me & Alberto Uriarte
// checks ending frame of the current game state, supervise the regions in combat
void GameState::execute(const playerActions_t &playerActions, bool player)
{
    ordersSanityCheck();
    unitGroupVector* armySide;
    if (player) { armySide = &army.friendly; }
    else        { armySide = &army.enemy; }

    uint8_t orderId;
    uint8_t targetRegionId;
    unitGroup_t* unitGroup;
    int timeToMove;
    std::set<int> computeCombatLengthIn; // set because we don't want duplicates

    for (size_t i = 0; i < playerActions.size(); ++i) {
        orderId = playerActions[i].action.orderID;
        targetRegionId = playerActions[i].action.targetRegion;
        unitGroup = (*armySide)[playerActions[i].pos];

#ifdef DEBUG_ORDERS
        bool printError = false;
        if (playerActions[i].isFriendly != player) { DEBUG("Picking the wrong army"); printError = true; }
        if (playerActions[i].unitTypeId != unitGroup->unitTypeId) { DEBUG("UnitTypeID mismatch"); printError = true; }
        if (playerActions[i].regionId != unitGroup->regionId) { DEBUG("regionID mismatch"); printError = true; }
        if (printError) {
            std::stringstream tmp;
            tmp << "List of all actions (* means current):" << std::endl;
            for (unsigned int j = 0; j < playerActions.size(); ++j) {
                if (j == i) tmp << "*";
                tmp << "Vector pos: " << playerActions[j].pos << " -> " << BWAPI::UnitType(playerActions[j].unitTypeId).getName() << " at " << intToString(playerActions[j].regionId)
                    << " " << abstractOrder::name[playerActions[j].action.orderID] + " to " + intToString(playerActions[j].action.targetRegion) << std::endl;
            }
            LOG(tmp.str());
            LOG("State before trying to update the actions to perform");
            LOG(toString());
        }
#endif
        unitGroup->orderId = orderId;
        unitGroup->targetRegionId = targetRegionId;
        unitGroup->startFrame = time;

        switch (orderId) {
            case abstractOrder::Move:
                timeToMove = getMoveTime(unitGroup->unitTypeId, unitGroup->regionId, targetRegionId);
                unitGroup->endFrame = time + timeToMove;
                break;
            case abstractOrder::Attack:
                // keep track of the regions with units in attack state
                regionsInCombat.insert(unitGroup->regionId);
                computeCombatLengthIn.insert(unitGroup->regionId);
                break;
            case abstractOrder::Nothing: // for buildings
                unitGroup->endFrame = time + NOTHING_TIME;
                break;
            default: // Unknown or Idle
                unitGroup->endFrame = time + IDLE_TIME;
                break;
        }
    }
    // if we have new attack actions, compute the combat length
    for (const auto& regionId : computeCombatLengthIn) { calculateCombatLengthAtRegion(regionId); }
#if _DEBUG
    ordersSanityCheck();
#endif
}

// © Alberto Uriarte
void GameState::calculateCombatLengthAtRegion(int regionId)
{
    army_t groupsInRegion(getGroupsInRegion(regionId));
#ifdef _DEBUG
    if (groupsInRegion.friendly.empty() || groupsInRegion.enemy.empty()) {
        DEBUG("Starting a combat where no adversarial groups");
    }
#endif
    calculateCombatLength(groupsInRegion);
}

// © me & Alberto Uriarte
void GameState::calculateCombatLength(army_t& army)
{
    int combatLenght = cs->getCombatLength(&army);
#ifdef _DEBUG
    if (combatLenght == INT_MAX) {
        DEBUG("Infinit combat lenght at region " << (int)army.enemy[0]->regionId);
        LOG(toString());
    }
#endif
    int combatEnd = combatLenght + time;
#ifdef _DEBUG
    if (combatEnd < 0 || combatEnd < time) {
        LOG("Combat lenght overflow");
    }
#endif
    // update end combat time for each unit attacking in this region
    for (auto unitGroup : army.friendly) updateCombatLenght(unitGroup, combatEnd);
    for (auto unitGroup : army.enemy) updateCombatLenght(unitGroup, combatEnd);
}

// © Alberto Uriarte
GameState::army_t GameState::getGroupsInRegion(int regionId)
{
    army_t groupsInRegion;
    for (const auto& unitGroup : army.friendly){
        if (unitGroup->regionId == regionId) groupsInRegion.friendly.emplace_back(unitGroup);
    }
    for (const auto& unitGroup : army.enemy){
        if (unitGroup->regionId == regionId) groupsInRegion.enemy.emplace_back(unitGroup);
    }
    return groupsInRegion;
}

// © Alberto Uriarte
void GameState::updateCombatLenght(unitGroup_t* group, int combatWillEndAtFrame)
{
    if (group->orderId == abstractOrder::Attack) {
        group->startFrame = time;
        group->endFrame = combatWillEndAtFrame;
    }
}

// © Alberto Uriarte
void GameState::setAttackOrderToIdle(unitGroupVector &myArmy)
{
    for (auto unitGroup : myArmy) {
        if (unitGroup->orderId == abstractOrder::Attack) {
            unitGroup->orderId = abstractOrder::Idle;
            unitGroup->endFrame = time;
        }
    }
}

// © Alberto Uriarte
// has time to execute all actions
bool GameState::canExecuteAnyAction(bool player) const
{
    const unitGroupVector *armySide;
    if (player) { armySide = &army.friendly; }
    else        { armySide = &army.enemy; }

    for (const auto& groupUnit : *armySide) {
        if (groupUnit->endFrame <= time) { return true; }
    }
    return false;
}

// © me & Alberto Uriarte
// Move forward the game state until the first unit finish its action (might delete unitGroup_t*)
void GameState::moveForward(int forwardTime)
{
    if (cs == nullptr) {
        DEBUG("Error, no combat simulator defined");
        return;
    }
#ifdef _DEBUG
    sanityCheck();
#endif
    unitGroupVector* armySide;
    bool moveUntilFinishAnAction = !forwardTime;

    if (moveUntilFinishAnAction) {
        forwardTime = INT_MAX;
        // search the first action to finish
        for (const auto& unitGroup : army.friendly) forwardTime = std::min(forwardTime, unitGroup->endFrame);
        for (const auto& unitGroup : army.enemy)    forwardTime = std::min(forwardTime, unitGroup->endFrame);
    }

    // forward the time of the game state to the first action to finish
    int timeStep = forwardTime - time;
    time = forwardTime;

    // update finished actions
    // ############################################################################################
    std::set<int> simulateCombatIn; // set because we don't want duplicates
    std::set<int> simulatePartialCombatIn; // set because we don't want duplicates
    std::map<int, army_t> unitGroupWasAtRegion, unitGroupWasNotAtRegion;
    for (int i = 0; i < 2; ++i) {
        if (i == 0) { armySide = &army.friendly; }
        else        { armySide = &army.enemy; }
        for (const auto& unitGroup : *armySide) {
            if (unitGroup->endFrame <= time) {
                if (unitGroup->orderId == abstractOrder::Move) {
                    int fromRegionId = unitGroup->regionId;
                    int toRegionId = unitGroup->targetRegionId;
                    // if we are moving to/from a region in combat we need to forward the combat time
                    if (regionsInCombat.find(fromRegionId) != regionsInCombat.end()) {
                        simulatePartialCombatIn.insert(fromRegionId);
                        if (i == 0) {
                            unitGroupWasAtRegion[fromRegionId].friendly.push_back(unitGroup);
                        }
                        else {
                            unitGroupWasAtRegion[fromRegionId].enemy.push_back(unitGroup);
                        }
                    }
                    if (regionsInCombat.find(toRegionId) != regionsInCombat.end()) {
                        simulatePartialCombatIn.insert(toRegionId);
                        if (i == 0) {
                            unitGroupWasNotAtRegion[toRegionId].friendly.push_back(unitGroup);
                        }
                        else {
                            unitGroupWasNotAtRegion[toRegionId].enemy.push_back(unitGroup);
                        }
                    }
                    // we are only updating the position (no "merge group" operation)
                    unitGroup->regionId = unitGroup->targetRegionId;
                    unitGroup->orderId = abstractOrder::Idle;
                } else if (unitGroup->orderId == abstractOrder::Attack) {
                    // keep track of the regions with units in attack state
                    simulateCombatIn.insert(unitGroup->regionId);
                    unitGroup->orderId = abstractOrder::Idle;
                }
            }
        }
    }

    // sometimes a finished combat is simulated at the same time of a partial combat. The partial combat has priority
    for (auto& regionId : simulatePartialCombatIn) {
        simulateCombatIn.erase(regionId);
    }

    // simulate FINISHED combat in regions
    // ############################################################################################
    for (const auto& regionId : simulateCombatIn) {
        army_t groupsInCombat = getGroupsInRegion(regionId);
        // sometimes our target moved away
        if (!groupsInCombat.friendly.empty() && !groupsInCombat.enemy.empty() && hasTargetableEnemy(groupsInCombat)) {
            combatsSimulated++;
            cs->simulateCombat(&groupsInCombat, &army);
        }
        regionsInCombat.erase(regionId);
    }
    // simulate NOT FINISHED combat in regions (because one unit left/arrive to the region)
    // ############################################################################################
    for (auto& regionId : simulatePartialCombatIn) {
        // sometimes we left/arrive when the battle is over
        if (regionsInCombat.find(regionId) == regionsInCombat.end()) continue;
        army_t groupsInCombat = getGroupsInRegion(regionId);
        // we need to ADD unitGroups that were in the region
        for (auto& unitGroup : unitGroupWasAtRegion[regionId].friendly) groupsInCombat.friendly.push_back(unitGroup);
        for (auto& unitGroup : unitGroupWasAtRegion[regionId].enemy) groupsInCombat.enemy.push_back(unitGroup);
        // we need to REMOVE unitGroups that weren't in the region
        for (auto& unitGroup : unitGroupWasNotAtRegion[regionId].friendly) {
            groupsInCombat.friendly.erase(std::remove(groupsInCombat.friendly.begin(), groupsInCombat.friendly.end(), unitGroup), groupsInCombat.friendly.end());
        }
        for (auto& unitGroup : unitGroupWasNotAtRegion[regionId].enemy) {
            groupsInCombat.enemy.erase(std::remove(groupsInCombat.enemy.begin(), groupsInCombat.enemy.end(), unitGroup), groupsInCombat.enemy.end());
        }
        // simulate
        int combatSeps = time - getCombatStartedFrame(groupsInCombat);
#ifdef _DEBUG
        combatUnitExistSanityCheck(groupsInCombat);
        GameState gameCopy(*this); // to check the state before simulating the combat
#endif
        combatsSimulated++;
        cs->simulateCombat(&groupsInCombat, &army, std::max(combatSeps, timeStep));
#ifdef _DEBUG
        combatUnitExistSanityCheck(getGroupsInRegion(regionId));
        ordersSanityCheck();
        bool print = false;
        if (print) LOG(gameCopy.toString());
#endif
    }
    // update partial combat times
    for (auto& regionId : simulatePartialCombatIn) {
        army_t groupsSurvived = getGroupsInRegion(regionId);
        cs->removeHarmlessIndestructibleUnits(&groupsSurvived);
        // if still groups to fight, update combat length
        if (!groupsSurvived.friendly.empty() && !groupsSurvived.enemy.empty() && hasAttackOrderAndTargetableEnemy(groupsSurvived)) {
            calculateCombatLength(groupsSurvived);
        } else { // else set attack groups to IDLE
            setAttackOrderToIdle(groupsSurvived.friendly);
            setAttackOrderToIdle(groupsSurvived.enemy);
            regionsInCombat.erase(regionId);
        }
    }
#ifdef _DEBUG
    sanityCheck();
#endif
//     calculateCombatTimes(); // to refresh combat list and combat times
    mergeGroups(); // auto merge groups with the same action

    if (!simulateCombatIn.empty() || !simulatePartialCombatIn.empty()) {
        // sometimes a group was killed by a "moving" group, therefore we need to
        // move forward again if none of the players can move
        if (!gameover() && !canExecuteAnyAction(true) && !canExecuteAnyAction(false)) {
#ifdef _DEBUG
            sanityCheck();
#endif
            moveForward();
        }
    }

#ifdef _DEBUG
    int nextAlt = 1;
    if (moveUntilFinishAnAction && !gameover() && getNextPlayerToMove(nextAlt) == -1) {
        DEBUG("None of the players can move on the next simulation step");
    }
    sanityCheck();
#endif
}

// © Alberto Uriarte
int GameState::getCombatStartedFrame(const army_t& army)
{
    int combatStarted = time;
    for (const auto& unitGroup : army.friendly) {
        if (unitGroup->orderId == abstractOrder::Attack && unitGroup->startFrame < combatStarted) {
            combatStarted = unitGroup->startFrame;
        }
    }
    for (const auto& unitGroup : army.enemy) {
        if (unitGroup->orderId == abstractOrder::Attack && unitGroup->startFrame < combatStarted) {
            combatStarted = unitGroup->startFrame;
        }
    }
    return combatStarted;
}

// © Alberto Uriarte
void GameState::mergeGroups() {
    mergeGroup(army.friendly);
    mergeGroup(army.enemy);
}

// © Alberto Uriarte
void GameState::mergeGroup(unitGroupVector& armySide)
{
    for (unitGroupVector::iterator it = armySide.begin(); it != armySide.end(); ++it) {
        for (unitGroupVector::iterator it2 = armySide.begin(); it2 != armySide.end();) {
            if (it != it2 &&
                (*it)->unitTypeId == (*it2)->unitTypeId &&
                (*it)->regionId == (*it2)->regionId && (*it)->orderId == (*it2)->orderId) {
#ifdef _DEBUG
                if ((*it)->orderId == abstractOrder::Attack &&
                    ((*it)->startFrame != (*it2)->startFrame || (*it)->endFrame != (*it2)->endFrame)) {
                    LOG("Merging 2 groups attacking but with different combat times");
                }
#endif
                // merge
                (*it)->numUnits += (*it2)->numUnits;
                (*it)->endFrame = std::min((*it)->endFrame, (*it2)->endFrame);
                // and delete old group
                delete *it2;
                it2 = armySide.erase(it2);
            } else {
                ++it2;
            }
        }
    }
}

// © Alberto Uriarte
int GameState::winner() const {
    if (army.friendly.empty() && army.enemy.empty()) return -1;
    if (army.friendly.empty()) return 1;
    if (army.enemy.empty()) return 0;
    return -1;
}

// © Alberto Uriarte
bool GameState::gameover() const {
    if (army.friendly.empty() || army.enemy.empty()) { return true; }
    // only buildings is also "gameover" (nothing to do)
    if (hasOnlyBuildings(army.friendly) && hasOnlyBuildings(army.enemy)) { return true; }

    return false;
}

// © Alberto Uriarte
bool GameState::hasOnlyBuildings(const unitGroupVector& armySide) const {
    for (const auto& unitGroup : armySide) {
        BWAPI::UnitType unitType(unitGroup->unitTypeId);
        if (!unitType.isBuilding()) return false;
    }
    return true;
}

// © Alberto Uriarte
// copies current game state and advances it to the next state
GameState GameState::cloneIssue(const playerActions_t& playerActions, bool player) const {
    GameState nextGameState(*this);
    nextGameState.execute(playerActions, player);
    nextGameState.moveForward();
    return nextGameState;
}

// © Alberto Uriarte
void GameState::resetFriendlyActions()
{
    for (auto& unitGroup : army.friendly) {
        // skip buildings
        BWAPI::UnitType unitType(unitGroup->unitTypeId);
        if (unitType.isBuilding()) { continue; }

        unitGroup->endFrame = time;
    }
}

// © Alberto Uriarte
void GameState::compareAllUnits(const GameState& gs, int &misplacedUnits, int &totalUnits)
{
    compareUnits(army.friendly, gs.army.friendly, misplacedUnits, totalUnits);
    compareUnits(army.enemy, gs.army.enemy, misplacedUnits, totalUnits);
}

// © Alberto Uriarte
void GameState::compareFriendlyUnits(const GameState& gs, int &misplacedUnits, int &totalUnits)
{
    compareUnits(army.friendly, gs.army.friendly, misplacedUnits, totalUnits);
}

// © Alberto Uriarte
void GameState::compareEnemyUnits(const GameState& gs, int &misplacedUnits, int &totalUnits)
{
    compareUnits(army.enemy, gs.army.enemy, misplacedUnits, totalUnits);
}

// © Alberto Uriarte
void GameState::compareUnits(const unitGroupVector& units1, const unitGroupVector& units2, int& misplacedUnits, int& totalUnits)
{
    bool match;
    for (auto groupUnit1 : units1) {
        match = false;
        for (auto groupUnit2 : units2) {
            if (groupUnit1->unitTypeId == groupUnit2->unitTypeId && groupUnit1->regionId == groupUnit2->regionId) {
                misplacedUnits += abs(groupUnit1->numUnits - groupUnit2->numUnits);
                totalUnits     += std::max(groupUnit1->numUnits, groupUnit2->numUnits);
                match = true;
                break; // we find it, stop looking for a match
            }
        }
        if (!match) {
            misplacedUnits += groupUnit1->numUnits;
            totalUnits     += groupUnit1->numUnits;
        }
    }

    for (auto groupUnit2 : units2) {
        match = false;
        for (auto groupUnit1 : units1) {
            if (groupUnit1->unitTypeId == groupUnit2->unitTypeId && groupUnit1->regionId == groupUnit2->regionId) {
                match = true;
                break;
            }
        }
        if (!match) {
            misplacedUnits += groupUnit2->numUnits;
            totalUnits     += groupUnit2->numUnits;
        }
    }
}

// © Alberto Uriarte
void GameState::compareUnits2(const unitGroupVector& units1, const unitGroupVector& units2, int &intersectionUnits, int &unionUnits)
{
    bool match;
    for (auto groupUnit1 : units1) {
        match = false;
        for (auto groupUnit2 : units2) {
            if (groupUnit1->unitTypeId == groupUnit2->unitTypeId && groupUnit1->regionId == groupUnit2->regionId) {
                intersectionUnits += std::min(groupUnit1->numUnits, groupUnit2->numUnits);
                unionUnits += std::max(groupUnit1->numUnits, groupUnit2->numUnits);
                match = true;
                break; // we find it, stop looking for a match
            }
        }
        if (!match) { unionUnits += groupUnit1->numUnits; }
    }
    for (auto groupUnit2 : units2) {
        match = false;
        for (auto groupUnit1 : units1) {
            if (groupUnit1->unitTypeId == groupUnit2->unitTypeId && groupUnit1->regionId == groupUnit2->regionId) {
                match = true;
                break;
            }
        }
        if (!match) { unionUnits += groupUnit2->numUnits; }
    }
}

// © Alberto Uriarte
// returns the Jaccard index comparing all the units from the current game state against other game state
float GameState::getJaccard(const GameState& gs) {
    int intersectionUnits = 0;
    int unionUnits = 0;
    compareUnits2(army.friendly, gs.army.friendly, intersectionUnits, unionUnits);
    compareUnits2(army.enemy, gs.army.enemy, intersectionUnits, unionUnits);
    return (unionUnits == 0 ? 1.0 : (float(intersectionUnits) / float(unionUnits)));
}

// © Alberto Uriarte
// returns the Jaccard index using a quality measure
// given a initialState we compare the "quality" of the current state and the other finalState
float GameState::getJaccard2(const GameState& initialState, const GameState& finalState, bool useKillScore)
{
    const double K = 1.0f;

    double intersectionSum = 0;
    double unionSum = 0;

    compareUnitsWithWeight(initialState.army.friendly, K, useKillScore, army.friendly, finalState.army.friendly, intersectionSum, unionSum);
    compareUnitsWithWeight(initialState.army.enemy, K, useKillScore, army.enemy, finalState.army.enemy, intersectionSum, unionSum);

    return unionSum == 0 ? 1.0 : float(intersectionSum / unionSum);
}

// © Alberto Uriarte
// returns the prediction accuracy given a initialState and other finalState like us
float GameState::getPredictionAccuracy(const GameState& initialState, const GameState& finalState)
{
    // get total number of units from initialState
    int numUnitsInitialState = 0;
    for (const auto& groupUnit : initialState.army.friendly) numUnitsInitialState += groupUnit->numUnits;
    for (const auto& groupUnit : initialState.army.enemy) numUnitsInitialState += groupUnit->numUnits;

    // sum of difference in intersection
    int sumDiffIntersec = 0;
    for (const auto& groupUnit1 : initialState.army.friendly) {
        // find intersection with our state (this)
        int intersection1 = getUnitIntersection(groupUnit1, army.friendly);
        // find intersection with other state (finalState)
        int intersection2 = getUnitIntersection(groupUnit1, finalState.army.friendly);
        // accumulate the difference
        sumDiffIntersec += std::abs(intersection1 - intersection2);
    }
    for (const auto& groupUnit1 : initialState.army.enemy) {
        // find intersection with our state (this)
        int intersection1 = getUnitIntersection(groupUnit1, army.enemy);
        // find intersection with other state (finalState)
        int intersection2 = getUnitIntersection(groupUnit1, finalState.army.enemy);
        // accumulate the difference
        sumDiffIntersec += std::abs(intersection1 - intersection2);
    }
    // 1 - (sum of difference in intersection / initial union)
    return 1.0 - (float(sumDiffIntersec) / float(numUnitsInitialState));
}

// © Alberto Uriarte
int GameState::getUnitIntersection(const unitGroup_t* groupUnit1, const unitGroupVector &groupUnitList)
{
    int intersection = 0;
    for (const auto& groupUnit2 : groupUnitList) {
        if (groupUnit1->unitTypeId == groupUnit2->unitTypeId && groupUnit1->regionId == groupUnit2->regionId) {
            intersection = std::min(groupUnit1->numUnits, groupUnit2->numUnits);
        }
    }
    return intersection;
}

// © Alberto Uriarte
void GameState::compareUnitsWithWeight(unitGroupVector unitsWeight, const double K, bool useKillScore,
    unitGroupVector units1, unitGroupVector units2, double &intersectionUnits, double &unionUnits)
{
    // get weights from the initialState
    // weight = killscore * (1 / (n + k))
    std::map<uint8_t, double> weights;
    double score = 0.0;
    for (auto groupUnit : unitsWeight) {
        score = 1 / ((double)groupUnit->numUnits + K);
        weights.insert(std::pair<uint8_t, double>(groupUnit->unitTypeId, score));
        if (useKillScore) {
            BWAPI::UnitType unitType(groupUnit->unitTypeId);
            weights[groupUnit->unitTypeId] *= unitType.destroyScore();
        }
    }

    bool match;
    for (auto groupUnit1 : units1) {
        match = false;
        for (auto groupUnit2 : units2) {
            if (groupUnit1->unitTypeId == groupUnit2->unitTypeId && groupUnit1->regionId == groupUnit2->regionId) {
                intersectionUnits += (double)std::min(groupUnit1->numUnits, groupUnit2->numUnits) * weights[groupUnit1->unitTypeId];
                unionUnits        += (double)std::max(groupUnit1->numUnits, groupUnit2->numUnits) * weights[groupUnit1->unitTypeId];
                match = true;
                break; // we find it, stop looking for a match
            }
        }
        if (!match) { unionUnits += groupUnit1->numUnits; }
    }
    for (auto groupUnit2 : units2) {
        match = false;
        for (auto groupUnit1 : units1) {
            if (groupUnit1->unitTypeId == groupUnit2->unitTypeId && groupUnit1->regionId == groupUnit2->regionId) {
                match = true;
                break;
            }
        }
        if (!match) { unionUnits += (double)groupUnit2->numUnits  * weights[groupUnit2->unitTypeId]; }
    }
}

// © me & Alberto Uriarte
int GameState::getNextPlayerToMove(int& nextPlayerInSimultaneousNode) const
{
    if (gameover()) return -1;
    int nextPlayer = -1;
    if (canExecuteAnyAction(true)) {
        if (canExecuteAnyAction(false)) {
            // if both can move: alternate
            nextPlayer = nextPlayerInSimultaneousNode;
            nextPlayerInSimultaneousNode = 1 - nextPlayerInSimultaneousNode;
        }
        else { return 1; }
    }
    else if (canExecuteAnyAction(false)) { return 0; }
    return nextPlayer; // has to be gameover
}

// © Alberto Uriarte
// check if a unit listed in the combat list is still alive
void GameState::combatUnitExistSanityCheck(army_t combatArmy)
{
    for (const auto& unitGroup : combatArmy.friendly) {
        auto groupFound = std::find(army.friendly.begin(), army.friendly.end(), unitGroup);
        if (groupFound == army.friendly.end()) {
            DEBUG("UNIT DOES NOT EXIST");
        }
    }

    for (const auto& unitGroup : combatArmy.enemy) {
        auto groupFound = std::find(army.enemy.begin(), army.enemy.end(), unitGroup);
        if (groupFound == army.enemy.end()) {
            DEBUG("UNIT DOES NOT EXIST");
        }
    }
}

// © Alberto Uriarte
void GameState::sanityCheck()
{
    ordersSanityCheck();
    // check if regions in combat is still valid
    for (const auto& regionId : regionsInCombat) {
        army_t armyInCombat = getGroupsInRegion(regionId);
        if (armyInCombat.friendly.empty() || armyInCombat.enemy.empty()) {
            DEBUG("No opossing army");
        }
        if (!hasAttackOrderAndTargetableEnemy(armyInCombat)) {
            DEBUG("No group attacking");
        }
    }
}

// © Alberto Uriarte
std::set<int> GameState::getArmiesRegionsIntersection() {
    std::set<int> result;
    for (const auto& enemyGroup : army.enemy){
        for (const auto& friendlyGroup : army.friendly){
            if (friendlyGroup->regionId == enemyGroup->regionId) result.insert(enemyGroup->regionId);
        }
    }
    return result;
}

// © Alberto Uriarte
bool GameState::hasAttackOrderAndTargetableEnemy(const army_t& army)
{
    for (const auto& group : army.friendly) {
        if (group->orderId == abstractOrder::Attack) {
            for (const auto& group2 : army.enemy) {
                if (group2->regionId == group->regionId &&
                    canAttackType(UnitType(group->unitTypeId), UnitType(group2->unitTypeId))) {
                    return true;
                }
            }
        }
    }
    for (const auto& group : army.enemy) {
        if (group->orderId == abstractOrder::Attack) {
            for (const auto& group2 : army.friendly) {
                if (group2->regionId == group->regionId &&
                    canAttackType(UnitType(group->unitTypeId), UnitType(group2->unitTypeId))) {
                    return true;
                }
            }
        }
    }
    return false;
}

// © Alberto Uriarte
bool GameState::hasTargetableEnemy(const army_t& army)
{
    for (const auto& group : army.friendly) {
        for (const auto& group2 : army.enemy) {
            if (group2->regionId == group->regionId &&
                canAttackType(UnitType(group->unitTypeId), UnitType(group2->unitTypeId))) {
                return true;
            }
        }
    }
    for (const auto& group : army.enemy) {
        for (const auto& group2 : army.friendly) {
            if (group2->regionId == group->regionId &&
                canAttackType(UnitType(group->unitTypeId), UnitType(group2->unitTypeId))) {
                return true;
            }
        }
    }
    return false;
}

// © Alberto Uriarte
void GameState::ordersSanityCheck()
{
    for (const auto& unitGroup : army.friendly) {
        BWAPI::UnitType unitType(unitGroup->unitTypeId);
        if (unitType.isBuilding() && unitGroup->orderId != abstractOrder::Nothing) {
            DEBUG("Building with wrong order!!");
        }
        if (unitGroup->endFrame < time) {
            DEBUG("Unit end frame action corrupted");
        }
        if (unitGroup->numUnits <= 0) {
            DEBUG("Wrong number of units");
        }
    }
}
