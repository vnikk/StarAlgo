#include "stdafx.h"
#include "GameState.h"
#include "CombatSimSustained.h"
#include "CombatSimDecreased.h"

#include <BWTA.h>

#include <iomanip>
//#include "InformationManager.h"

const int IDLE_TIME = 400; //in frames
const int NOTHING_TIME = 24 * 60 * 30; //in frames (30 minutes)

using namespace BWAPI;

// player = true  = friendly player
// player = false = enemy player

struct SortByXY
{
    bool operator ()(const BWTA::Region* const &lReg, const BWTA::Region* const &rReg) const
    {
        BWAPI::Position lPos = lReg->getCenter();
        BWAPI::Position rPos = rReg->getCenter();
        if (lPos.x == rPos.x) {
            return lPos.y < rPos.y;
        } else {
            return lPos.x < rPos.x;
        }
    }

    bool operator ()(const BWTA::Chokepoint* const &lReg, const BWTA::Chokepoint* const &rReg) const
    {
        BWAPI::Position lPos = lReg->getCenter();
        BWAPI::Position rPos = rReg->getCenter();
        if (lPos.x == rPos.x) {
            return lPos.y < rPos.y;
        } else {
            return lPos.x < rPos.x;
        }
    }
};

void GameState::initRegion()  {
    const std::set<BWTA::Region*> &regions_unsort = BWTA::getRegions();
    std::set<BWTA::Region*, SortByXY> regions(regions_unsort.begin(), regions_unsort.end());

    // -- Assign region to identifiers
    int id = 0;
    for (std::set<BWTA::Region*, SortByXY>::const_iterator r = regions.begin(); r != regions.end(); ++r) {
        _regionID[*r] = id;
        _regionFromID[id] = *r;
        id++;
    }

    for (int x = 0; x < Broodwar->mapWidth(); ++x) {
        for (int y = 0; y < Broodwar->mapHeight(); ++y) {
            BWTA::Region* tileRegion = BWTA::getRegion(x, y);
            if (tileRegion == NULL) tileRegion = getNearestRegion(x, y);
            _regionIdMap[x][y] = _regionID[tileRegion];
        }
    }

    // -- Sort chokepoints
    const std::set<BWTA::Chokepoint*> &chokePoints_unsort = BWTA::getChokepoints();
    std::set<BWTA::Chokepoint*, SortByXY> chokePoints(chokePoints_unsort.begin(), chokePoints_unsort.end());

    // -- Assign chokepoints to identifiers and fill map
    for (std::set<BWTA::Chokepoint*, SortByXY>::const_iterator c = chokePoints.begin(); c != chokePoints.end(); ++c) {
        _chokePointID[*c] = id;
        _chokePointFromID[id] = *c;
        const std::pair<BWAPI::Position, BWAPI::Position> sides = (*c)->getSides();
//        drawInfluenceLine(sides.first, sides.second, id);
        id++;
    }
}

BWTA::Region*  GameState::getNearestRegion(int x, int y) {
    //searches outward in a spiral.
    int length = 1;
    int j = 0;
    bool first = true;
    int dx = 0;
    int dy = 1;
    BWTA::Region* tileRegion = NULL;
    while (length < Broodwar->mapWidth()) //We'll ride the spiral to the end
    {
        //if is a valid regions, return it
        tileRegion = BWTA::getRegion(x, y);
        if (tileRegion != NULL) return tileRegion;

        //otherwise, move to another position
        x = x + dx;
        y = y + dy;
        //count how many steps we take in this direction
        j++;
        if (j == length) { //if we've reached the end, its time to turn
            j = 0;    //reset step counter

            //Spiral out. Keep going.
            if (!first)
                length++; //increment step counter if needed


            first = !first; //first=true for every other turn so we spiral out at the right rate

            //turn counter clockwise 90 degrees:
            if (dx == 0) {
                dx = dy;
                dy = 0;
            }
            else {
                dy = -dx;
                dx = 0;
            }
        }
        //Spiral out. Keep going.
    }
    return tileRegion;
}

GameState::GameState()
    :_buildingTypes(NoneBuilding),
    _time(0),
    cs(nullptr),
    friendlySiegeTankResearched(false),
    enemySiegeTankResearched(false)
{
    _army.friendly.clear();
    _army.enemy.clear();
}

GameState::GameState(CombatSimulator* combatSim)
    :_buildingTypes(NoneBuilding),
    _time(0),
    cs(combatSim),
    friendlySiegeTankResearched(false),
    enemySiegeTankResearched(false)
{
    _army.friendly.clear();
    _army.enemy.clear();
}

GameState::~GameState()
{
    cleanArmyData();
    delete cs;
}

