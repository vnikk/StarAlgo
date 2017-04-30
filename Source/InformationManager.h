#pragma once

//#include "BuildManager.h"
#include "BWTA.h"
#include "GameState.h"

#define SCANNER_SWEEP_FREQUENCY 30

DWORD WINAPI AnalyzeMapThread();

struct unitCache_t {
    BWAPI::UnitType type;
    BWAPI::Position position;
    unitCache_t() :type(BWAPI::UnitTypes::None), position(BWAPI::Positions::None) {}
    unitCache_t(BWAPI::UnitType t, BWAPI::Position p) :type(t), position(p) {}
};

typedef std::map<BWAPI::UnitType, int> UnitToPercent;
typedef std::map<BWAPI::UnitType, BWAPI::UnitType> BuildingToUnitMap;
typedef std::map<BWAPI::UnitType, std::vector<BWAPI::TechType> > BuildingToTechMap;
typedef std::map<BWAPI::UnitType, std::vector<BWAPI::UpgradeType> > BuildingToUpgradeMap;
typedef std::map<BWAPI::UnitType, std::vector<BWAPI::UnitType> > BuildingToAddonMap;
typedef std::map<BWAPI::Unit, unitCache_t> UnitToCache;
typedef std::map<BWAPI::Unit, int> UnitToTimeMap;
typedef std::pair<BWTA::BaseLocation*, BWTA::BaseLocation*> baseDistanceKey;
typedef std::map<baseDistanceKey, double> baseDistanceMap;

class InformationManager
{
public:
    InformationManager();
    ~InformationManager();
    void analyzeMap();

#ifdef NOVA_GUI
    Qsignal* _GUIsignal;
#endif
    
    // Game state
    GameState gameState;
    GameState lastGameState;

    // Economical information
    void reserveMinerals(int minerals);
    void removeReservedMinerals(int minerals);
    void reserveGas(int gas);
    void removeReservedGas(int gas);
    int minerals();
    int gas();
    bool haveResources(BWAPI::UnitType type);
    bool haveResources(BWAPI::TechType type);
    bool haveResources(BWAPI::UpgradeType type);
    void frameSpend(BWAPI::UnitType type);
    void frameSpend(BWAPI::TechType type);
    void frameSpend(BWAPI::UpgradeType type);
    int _mineralsReserved;
    int _gasReserved;
    int _frameMineralSpend;
    int _frameGasSpend;
    unsigned int _workersNeededToBuild; // workers needed before start executing the build order
    unsigned int _maxWorkersMining;

    // Build request
    void buildRequest(BWAPI::UnitType type);
    void buildRequest(BWAPI::UnitType type, bool checkUnic);
    void criticalBuildRequest(BWAPI::UnitType type);
    void criticalBuildRequest(BWAPI::UnitType type, bool first);
    void addonRequest(BWAPI::UnitType addonType);
    void researchRequest(BWAPI::TechType techType);
    void upgradeRequest(BWAPI::UpgradeType upgradeType);
    void upgradeRequest(BWAPI::UpgradeType upgradeType, int level);
    // Base
    enum Expand
    {
        Natural,
        Gas
    };
    void naturalExpandRequest(bool critical = false);
    void gasExpandRequest(bool critical = false);
    void onCommandCenterShow(BWAPI::Unit unit);
    void onEnemyResourceDepotShow(BWAPI::Unit unit);
    void onCommandCenterDestroy(BWAPI::Unit unit);
    void onEnemyResourceDepotDestroy(BWAPI::Unit unit);
    BWAPI::TilePosition getExpandPosition();
    BWAPI::TilePosition getExpandPosition(bool gasRequired);
    inline bool existGasExpand(){ return existExpand(true); };
    inline bool existNaturalExpand(){ return existExpand(false); };
    bool existExpand(bool gasRequired);

    // enemy DPS map:
    double get_enemy_air_dps(BWAPI::TilePosition, int cycle);
    double get_enemy_ground_dps(BWAPI::TilePosition, int cycle);
    double get_enemy_air_dps(int x, int y, int cycle);
    double get_enemy_ground_dps(int x, int y, int cycle);
    void recompute_enemy_dps(int cycle);
    void drawAirDPSMap();
    void drawGroundDPSMap();

    // Military information
    UnitToCache visibleEnemies;
    UnitToCache seenEnemies;
    void markEnemyAsSeen(BWAPI::Unit unit);
    void markEnemyAsVisible(BWAPI::Unit unit);
    UnitToCache::iterator deleteSeenEnemy(UnitToCache::iterator unit);
    void updateLastSeenUnitPosition();
    bool _firstPush;
    unsigned int _minSquadSize; // WARNING! it counts units training (TODO fix it on BWAPI 4)
    bool _needAntiAirUnits;
    bool _retreatDisabled;
    bool _harassing;
    UnitToTimeMap _tankSiege;
        // Self army
        double _marines, _medics, _firebats, _ghosts;
        double _vultures, _tank, _goliath;
        double _wraiths;
        // Enemy Terran army
        double _enemyMarines, _enemyMedics, _enemyFirebats, _enemyGhosts;
        double _enemyVultures, _enemyTank, _enemyGoliath;
        double _enemyWraiths, _enemyDropship, _enemyScienceVessel, _enemyBattlecruiser, _enemyValkyrie;
        double _enemyTurrets;
        // Enemy Protoss army
        double _enemyZealot, _enemyDragoon, _enemyHTemplar, _enemyDTemplar, _enemyArchon, _enemyDArchon;
        double _enemyReaver, _enemyObserver, _enemyShuttle;
        double _enemyScout, _enemyCarrier, _enemyArbiter, _enemyCorsair;
        double _enemyPhotonCanon;
        // Enemy Zerg army
        double _enemyZergling, _enemyHydralisk, _enemyOverlord;
        double _enemyLurker, _enemyMutalisk;
        double _enemyUltralisk, _enemyGuardian, _enemyDevourer;
        double _enemySunkenColony, _enemySporeColony;
        // stats
        double _ourAirDPS, _ourAntiAirHP, _ourGroundDPS, _ourAirHP, _ourGroundHP;
        double _armySize;
        double _enemyAirDPS, _enemyAntiAirHP, _enemyGroundDPS, _enemyAirHP, _enemyGroundHP;
        double _enemyArmySize;

