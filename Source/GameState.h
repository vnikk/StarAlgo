#pragma once

// TODO remove or add to solution
#include "AbstractGroup.h"
#include "AbstractOrder.h"

#include <BWAPI.h>
#include <BWTA.h>

#include <set>

class CombatSimulator;
class RegionManager;

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

    //GameState(); // by default uses a nullptr as combat simulator
    GameState(CombatSimulator* combatSim, RegionManager* regman);
    GameState(const GameState &gameState); // copy constructor
    ~GameState();
    const GameState& operator=(const GameState& gameState); // assignment operator

    RegionManager* _regman; // TODO remove: probs for Negamax

    int _buildingTypes;
    army_t _army;
    std::set<int> _regionsInCombat;
//     std::vector<army_t> combats;
    int _time;
    CombatSimulator* cs;
    bool friendlySiegeTankResearched;
    bool enemySiegeTankResearched;

//     void cleanData();
    void cleanArmyData();
    void loadIniConfig();
    int getMilitaryGroupsSize() const;
    int getAllGroupsSize() const;
    int getFriendlyGroupsSize() const;
    int getEnemyGroupsSize() const;
    int getFriendlyUnitsSize() const;
    int getEnemyUnitsSize() const;
    //void importCurrentGameState();
    void addAllEnemyUnits();
    void addSelfBuildings();
    unsigned short addFriendlyUnit(BWAPI::Unit unit);
    void addGroup(int unitId, int numUnits, int regionId, int listID, float groupHP, int orderId, int targetRegion, int endFrame);
    void addUnit(int unitId, int regionId, int listID, int orderId, float unitHP);
    unsigned short addUnitToArmySide(unitGroup_t* unit, unitGroupVector& armySide);
    void calculateExpectedEndFrameForAllGroups();
    int getAbstractOrderID(int orderId, int currentRegion, int targetRegion);
    std::string getAbstractOrderName(BWAPI::Order order, int currentRegion, int targetRegion);
    std::string getAbstractOrderName(int abstractId) const;
    std::string toString() const;
    
    void execute(const playerActions_t &playerActions, bool player);
    bool canExecuteAnyAction(bool player) const;
    void moveForward(int forwardTime = 0);
    int winner() const;
    bool gameover() const;
    bool hasOnlyBuildings(unitGroupVector armySide) const;
    GameState cloneIssue(playerActions_t playerActions, bool player) const;
    void mergeGroups();
    inline void mergeGroup(unitGroupVector& armySide);

    void resetFriendlyActions();
    BWAPI::Position getCenterRegionId(int regionId);
    int getRegionDistance(int regId1, int regId2);

    void compareAllUnits(GameState gs, int &misplacedUnits, int &totalUnits);
    void compareFriendlyUnits(GameState gs, int &misplacedUnits, int &totalUnits);
    void compareEnemyUnits(GameState gs, int &misplacedUnits, int &totalUnits);
    float getJaccard(GameState gs);
    float getJaccard2(GameState initialState, GameState finalState, bool useKillScore = false);
    float getPredictionAccuracy(GameState initialState, GameState finalState);
    int getUnitIntersection(const unitGroup_t* groupUnit1, const unitGroupVector &groupUnitList);

    void changeCombatSimulator(CombatSimulator* newCS);
    int getNextPlayerToMove(int &nextPlayerInSimultaneousNode) const;
    void sanityCheck();
    std::set<int> GameState::getArmiesRegionsIntersection();
    void addSquadToGameState(const BWAPI::Unitset& squad); // TODO change to Nova Squad; maybe??

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
    void compareUnits(unitGroupVector units1, unitGroupVector units2, int &misplacedUnits, int &totalUnits);
    void compareUnits2(unitGroupVector units1, unitGroupVector units2, int &intersectionUnits, int &unionUnits);
    void compareUnitsWithWeight(unitGroupVector unitsWeight, const double K, bool useKillScore,
        unitGroupVector units1, unitGroupVector units2, double &intersectionUnits, double &unionUnits);
    void combatUnitExistSanityCheck(army_t army);
    void ordersSanityCheck();
};