void GameState::cleanArmyData()
{
    for (auto unitGroup : _army.friendly) delete unitGroup;
    _army.friendly.clear();
    for (auto unitGroup : _army.enemy) delete unitGroup;
    _army.enemy.clear();
}

void GameState::changeCombatSimulator(CombatSimulator* newCS)
{
    delete cs;
    cs = newCS;
}

GameState::GameState(const GameState &other)
    :_buildingTypes(other._buildingTypes),
    _regionsInCombat(other._regionsInCombat),
    _time(other._time),
    cs(/*other.cs->clone()*/nullptr),
    friendlySiegeTankResearched(other.friendlySiegeTankResearched),
    enemySiegeTankResearched(other.enemySiegeTankResearched)
{
    for (auto unitGroup : other._army.friendly) _army.friendly.push_back(new unitGroup_t(*unitGroup));
    for (auto unitGroup : other._army.enemy) _army.enemy.push_back(new unitGroup_t(*unitGroup));
}

const GameState& GameState::operator=(const GameState& other)
{
    // clean previous data
    cleanArmyData();
    delete cs;
    // copy new data
    _buildingTypes = other._buildingTypes;
    for (auto unitGroup : other._army.friendly) _army.friendly.push_back(new unitGroup_t(*unitGroup));
    for (auto unitGroup : other._army.enemy) _army.enemy.push_back(new unitGroup_t(*unitGroup));
    _regionsInCombat = other._regionsInCombat;
    _time = other._time;
    cs = other.cs->clone();
    friendlySiegeTankResearched = other.friendlySiegeTankResearched;
    enemySiegeTankResearched = other.enemySiegeTankResearched;

    return *this;
}

int GameState::getMilitaryGroupsSize()
{
    int size = 0;
    for (const auto& unitGroup : _army.friendly) {
        BWAPI::UnitType unitType(unitGroup->unitTypeId);
        if (unitType.canAttack() || unitType.isSpellcaster()) ++size;
    }
    for (const auto& unitGroup : _army.enemy) {
        BWAPI::UnitType unitType(unitGroup->unitTypeId);
        if (unitType.canAttack() || unitType.isSpellcaster()) ++size;
    }
    return size;
}

int GameState::getAllGroupsSize()
{
    return _army.friendly.size() + _army.enemy.size();
}

int GameState::getFriendlyGroupsSize()
{
    return _army.friendly.size();
}

int GameState::getEnemyGroupsSize()
{
    return _army.enemy.size();
}

int GameState::getFriendlyUnitsSize()
{
    int size = 0;
    for (const auto& unitGroup : _army.friendly) {
        size += unitGroup->numUnits;
    }
    return size;
}

int GameState::getEnemyUnitsSize()
{
    int size = 0;
    for (const auto& unitGroup : _army.enemy) {
        size += unitGroup->numUnits;
    }
    return size;
}

void GameState::loadIniConfig()
{
    std::string iniBuilding = LoadConfigString("high_level_search", "buildings", "RESOURCE_DEPOT");
    if (iniBuilding == "NONE") {
        _buildingTypes = NoneBuilding;
    } else if (iniBuilding == "ALL") {
        _buildingTypes = AllBuildings;
    } else {
        _buildingTypes = ResourceDepot;
    }
}

/*
void GameState::importCurrentGameState()
{
    cleanArmyData();
    _time = Broodwar->getFrameCount();
    unitGroupVector* armySide;

    for (auto player : Broodwar->getPlayers()) {
        if (player->isNeutral()) continue;
        else if (_army.friendly.empty()) armySide = &_army.friendly;
        else armySide = &_army.enemy;

        for (auto unit : player->getUnits()) {
            // ignore buildings (except _buildingTypes)
            if ((_buildingTypes == ResourceDepot) && unit->getType().isBuilding() && !unit->getType().isResourceDepot() ) continue;
            // ignore units training
            if (!unit->isCompleted()) continue;
            if (unit->getType().isWorker()) continue;
            // ignore spells
            if (unit->getType().isSpell()) continue;

            uint8_t regionId = informationManager->_regionIdMap[unit->getTilePosition().x][unit->getTilePosition().y];
            uint8_t orderId = abstractOrder::Unknown;
            if (unit->getType().isBuilding()) orderId = abstractOrder::Nothing;
            float unitHP = unit->getHitPoints() + unit->getShields();
            unitGroup_t* unitGroup = new unitGroup_t(unit->getType().getID(), 1, regionId, orderId, 0, _time, unitHP);

            addUnitToArmySide(unitGroup, *armySide);
        }
    }

//     calculateExpectedEndFrameForAllGroups();
//     ordersSanityCheck();
}*/

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

