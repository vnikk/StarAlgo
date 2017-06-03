#pragma once

#include "AbstractGroup.h"
#include "AbstractOrder.h"

#include <BWAPI.h>
#include <BWTA.h>

#include <set>

class CombatSimulator;
class RegionManager;

// © me & Alberto Uriarte
class GameState
{
public:
    enum BuildingOption {
        NoneBuilding,
        ResourceDepot,
        AllBuildings
    };

    struct army_t {
        unitGroupVector friendly;
        unitGroupVector enemy;
    };

    GameState(CombatSimulator* combatSim, RegionManager* regman);
    GameState(const GameState &gameState);
    const GameState& operator=(const GameState& gameState);
    ~GameState();

    CombatSimulator* cs;
    RegionManager*   regman;
    std::set<int>    regionsInCombat;
    army_t           army;
    int              buildingTypes;
    int              time;
    bool friendlySiegeTankResearched;
    bool enemySiegeTankResearched;

    void cleanArmyData();
    void loadIniConfig();

    int getMilitaryGroupsSize() const;
    int getAllGroupsSize()      const;
    int getFriendlyGroupsSize() const;
    int getEnemyGroupsSize()    const;
    int getFriendlyUnitsSize()  const;
    int getEnemyUnitsSize()     const;
    int getAbstractOrderID(int orderId, int currentRegion, int targetRegion) const;
    std::string getAbstractOrderName(BWAPI::Order order, int currentRegion, int targetRegion) const;
    std::string getAbstractOrderName(int abstractId) const;

    void addAllEnemyUnits();
    void addSelfBuildings();
    void addGroup(int unitId, int numUnits, int regionId, int listID, float groupHP, int orderId, int targetRegion, int endFrame);
    void addUnit(int unitId, int regionId, int listID, int orderId, float unitHP);
    void addSquadToGameState(const BWAPI::Unitset& squad);
    unsigned short addFriendlyUnit(BWAPI::Unit unit);
    unsigned short addUnitToArmySide(unitGroup_t* unit, unitGroupVector& armySide);

    void calculateExpectedEndFrameForAllGroups();
    void execute(const playerActions_t &playerActions, bool player);
    bool canExecuteAnyAction(bool player) const;
    void moveForward(int forwardTime = 0);
    int  winner() const;
    bool gameover() const;
    bool hasOnlyBuildings(const unitGroupVector& armySide) const;
    GameState cloneIssue(const playerActions_t& playerActions, bool player) const;
    void mergeGroups();
    inline void mergeGroup(unitGroupVector& armySide);

    void resetFriendlyActions();
    int getRegionDistance(int regId1, int regId2) const;

    void compareAllUnits(const GameState& gs, int &misplacedUnits, int &totalUnits);
    void compareFriendlyUnits(const GameState& gs, int &misplacedUnits, int &totalUnits);
    void compareEnemyUnits(const GameState& gs, int &misplacedUnits, int &totalUnits);
    float getJaccard(const GameState& gs);
    float getJaccard2(const GameState& initialState, const GameState& finalState, bool useKillScore = false);
    float getPredictionAccuracy(const GameState& initialState, const GameState& finalState);
    int getUnitIntersection(const unitGroup_t* groupUnit1, const unitGroupVector &groupUnitList);
    std::set<int> GameState::getArmiesRegionsIntersection();
    int getNextPlayerToMove(int &nextPlayerInSimultaneousNode) const;

    void changeCombatSimulator(CombatSimulator* newCS);
    void sanityCheck();

    std::string toString() const;

private:
    std::vector<int> regionsToDelete;

    void setAttackOrderToIdle(unitGroupVector &army);
    bool hasAttackOrderAndTargetableEnemy(const army_t& army);
    bool hasTargetableEnemy(const army_t& army);
    int getMoveTime(int unitTypeId, int regionId, int targetRegionId);
    void calculateCombatLengthAtRegion(int regionId);
    void calculateCombatLength(army_t& army);
    army_t getGroupsInRegion(int regionId);
    inline void updateCombatLenght(unitGroup_t* group, int combatWillEndAtFrame);
    int getCombatStartedFrame(const army_t& army);
    void compareUnits(const unitGroupVector& units1, const unitGroupVector& units2, int &misplacedUnits, int &totalUnits);
    void compareUnits2(const unitGroupVector& units1, const unitGroupVector& units2, int &intersectionUnits, int &unionUnits);
    void compareUnitsWithWeight(unitGroupVector unitsWeight, const double K, bool useKillScore,
        unitGroupVector units1, unitGroupVector units2, double &intersectionUnits, double &unionUnits);
    void combatUnitExistSanityCheck(army_t army);
    void ordersSanityCheck();
};