    // Production information
    BuildingToUnitMap _trainOrder;
    std::vector<BWAPI::UnitType> _buildRequest;
    std::vector<BWAPI::UnitType> _criticalBuildRequest;
    BuildingToAddonMap _addonOrder;
    BuildingToTechMap _researchOrder;
    BuildingToUpgradeMap _upgradeOrder;
    UnitToPercent _percentList;
    void checkRequirements(BWAPI::UnitType type);
    bool _autoVehicleUpgrade;
    bool _autoShipUpgrade;
    bool _autoInfanteryUpgrade;
    int _wastedProductionFramesByCommandCenter;
    int _wastedProductionFramesByResearch;
    int _wastedProductionFramesByMoney;
    int _wastedProductionFramesBySupply;

    // Events information
    void seenCloakedEnemy(BWAPI::TilePosition pos);
    std::set<BWAPI::TilePosition> _cloakedEnemyPositions;
    bool _turretDefenses;
    //bool _enemyWorkerScout;
    bool _scienceVesselDetector;
    bool _searchAndDestroy;
    int _searchCorner;
    int _searchIter;
    bool _panicMode;
    bool _bbs;

    // Base and Buildings
    Expand _expandType;
    std::map<BWTA::BaseLocation*, BWAPI::TilePosition> _ourBases;
    std::set<BWTA::BaseLocation*> _emptyBases;
    std::set<BWTA::BaseLocation*> _enemyBases;
    std::set<BWAPI::TilePosition> _ignoreBases;
    BWAPI::TilePosition _proxyPosition;
    bool _useProxyPosition;
    void onMissileTurretShow(BWAPI::Unit unit);
    void onMissileTurretDestroy(BWAPI::Unit unit);
    std::map<BWAPI::Unit, BWTA::Region*> _missileTurrets;
    bool _autoBuildSuplies;
    bool _priorCommandCenters;
    baseDistanceMap _baseDistance;
    BWAPI::Position _enemyStartPosition;
    BWTA::BaseLocation* _enemyStartBase;
    std::set<BWTA::BaseLocation*> startLocationCouldContainEnemy;
    void deleteEnemyStartLocation(BWAPI::Position _startPosition);
    void enemyStartFound(BWAPI::Position _startPosition);

    // Map information
    // ---------------
    bool mapAnalyzed;
    BWTA::Region* home;
    bool _scoutedAnEnemyBase;
    BWTA::Chokepoint* _choke;
    BWAPI::Position _initialRallyPosition;
    BWAPI::Position _bunkerSeedPosition;

    int _chokepointIdOffset;
    std::map<BWTA::Region*, int> _regionID;
    std::map<BWTA::Chokepoint*, int> _chokePointID;
    // TODO maybe use  boost::multi_index instead of two std::map?
    std::map<int, BWTA::Region*> _regionFromID;
    std::map<int, BWTA::Chokepoint*> _chokePointFromID;

    BWTA::RectangleArray<uint8_t> _regionIdMap;
    void drawInfluenceLine( BWAPI::Position pos1, BWAPI::Position pos2, const int id );
    static BWTA::Region* getNearestRegion(int x, int y);

    BWTA::RectangleArray<bool> _tankSiegeMap;
    // TODO move this to BWTA
    BWTA::RectangleArray<int> _distanceBetweenRegions;
    BWTA::RectangleArray<int> _distanceBetweenRegAndChoke;
    void spacePartitionStats();
    bool _onlyRegions;


    // Debug variables
    // ---------------
    void printDebugBox(BWAPI::Position topLeft, BWAPI::Position bottomRight, std::string message);
    void printDebugBox(BWAPI::Position position, std::string message) {
        printDebugBox(position, BWAPI::Position(position.x + TILE_SIZE, position.y + TILE_SIZE), message);
    };
    BWAPI::Position getDebugBoxTopLeft() { return _topLeft; };
    BWAPI::Position getDebugBoxBottomRight() { return _bottomRight; };
    BWAPI::Position _center;
    int _radius;
    BWAPI::Position _debugPosition1;
    BWAPI::Position _debugPosition2;

    // Enemy DPS map: (stores, for each cell of the map, the aggregated dps that the seen units of the enemy can do to our units in that cell)
    int _last_cycle_dps_map_was_recomputed;
    double *_ground_enemy_dps_map;
    double *_air_enemy_dps_map;


    // TODO move to scoutManager
    UnitToTimeMap _comsatStation;
    void scanBases();
    bool useScanner(BWAPI::Unit comsat, BWAPI::Position pos);
    std::set<BWTA::BaseLocation*>::iterator _lastBaseScan;
    std::set<BWAPI::TilePosition>::iterator _lastIgnoreBaseScan;

    // Learning
    int gamesSaved;
    size_t strategySelected;
    std::vector<int> learningData;

private:
//    log4cxx::LoggerPtr _logger;

    // Debug variables
    // ---------------
    BWAPI::Position _topLeft;
    BWAPI::Position _bottomRight;
    
    inline BWAPI::TilePosition getNaturalExpandPosition() { return getExpandPosition(false); };
    inline BWAPI::TilePosition getGasExpandPosition() { return getExpandPosition(true); };
};

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