// assumes army.enmey is empty
void GameState::addAllEnemyUnits()
{
    _time = Broodwar->getFrameCount();

    for (auto unit : Broodwar->enemy()->getUnits()) {
        // ignore buildings (except _buildingTypes)
        if ((_buildingTypes == ResourceDepot) && unit->getType().isBuilding() && !unit->getType().isResourceDepot() ) continue;
        // ignore units training
        if (!unit->isCompleted()) continue;
        if (unit->getType().isWorker()) continue;
        // ignore spells
        if (unit->getType().isSpell()) continue;

        uint8_t regionId = _regionIdMap[unit->getTilePosition().x][unit->getTilePosition().y];
        uint8_t orderId = abstractOrder::Unknown;
        if (unit->getType().isBuilding()) orderId = abstractOrder::Nothing;
        float unitHP = unit->getHitPoints() + unit->getShields();
        unitGroup_t* unitGroup = new unitGroup_t(unit->getType().getID(), 1, regionId, orderId, 0, _time, unitHP);

        addUnitToArmySide(unitGroup, _army.enemy);
    }
}

void GameState::addSelfBuildings()
{
    for (auto unit : Broodwar->self()->getUnits()) {
        if ( !unit->getType().isBuilding() ) continue;
        if ( _buildingTypes == ResourceDepot && !unit->getType().isResourceDepot() ) continue;

        uint8_t regionId = _regionIdMap[unit->getTilePosition().x][unit->getTilePosition().y];
        float unitHP = unit->getHitPoints() + unit->getShields();
        unitGroup_t* unitGroup = new unitGroup_t(unit->getType().getID(), 1, regionId, abstractOrder::Nothing, 0, _time, unitHP);

        addUnitToArmySide(unitGroup, _army.friendly);
    }
}

unsigned short GameState::addFriendlyUnit(Unit unit)
{
        uint8_t regionId = _regionIdMap[unit->getTilePosition().x][unit->getTilePosition().y];
        float unitHP = unit->getHitPoints() + unit->getShields();
        unitGroup_t* unitGroup = new unitGroup_t(unit->getType().getID(), 1, regionId, abstractOrder::Unknown, 0, _time, unitHP);

        return addUnitToArmySide(unitGroup, _army.friendly);
}

void GameState::addGroup(int unitId, int numUnits, int regionId, int listID, float groupHP, int orderId, int targetRegion, int endFrame)
{

    // simplify unitTypeId
//     if (unitId == UnitTypes::Terran_Siege_Tank_Siege_Mode) {
//         unitId = UnitTypes::Terran_Siege_Tank_Tank_Mode;
//     }
    // commented since this is only called for testing using the GUI

    UnitType unitType(unitId);
    if (unitType.isAddon() || unitType.isSpell()) return;

    if (unitType.isBuilding()) orderId = abstractOrder::Nothing;
    unitGroup_t* unitGroup = new unitGroup_t(unitId, numUnits, regionId, orderId, targetRegion, endFrame, _time, groupHP);

    if (listID == 1) {
        _army.friendly.push_back(unitGroup);
    } else if (listID == 2) {
        _army.enemy.push_back(unitGroup);
    }
}

void GameState::addUnit(int unitId, int regionId, int listID, int orderId, float unitHP)
{
    UnitType unitType(unitId);
    if (unitType.isAddon() || unitType.isSpell()) return;

    if (unitType.isBuilding()) orderId = abstractOrder::Nothing;
    unitGroup_t* unitGroup = new unitGroup_t(unitId, 1, regionId, orderId, 0, _time, unitHP);

    if (listID == 1) {
        addUnitToArmySide(unitGroup, _army.friendly);
    } else if (listID == 2) {
        addUnitToArmySide(unitGroup, _army.enemy);
    }
}



// Calculate the expected end frame for each order
void GameState::calculateExpectedEndFrameForAllGroups()
{
    _regionsInCombat.clear();
    unitGroupVector* armySide;

    for (int i = 0; i<2; ++i) {
        if (i == 0) armySide = &_army.friendly;
        else armySide = &_army.enemy;

        for (auto& unitGroup : *armySide) {
            switch (unitGroup->orderId) {
            case abstractOrder::Move:
                unitGroup->endFrame = _time + getMoveTime(unitGroup->unitTypeId, unitGroup->regionId, unitGroup->targetRegionId);
                break;
            case abstractOrder::Attack:
                _regionsInCombat.insert(unitGroup->regionId); // keep track of the regions with units in attack state
                break;
            case abstractOrder::Nothing: // it's a building
                unitGroup->endFrame = _time + NOTHING_TIME;
                break;
            case abstractOrder::Unknown:
            default:
                unitGroup->endFrame = _time + IDLE_TIME;
                break;
            }
        }
    }

#ifdef _DEBUG
    sanityCheck();
#endif

    for (const auto& regionId : _regionsInCombat) calculateCombatLengthAtRegion(regionId);
}

int GameState::getMoveTime(int unitTypeId, int regionId, int targetRegionId)
{
    if (regionId == targetRegionId) return 0;

    // auto-unsiege tanks
    // TODO warning we don't consider unsiege time
    if (unitTypeId == UnitTypes::Terran_Siege_Tank_Siege_Mode) {
        unitTypeId = UnitTypes::Terran_Siege_Tank_Tank_Mode;
    }
    UnitType unitType(unitTypeId);
    double speed = unitType.topSpeed(); //non-upgraded top speed in pixels per frame
    int distance = getRegionDistance(regionId, targetRegionId); // distance in pixels

    int timeToMove = (int)((double)distance / speed);
    if (timeToMove == INT_MAX || timeToMove < 0) {
        DEBUG("Wrong move time: " << timeToMove << " traveling from " << regionId << " to " << targetRegionId);
        DEBUG(toString());
    }
    return timeToMove;
}

// TODO remove?
BWAPI::Position GameState::getCenterRegionId(int regionId)
{
    // TODO use informationManager->_chokepointIdOffset
    BWTA::Region* region = _regionFromID[regionId];
    if (region != NULL) {
        return region->getCenter();
    } else {
        BWTA::Chokepoint* cp = _chokePointFromID[regionId];
        if (cp != NULL) {
            return cp->getCenter();
        } else {
            return BWAPI::Positions::None;
        }
    }
}

// TODO move this to BWTA
int GameState::getRegionDistance(int regId1, int regId2)
{
    /* assumed no chokes
    
    if (!_onlyRegions) {
        if (regId1 < _chokepointIdOffset) {
            // regId1 is a region, and regId2 a chokepoint
            return _distanceBetweenRegAndChoke[regId1][regId2];
        } else {
            // regId1 is a chokepoint, and regId2 a region
            return _distanceBetweenRegAndChoke[regId2][regId1];
        }
    } else {
    */
    //LOG("Distance from " << regId1 << " to " << regId2 << " = " << informationManager->_distanceBetweenRegions[regId1][regId2]);
        return _distanceBetweenRegions[regId1][regId2];
    //}
}

int GameState::getAbstractOrderID(int orderId, int currentRegion, int targetRegion)
{
     if ( orderId == BWAPI::Orders::MoveToMinerals ||
         orderId == BWAPI::Orders::WaitForMinerals ||
         orderId == BWAPI::Orders::MiningMinerals ||
         orderId == BWAPI::Orders::Harvest3 ||
         orderId == BWAPI::Orders::Harvest4 ||
          orderId == BWAPI::Orders::ReturnMinerals )
        //return abstractOrder::Mineral;
        return abstractOrder::Unknown;
     else if ( orderId == BWAPI::Orders::MoveToGas ||
              orderId == BWAPI::Orders::Harvest1 ||
              orderId == BWAPI::Orders::Harvest2 ||
              orderId == BWAPI::Orders::WaitForGas ||
              orderId == BWAPI::Orders::HarvestGas ||
               orderId == BWAPI::Orders::ReturnGas )
         //return abstractOrder::Gas;
        return abstractOrder::Unknown;
    else if ( orderId == BWAPI::Orders::Move ||
              orderId == BWAPI::Orders::Follow ||
              orderId == BWAPI::Orders::ComputerReturn ) 
        return abstractOrder::Move;
    else if ( orderId == BWAPI::Orders::AttackUnit ||
              orderId == BWAPI::Orders::AttackMove ||
              orderId == BWAPI::Orders::AttackTile)
          if (currentRegion == targetRegion) return abstractOrder::Attack;
          else return abstractOrder::Move;
    else if ( orderId == BWAPI::Orders::Repair || 
              orderId == BWAPI::Orders::MedicHeal )
            return abstractOrder::Heal;
    else if ( orderId == BWAPI::Orders::Nothing )
            return abstractOrder::Nothing;
    else {
        //LOG("Not detected action " << BWAPI::Order(orderId).getName() << " id: " << orderId);
        return abstractOrder::Unknown;
    }
}

std::string GameState::getAbstractOrderName(BWAPI::Order order, int currentRegion, int targetRegion)
{
    int abstractId = getAbstractOrderID(order.getID(), currentRegion, targetRegion);
    return abstractOrder::name[abstractId];
}

std::string GameState::getAbstractOrderName(int abstractId) const
{
    return abstractOrder::name[abstractId];
}

std::string GameState::toString() const
{
    std::stringstream tmp;
    tmp << "GameState: time " << _time << " ====================================\n";
    tmp << std::setw(5) << "Type" << std::setw(4) << "Num" << std::setw(4) << "Reg" 
        << std::setw(8) << "Order" << std::setw(6) << "toReg" << std::setw(9) << "EndFrame" 
        << std::setw(5) << "HP" << " " << "Unit name" << std::endl;
    for (const auto& group : _army.friendly) {
        tmp << std::setw(5) << intToString(group->unitTypeId)
        << std::setw(4) << intToString(group->numUnits)
        << std::setw(4) << intToString(group->regionId)
        << std::setw(8) << getAbstractOrderName(group->orderId)
        << std::setw(6) << intToString(group->targetRegionId)
        << std::setw(9) << intToString(group->endFrame - _time)
        << std::setw(5) << intToString(group->HP)
        << " " << BWAPI::UnitType(group->unitTypeId).getName() << std::endl;
    }
    tmp << "(up)FRIENDS---------------ENEMIES(down)\n";
    for (const auto& group : _army.enemy) {
        tmp << std::setw(5) << intToString(group->unitTypeId)
            << std::setw(4) << intToString(group->numUnits)
            << std::setw(4) << intToString(group->regionId)
            << std::setw(8) << getAbstractOrderName(group->orderId)
            << std::setw(6) << intToString(group->targetRegionId)
            << std::setw(9) << intToString(group->endFrame - _time)
            << std::setw(5) << intToString(group->HP)
            << " " << BWAPI::UnitType(group->unitTypeId).getName() << std::endl;
    }

    return tmp.str();
}

void GameState::execute(const playerActions_t &playerActions, bool player)
{
    ordersSanityCheck();
    unitGroupVector* armySide;
    if (player) armySide = &_army.friendly;
    else armySide = &_army.enemy;

    uint8_t orderId;
    uint8_t targetRegionId;
    unitGroup_t* unitGroup;
    int timeToMove;
    std::set<int> computeCombatLenghtIn; // set because we don't want duplicates

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
        unitGroup->startFrame = _time;

        switch (orderId) {
        case abstractOrder::Move:
            timeToMove = getMoveTime(unitGroup->unitTypeId, unitGroup->regionId, targetRegionId);
            unitGroup->endFrame = _time + timeToMove;
            break;
        case abstractOrder::Attack:
            // keep track of the regions with units in attack state
            _regionsInCombat.insert(unitGroup->regionId);
            computeCombatLenghtIn.insert(unitGroup->regionId);
            break;
        case abstractOrder::Nothing: // for buildings
            unitGroup->endFrame = _time + NOTHING_TIME;
            break;
        default: // Unknown or Idle
            unitGroup->endFrame = _time + IDLE_TIME;
            break;
        }

    }

    // if we have new attack actions, compute the combat length
    for (const auto& regionId : computeCombatLenghtIn) calculateCombatLengthAtRegion(regionId);

#if _DEBUG
    ordersSanityCheck();
#endif
}

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

void GameState::calculateCombatLength(army_t& army)
{
    int combatLenght = cs->getCombatLength(&army);

#ifdef _DEBUG
    if (combatLenght == INT_MAX) {
        DEBUG("Infinit combat lenght at region " << (int)army.enemy[0]->regionId);
        LOG(toString());
    }
#endif

    int combatEnd = combatLenght + _time;
#ifdef _DEBUG
    if (combatEnd < 0 || combatEnd < _time) {
        LOG("Combat lenght overflow");
    }
#endif

    // update end combat time for each unit attacking in this region
    for (auto unitGroup : army.friendly) updateCombatLenght(unitGroup, combatEnd);
    for (auto unitGroup : army.enemy) updateCombatLenght(unitGroup, combatEnd);
}

GameState::army_t GameState::getGroupsInRegion(int regionId)
{
    army_t groupsInRegion;
    for (const auto& unitGroup : _army.friendly){
        if (unitGroup->regionId == regionId) groupsInRegion.friendly.emplace_back(unitGroup);
    }
    for (const auto& unitGroup : _army.enemy){
        if (unitGroup->regionId == regionId) groupsInRegion.enemy.emplace_back(unitGroup);
    }
    return groupsInRegion;
}

void GameState::updateCombatLenght(unitGroup_t* group, int combatWillEndAtFrame)
{
    if (group->orderId == abstractOrder::Attack) {
        group->startFrame = _time;
        group->endFrame = combatWillEndAtFrame;
    }
}

void GameState::setAttackOrderToIdle(unitGroupVector &myArmy)
{
    for (auto unitGroup : myArmy) {
        if (unitGroup->orderId == abstractOrder::Attack) {
            unitGroup->orderId = abstractOrder::Idle;
            unitGroup->endFrame = _time;
        }
    }
}

bool GameState::canExecuteAnyAction(bool player) const
{
    const unitGroupVector *armySide;
    if (player) armySide = &_army.friendly;
    else armySide = &_army.enemy;

    for (const auto& groupUnit : *armySide) {
        if (groupUnit->endFrame <= _time) return true;
    }

    return false;
}

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
    bool moveUntilFinishAnAction = (forwardTime == 0);

    if (moveUntilFinishAnAction) {
        forwardTime = INT_MAX;
        // search the first action to finish
        for (const auto& unitGroup : _army.friendly) forwardTime = std::min(forwardTime, unitGroup->endFrame);
        for (const auto& unitGroup : _army.enemy)    forwardTime = std::min(forwardTime, unitGroup->endFrame);
    }

    // forward the time of the game state to the first action to finish
    int timeStep = forwardTime - _time;
    _time = forwardTime;

    // update finished actions
    // ############################################################################################
    std::set<int> simulateCombatIn; // set because we don't want duplicates
    std::set<int> simulatePartialCombatIn; // set because we don't want duplicates
    std::map<int, army_t> unitGroupWasAtRegion, unitGroupWasNotAtRegion;
    for (int i = 0; i<2; ++i) {
        if (i == 0) armySide = &_army.friendly;
        else armySide = &_army.enemy;
        for (const auto& unitGroup : *armySide) {
            if (unitGroup->endFrame <= _time) {
                if (unitGroup->orderId == abstractOrder::Move) {
                    int fromRegionId = (int)unitGroup->regionId;
                    int toRegionId = (int)unitGroup->targetRegionId;
                    // if we are moving to/from a region in combat we need to forward the combat time
                    if (_regionsInCombat.find(fromRegionId) != _regionsInCombat.end()) {
                        simulatePartialCombatIn.insert(fromRegionId);
                        if (i == 0) {
                            unitGroupWasAtRegion[fromRegionId].friendly.push_back(unitGroup);
                        } else {
                            unitGroupWasAtRegion[fromRegionId].enemy.push_back(unitGroup);
                        }
                    }
                    if (_regionsInCombat.find(toRegionId) != _regionsInCombat.end()) {
                        simulatePartialCombatIn.insert(toRegionId);
                        if (i == 0) {
                            unitGroupWasNotAtRegion[toRegionId].friendly.push_back(unitGroup);
                        } else {
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
            cs->simulateCombat(&groupsInCombat, &_army);
        }
        _regionsInCombat.erase(regionId);
    }
// #ifdef _DEBUG
//     sanityCheck();
// #endif

    // simulate NOT FINISHED combat in regions (because one unit left/arrive to the region)
    // ############################################################################################
    for (auto& regionId : simulatePartialCombatIn) {
        // sometimes we left/arrive when the battle is over
        if (_regionsInCombat.find(regionId) == _regionsInCombat.end()) continue;
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
        int combatSeps = _time - getCombatStartedFrame(groupsInCombat);
#ifdef _DEBUG
        combatUnitExistSanityCheck(groupsInCombat);
        GameState gameCopy(*this); // to check the state before simulating the combat
#endif
        combatsSimulated++;
        cs->simulateCombat(&groupsInCombat, &_army, std::max(combatSeps, timeStep));

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
            _regionsInCombat.erase(regionId);
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

int GameState::getCombatStartedFrame(const army_t& army)
{
    int combatStarted = _time;
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

void GameState::mergeGroups() {
    mergeGroup(_army.friendly);
    mergeGroup(_army.enemy);
}

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

int GameState::winner() const {
    if (_army.friendly.empty() && _army.enemy.empty()) return -1;
    if (_army.friendly.empty()) return 1;
    if (_army.enemy.empty()) return 0;
    return -1;
}

bool GameState::gameover() const {
    if (_army.friendly.empty() || _army.enemy.empty()) {
        return true;
    }
    // only buildings is also "gameover" (nothing to do)
    if (hasOnlyBuildings(_army.friendly) && hasOnlyBuildings(_army.enemy)) {
        return true;
    }

    return false;
}

bool GameState::hasOnlyBuildings(unitGroupVector armySide) const {
    for (const auto& unitGroup : armySide) {
        BWAPI::UnitType unitType(unitGroup->unitTypeId);
        if (!unitType.isBuilding()) return false;
    }
    return true;
}

GameState GameState::cloneIssue(playerActions_t playerActions, bool player) const {
    GameState nextGameState(*this);
    nextGameState.execute(playerActions, player);
    nextGameState.moveForward();
    return nextGameState;
}

void GameState::resetFriendlyActions()
{
    for (auto& unitGroup : _army.friendly) {
        // skip buildings
        BWAPI::UnitType unitType(unitGroup->unitTypeId);
        if (unitType.isBuilding()) continue;

        unitGroup->endFrame = _time;
    }
}

// void GameState::autoSiegeTanksAtRegion(int regionId)
// {
//     if (friendlySiegeTankResearched) autoSiegeArmy(combats[regionId].friendly);
//     if (enemySiegeTankResearched) autoSiegeArmy(combats[regionId].enemy);
// }

// void GameState::autoSiegeArmy(unitGroupVector &myArmy)
// {
//     for (auto& unitGroup : myArmy) {
//         if (unitGroup->unitTypeId == UnitTypes::Terran_Siege_Tank_Tank_Mode) {
//             unitGroup->unitTypeId = UnitTypes::Terran_Siege_Tank_Siege_Mode;
//         }
//     }
// }

void GameState::compareAllUnits(GameState gs, int &misplacedUnits, int &totalUnits)
{
    compareUnits(_army.friendly, gs._army.friendly, misplacedUnits, totalUnits);
    compareUnits(_army.enemy, gs._army.enemy, misplacedUnits, totalUnits);
}

void GameState::compareFriendlyUnits(GameState gs, int &misplacedUnits, int &totalUnits)
{
    compareUnits(_army.friendly, gs._army.friendly, misplacedUnits, totalUnits);
}

void GameState::compareEnemyUnits(GameState gs, int &misplacedUnits, int &totalUnits)
{
    compareUnits(_army.enemy, gs._army.enemy, misplacedUnits, totalUnits);
}

// TODO we are not considering units of the same type and region but with different orders!!
void GameState::compareUnits(unitGroupVector units1, unitGroupVector units2, int &misplacedUnits, int &totalUnits)
{
    bool match;
    for (auto groupUnit1 : units1) {
        match = false;
        for (auto groupUnit2 : units2) {
            if (groupUnit1->unitTypeId == groupUnit2->unitTypeId && groupUnit1->regionId == groupUnit2->regionId) {
                misplacedUnits += abs(groupUnit1->numUnits - groupUnit2->numUnits);
                totalUnits += std::max(groupUnit1->numUnits, groupUnit2->numUnits);
                match = true;
                break; // we find it, stop looking for a match
            }
        }
        if (!match) {
            misplacedUnits += groupUnit1->numUnits;
            totalUnits += groupUnit1->numUnits;
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
            totalUnits += groupUnit2->numUnits;
        }
    }
}

// TODO we are not considering units of the same type and region but with different orders!!
void GameState::compareUnits2(unitGroupVector units1, unitGroupVector units2, int &intersectionUnits, int &unionUnits)
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
        if (!match) {
            unionUnits += groupUnit1->numUnits;
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
            unionUnits += groupUnit2->numUnits;
        }
    }
}

// returns the Jaccard index comparing all the units from the current game state against other game state
float GameState::getJaccard(GameState gs) {
    int intersectionUnits = 0;
    int unionUnits = 0;
    compareUnits2(_army.friendly, gs._army.friendly, intersectionUnits, unionUnits);
    compareUnits2(_army.enemy, gs._army.enemy, intersectionUnits, unionUnits);
    return unionUnits == 0 ? 1.0 : (float(intersectionUnits) / float(unionUnits));
}

// returns the Jaccard index using a quality measure
// given a initialState we compare the "quality" of the current state and the other finalState
float GameState::getJaccard2(GameState initialState, GameState finalState, bool useKillScore)
{
    const double K = 1.0f;

    double intersectionSum = 0;
    double unionSum = 0;

    compareUnitsWithWeight(initialState._army.friendly, K, useKillScore, _army.friendly, finalState._army.friendly, intersectionSum, unionSum);
    compareUnitsWithWeight(initialState._army.enemy, K, useKillScore, _army.enemy, finalState._army.enemy, intersectionSum, unionSum);

    return unionSum == 0 ? 1.0 : float(intersectionSum / unionSum);
}

// returns the prediction accuracy given a initialState and other finalState like us
float GameState::getPredictionAccuracy(GameState initialState, GameState finalState)
{
    // get total number of units from initialState
    int numUnitsInitialState = 0;
    for (const auto& groupUnit : initialState._army.friendly) numUnitsInitialState += groupUnit->numUnits;
    for (const auto& groupUnit : initialState._army.enemy) numUnitsInitialState += groupUnit->numUnits;

    // sum of difference in intersection
    int sumDiffIntersec = 0;
    for (const auto& groupUnit1 : initialState._army.friendly) {
        // find intersection with our state (this)
        int intersection1 = getUnitIntersection(groupUnit1, _army.friendly);
        // find intersection with other state (finalState)
        int intersection2 = getUnitIntersection(groupUnit1, finalState._army.friendly);
        // accumulate the difference
        sumDiffIntersec += std::abs(intersection1 - intersection2);
    }
    for (const auto& groupUnit1 : initialState._army.enemy) {
        // find intersection with our state (this)
        int intersection1 = getUnitIntersection(groupUnit1, _army.enemy);
        // find intersection with other state (finalState)
        int intersection2 = getUnitIntersection(groupUnit1, finalState._army.enemy);
        // accumulate the difference
        sumDiffIntersec += std::abs(intersection1 - intersection2);
    }

    // 1 - (sum of difference in intersection / initial union)
    return 1.0 - (float(sumDiffIntersec) / float(numUnitsInitialState));
}

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
                unionUnits += (double)std::max(groupUnit1->numUnits, groupUnit2->numUnits) * weights[groupUnit1->unitTypeId];
                match = true;
                break; // we find it, stop looking for a match
            }
        }
        if (!match) {
            unionUnits += groupUnit1->numUnits;
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
            unionUnits += (double)groupUnit2->numUnits  * weights[groupUnit2->unitTypeId];
        }
    }
}

int GameState::getNextPlayerToMove(int &nextPlayerInSimultaneousNode) const
{
    if (gameover()) return -1;
    int nextPlayer = -1;
    if (canExecuteAnyAction(true)) {
        if (canExecuteAnyAction(false)) {
            // if both can move: alternate
            nextPlayer = nextPlayerInSimultaneousNode;
            nextPlayerInSimultaneousNode = 1 - nextPlayerInSimultaneousNode;
        } else {
            nextPlayer = 1;
        }
    } else {
        if (canExecuteAnyAction(false)) nextPlayer = 0;
    }
    return nextPlayer;
}

// check if a unit listed in the combat list is still alive
void GameState::combatUnitExistSanityCheck(army_t combatArmy)
{
    for (const auto& unitGroup : combatArmy.friendly) {
        auto groupFound = std::find(_army.friendly.begin(), _army.friendly.end(), unitGroup);
        if (groupFound == _army.friendly.end()) {
            DEBUG("UNIT DOES NOT EXIST");
        } 
    }

    for (const auto& unitGroup : combatArmy.enemy) {
        auto groupFound = std::find(_army.enemy.begin(), _army.enemy.end(), unitGroup);
        if (groupFound == _army.enemy.end()) {
            DEBUG("UNIT DOES NOT EXIST");
        }
    }
}

void GameState::sanityCheck()
{
    ordersSanityCheck();

    // check if regions in combat is still valid
    for (const auto& regionId : _regionsInCombat) {
        army_t armyInCombat(getGroupsInRegion(regionId));
        if (armyInCombat.friendly.empty() || armyInCombat.enemy.empty()) {
            DEBUG("No opossing army");
        }
        if (!hasAttackOrderAndTargetableEnemy(armyInCombat)) {
            DEBUG("No group attacking");
        }
    }
}

bool GameState::hasAttackOrderAndTargetableEnemy(const army_t& army)
{
    for (const auto& group : army.friendly) if (group->orderId == abstractOrder::Attack) {
        for (const auto& group2 : army.enemy) {
            if (group2->regionId == group->regionId &&
                canAttackType(UnitType(group->unitTypeId), UnitType(group2->unitTypeId))) {
                return true;
            }
        }
    }
    for (const auto& group : army.enemy) if (group->orderId == abstractOrder::Attack) {
        for (const auto& group2 : army.friendly) {
            if (group2->regionId == group->regionId &&
                canAttackType(UnitType(group->unitTypeId), UnitType(group2->unitTypeId))) {
                return true;
            }
        }
    }
    return false;
}

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

void GameState::ordersSanityCheck()
{
    for (const auto& unitGroup : _army.friendly) {
        BWAPI::UnitType unitType(unitGroup->unitTypeId);
        if (unitType.isBuilding() && unitGroup->orderId != abstractOrder::Nothing) {
            DEBUG("Building with wrong order!!");
        }
        if (unitGroup->endFrame < _time) {
            DEBUG("Unit end frame action corrupted");
        }
        if (unitGroup->numUnits <= 0) {
            DEBUG("Wrong number of units");
        }
    }
}
