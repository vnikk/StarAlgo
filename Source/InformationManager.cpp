#include "stdafx.h"
#include <random>
#include "InformationManager.h"
//#include "SquadAgent.h"
//#include "SquadManager.h"

using namespace BWAPI;

InformationManager::InformationManager()
	: mapAnalyzed(false),
	_mineralsReserved(0),
	_gasReserved(0),
	_workersNeededToBuild(9),
	_maxWorkersMining(0),
	_choke(NULL),
	_minSquadSize(5),
	_proxyPosition(TilePositions::None),
	_enemyStartPosition(Positions::None),
	_useProxyPosition(false),
	_expandType(Natural),
	_turretDefenses(false),
	_scienceVesselDetector(false),
	_searchAndDestroy(false),
	_searchCorner(1),
	_searchIter(0),
	_panicMode(false),
	_autoVehicleUpgrade(false),
	_autoShipUpgrade(false),
	_autoInfanteryUpgrade(false),
	_priorCommandCenters(true),
	_retreatDisabled(false),
	_harassing(false),
	_firstPush(false),
	_initialRallyPosition(Positions::None),
	_bunkerSeedPosition(Positions::None),
	_topLeft(Positions::None),
	_bottomRight(Positions::None),
	_debugPosition1(Positions::None),
	_debugPosition2(Positions::None),
    _bbs(false),
    gamesSaved(0),
    strategySelected(0)/*,
					   TODO
	_logger(log4cxx::Logger::getLogger("InfoManager"))
	*/
{

#ifdef NOVA_GUI
	_GUIsignal = new Qsignal;
#endif

	//init train order
	_trainOrder[UnitTypes::Terran_Barracks] = UnitTypes::None;
	_trainOrder[UnitTypes::Terran_Factory]	= UnitTypes::None;
	_trainOrder[UnitTypes::Terran_Starport] = UnitTypes::None;

	//init addon order
	_addonOrder[UnitTypes::Terran_Command_Center].clear();
	_addonOrder[UnitTypes::Terran_Factory].clear();
	_addonOrder[UnitTypes::Terran_Starport].clear();
	_addonOrder[UnitTypes::Terran_Science_Facility].clear();

	//init research order
	//_researchOrder[UnitTypes::Terran_Academy];

	//init upgrade order
	//_upgradeOrder[UnitTypes::Terran_Academy];
	_upgradeOrder[UnitTypes::Terran_Engineering_Bay];

	_ourBases.clear();
	_emptyBases.clear();
	_enemyBases.clear();
	_ignoreBases.clear();
	_percentList.clear();
	_autoBuildSuplies = false;

	// Enemy DPS map:
	_last_cycle_dps_map_was_recomputed = -1;
	//_ground_enemy_dps_map = 0;
	//_air_enemy_dps_map = 0;

	int dx = BWAPI::Broodwar->mapWidth();
	int dy = BWAPI::Broodwar->mapHeight();

	_ground_enemy_dps_map = new double[dx*dy];
	_air_enemy_dps_map = new double[dx*dy];

	_regionIdMap.resize(dx, dy);
	_regionIdMap.setTo(0);

	// Init Tank siege map
	int walkMapX = dx*4;
	int walkMapY = dy*4;
	_tankSiegeMap.resize(walkMapX, walkMapY);
	_tankSiegeMap.setTo(1);

	for(int x = 0 ; x < walkMapX ; ++x) {
		//Fill from static map and Buildings
		for (int y = 0; y < walkMapY; ++y) {
			//if (!Broodwar->isBuildable(x, y)) {
			if (!Broodwar->isWalkable(x, y)) {
				_tankSiegeMap[x][y] = 0;
			}
		}
	}

	// clear the map:
	for(int i = 0;i<dx*dy;i++) _ground_enemy_dps_map[i] = _air_enemy_dps_map[i] = 0;

	// init scouting flags
	_lastBaseScan = _emptyBases.end();
	_lastIgnoreBaseScan = _ignoreBases.end();

	_wastedProductionFramesByCommandCenter = 0;
	_wastedProductionFramesByResearch = 0;
	_wastedProductionFramesByMoney = 0;
	_wastedProductionFramesBySupply = 0;
};

InformationManager::~InformationManager()
{
	if (_ground_enemy_dps_map!=0) delete[] _ground_enemy_dps_map;
	if (_air_enemy_dps_map!=0) delete[] _air_enemy_dps_map;
#ifdef NOVA_GUI
	delete _GUIsignal;
#endif
}


void InformationManager::analyzeMap()
{
	if (!mapAnalyzed) {
		//read map information into BWTA so terrain analysis can be done in another thread
		//BWTA::readMap();
		// TODO CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)AnalyzeMapThread, NULL, 0, NULL);
	}
	
}

DWORD WINAPI AnalyzeMapThread()
{
	BWTA::analyze();

	// Self start location (only available if the map has base locations)
	// ========================================================================
	BWTA::BaseLocation* myBase = BWTA::getStartLocation(Broodwar->self());
	if (myBase != nullptr) {
		TilePosition geyserLocation = TilePositions::None;
		if (!myBase->isMineralOnly()) {
			BWAPI::Unitset::iterator geyser = myBase->getGeysers().begin();
			geyserLocation = (*geyser)->getTilePosition();
		}
		informationManager->_ourBases[myBase] = geyserLocation;
		informationManager->home = myBase->getRegion();
	}

	// Enemy start location
	// ========================================================================
	informationManager->startLocationCouldContainEnemy = BWTA::getStartLocations();
	informationManager->startLocationCouldContainEnemy.erase(myBase);
	informationManager->_scoutedAnEnemyBase = false;
	if (informationManager->startLocationCouldContainEnemy.size()==1) {
        informationManager->enemyStartFound( (*informationManager->startLocationCouldContainEnemy.begin())->getPosition() );
	}

	// Empty base locations
	// ========================================================================
	informationManager->_emptyBases = BWTA::getBaseLocations();
	informationManager->_emptyBases.erase(BWTA::getStartLocation(Broodwar->self()));
	if (informationManager->startLocationCouldContainEnemy.size()==1) {
		informationManager->_emptyBases.erase(*informationManager->startLocationCouldContainEnemy.begin());
	}

	// Ignore island bases or special ones (The Fortress)
	// ========================================================================
	for(std::set<BWTA::BaseLocation*>::iterator emptyBase=informationManager->_emptyBases.begin();emptyBase!=informationManager->_emptyBases.end();) {
		if ( (*emptyBase)->isIsland() ) {
			informationManager->_ignoreBases.insert((*emptyBase)->getTilePosition());
			emptyBase = informationManager->_emptyBases.erase(emptyBase);
		} else if ( Broodwar->mapHash() == "83320e505f35c65324e93510ce2eafbaa71c9aa1" && 
			((*emptyBase)->getTilePosition() == TilePosition(7,7) || (*emptyBase)->getTilePosition() == TilePosition(7,118) ||
			 (*emptyBase)->getTilePosition() == TilePosition(117,118) || (*emptyBase)->getTilePosition() == TilePosition(117,7) ) ) {
				 informationManager->_ignoreBases.insert((*emptyBase)->getTilePosition());
				 emptyBase = informationManager->_emptyBases.erase(emptyBase);
		} else {
			++emptyBase;
		}
	}

	if (!Broodwar->isReplay()) {
		// TODO buildManager->reserveBaseLocations();

		// Last sweep scan
		// ========================================================================
		informationManager->_lastBaseScan = informationManager->_emptyBases.begin();

		// Find best bunker seed position (_bunkerSeedPosition)
		// ========================================================================
		// if no rush use home region, else use enemy's home region
		// TODO if (!informationManager->_bbs) buildManager->findBunkerSeedPosition(informationManager->home);

		// Precalculate ground distance from home to other baseLocations
		// ========================================================================
		BWTA::BaseLocation* home = BWTA::getStartLocation(Broodwar->self());
		std::set<BWTA::BaseLocation*> allBases = BWTA::getBaseLocations();
		double distance;
		allBases.erase(home);

		for (std::set<BWTA::BaseLocation*>::iterator i = allBases.begin(); i != allBases.end(); ++i) {
			// calculate distance
			distance = BWTA::getGroundDistance(TilePosition((*i)->getPosition()), TilePosition(home->getPosition()));
			// insert to map
			informationManager->_baseDistance.insert(std::make_pair(baseDistanceKey(home, *i), distance));
		}

		// Precalculate enemy init positions to to other baseLocations
		// ========================================================================
		informationManager->startLocationCouldContainEnemy;
		for (std::set<BWTA::BaseLocation*>::iterator enemyStart = informationManager->startLocationCouldContainEnemy.begin(); enemyStart != informationManager->startLocationCouldContainEnemy.end(); ++enemyStart) {
			allBases.clear();
			allBases = BWTA::getBaseLocations();
			allBases.erase(*enemyStart);
			for (std::set<BWTA::BaseLocation*>::iterator i = allBases.begin(); i != allBases.end(); ++i) {
				// calculate distance
				distance = BWTA::getGroundDistance(TilePosition((*i)->getPosition()), TilePosition((*enemyStart)->getPosition()));
				// insert to map
				informationManager->_baseDistance.insert(std::make_pair(baseDistanceKey(*enemyStart, *i), distance));
			}
		}
	}

#ifndef TOURNAMENT
    //clock_t _start = clock();
    // -- Sort regions
    const std::set<BWTA::Region*> &regions_unsort = BWTA::getRegions();
    std::set<BWTA::Region*, SortByXY> regions(regions_unsort.begin(), regions_unsort.end());


	// -- Assign region to identifiers
	int id = 0;
	for(std::set<BWTA::Region*, SortByXY>::const_iterator r = regions.begin(); r != regions.end(); ++r) {
		informationManager->_regionID[*r] = id;
		informationManager->_regionFromID[id] = *r;
		id++;
	}

	// -- Fill map to regionID variable
	for(int x=0;x<Broodwar->mapWidth();++x) {
		for(int y=0;y<Broodwar->mapHeight();++y) {
			BWTA::Region* tileRegion = BWTA::getRegion(x,y);
			if (tileRegion == NULL) tileRegion = informationManager->getNearestRegion(x,y);
			informationManager->_regionIdMap[x][y] = informationManager->_regionID[tileRegion];
		}
	}

    std::string spacePartition = LoadConfigString("high_level_search", "space_partition", "REGIONS_AND_CHOKEPOINTS");

    int distance2 = 0;
    if (spacePartition == "REGIONS_AND_CHOKEPOINTS") {
        informationManager->_onlyRegions = false;
        informationManager->_chokepointIdOffset = id;

        // -- Sort chokepoints
         const std::set<BWTA::Chokepoint*> &chokePoints_unsort = BWTA::getChokepoints();
         std::set<BWTA::Chokepoint*, SortByXY> chokePoints(chokePoints_unsort.begin(), chokePoints_unsort.end());

	    // -- Assign chokepoints to identifiers and fill map
	    for(std::set<BWTA::Chokepoint*, SortByXY>::const_iterator c = chokePoints.begin(); c != chokePoints.end(); ++c) {
		    informationManager->_chokePointID[*c] = id;
		    informationManager->_chokePointFromID[id] = *c;
            const std::pair<BWAPI::Position,BWAPI::Position> sides = (*c)->getSides();
            informationManager->drawInfluenceLine( sides.first, sides.second, id );
		    id++;
	    }

        // -- Precalculate distance between regions and chokepoints
		informationManager->_distanceBetweenRegAndChoke.resize(regions.size(), regions.size() + chokePoints.size());
        for(std::set<BWTA::Region*, SortByXY>::const_iterator j = regions.begin(); j != regions.end(); ++j) {
            const std::set<BWTA::Chokepoint*> chokes = (*j)->getChokepoints();
            Position pos1 = (*j)->getCenter();
            for (std::set<BWTA::Chokepoint*>::const_iterator k = chokes.begin(); k != chokes.end(); ++k) {
                Position pos2 = (*k)->getCenter();
                // sometimes the center of the region is in an unwalkable area
                if ( Broodwar->isBuildable(BWAPI::TilePosition(pos1)) ) { 
                    distance2 = (int)BWTA::getGroundDistance(BWAPI::TilePosition(pos1), BWAPI::TilePosition(pos2));
                } else { // TODO improve this to get a proper position and compute groundDistance
                    distance2 = (int)pos1.getDistance(pos2);
                }
                int id1 = informationManager->_regionID[*j];
                int id2 = informationManager->_chokePointID[*k];
				informationManager->_distanceBetweenRegAndChoke[id1][id2] = distance2;
//                 LOG("Storing ["<<id1<<"]["<<id2<<"] = "<<distance2);
            }
        }
    } else if(spacePartition == "REGIONS") {
        informationManager->_onlyRegions = true;
        // -- Precalculate distance between regions
        informationManager->_distanceBetweenRegions.resize(regions.size(), regions.size());
		for (const auto& r1 : regions) {
			for (const auto& r2 : regions) {
				Position pos1(r1->getCenter());
				Position pos2(r2->getCenter());
				int id1 = informationManager->_regionID[r1];
				int id2 = informationManager->_regionID[r2];
				// sometimes the center of the region is in an unwalkable area
				if (Broodwar->isWalkable(BWAPI::WalkPosition(pos1)) &&
					Broodwar->isWalkable(BWAPI::WalkPosition(pos2))) {
					distance2 = (int)BWTA::getGroundDistance(BWAPI::TilePosition(pos1), BWAPI::TilePosition(pos2));
				} else { // TODO improve this to get a proper position and compute groundDistance
// 					if (!Broodwar->isWalkable(BWAPI::WalkPosition(pos1))) {
// 						LOG("Center of Region " << id1 << " isn't walkable");
// 					}
// 					if (!Broodwar->isWalkable(BWAPI::WalkPosition(pos2))) {
// 						LOG("Center of Region " << id2 << " isn't walkable");
// 					}
					distance2 = (int)pos1.getDistance(pos2);
				}
// 				LOG("Storing ["<<id1<<"]["<<id2<<"] = " << distance2);
				informationManager->_distanceBetweenRegions[id1][id2] = distance2;
			}
		}
    } else {
        LOG("Error, unknown space_partition in Nova.ini");
    }
#endif
 
    //informationManager->spacePartitionStats();

//     double regionTime = double(clock()-_start)/CLOCKS_PER_SEC;
//     LOG("Time to process regions: " << regionTime);

    //LOG("Map analysis finished");
	informationManager->mapAnalyzed = true;

#ifdef NOVA_GUI
	informationManager->_GUIsignal->emitMapInfoChanged();
#endif
	return 0;
}

void InformationManager::spacePartitionStats()
{
    LOG("===== SPACE PARTITION STATS =====");
    //--------------------------- 
    // number of nodes
    //--------------------------- 
    int Rsize = BWTA::getRegions().size() + BWTA::getChokepoints().size();
    int RCsize = Rsize + BWTA::getChokepoints().size();

    //--------------------------- 
    //branching (max child)
    //--------------------------- 
    int totalBranching = BWTA::getChokepoints().size()*2;
    float RavgBranch = (float)totalBranching / (float)Rsize;
    float RCavgBranch = (float)(totalBranching*2) / (float)RCsize;

    //--------------------------- 
    // depth (max graph distance)
    //--------------------------- 
    // BFS at random region (with chokepoints, i.e. not an island) 
    std::set<BWTA::Region*>::const_iterator randomRegion = BWTA::getRegions().begin();
    while( (*randomRegion)->getChokepoints().size() == 0 ) {
		std::uniform_int_distribution<int> uniformDist(0, Rsize - 1);
		int steps = uniformDist(gen);
        randomRegion = BWTA::getRegions().begin();
        std::advance(randomRegion,steps);
    }
    BWTA::Region* fardestRegion = *randomRegion;
    
    int fardestDepth = 0;
    // node, depth queue
    std::vector< std::pair< BWTA::Region*, int> > queue;
    std::vector<BWTA::Region*> visited;
    //LOG("visit node: " << informationManager->_regionID[fardestRegion] << " depth: " << fardestDepth);
    queue.push_back(std::make_pair(fardestRegion, fardestDepth));
    visited.push_back(fardestRegion);
    int actualDepth = 0;
    while (!queue.empty()) {
        // pop node
        std::pair<BWTA::Region*, int> v = queue.back();
        queue.pop_back();
        actualDepth = v.second + 1;
 
        // explore all neighbors
        const std::set<BWTA::Chokepoint*> chokes = v.first->getChokepoints();
        for (std::set<BWTA::Chokepoint*>::const_iterator k = chokes.begin(); k != chokes.end(); ++k) {
            // check if bot regions are visited
            const std::pair<BWTA::Region*,BWTA::Region*> regions = (*k)->getRegions();
            if (std::find(visited.begin(), visited.end(), regions.first)==visited.end()) {
                //LOG("visit node: " << informationManager->_regionID[regions.first] << " depth: " << actualDepth);
                queue.push_back(std::make_pair(regions.first,actualDepth));
                visited.push_back(regions.first);
                if (actualDepth > fardestDepth) {
                    fardestDepth = actualDepth;
                    fardestRegion = regions.first;
                }
            }
            if (std::find(visited.begin(), visited.end(), regions.second)==visited.end()) {
                //LOG("visit node: " << informationManager->_regionID[regions.second] << " depth: " << actualDepth);
                queue.push_back(std::make_pair(regions.second,actualDepth));
                visited.push_back(regions.second);
                if (actualDepth > fardestDepth) {
                    fardestDepth = actualDepth;
                    fardestRegion = regions.second;
                }
            }
        }
    }
    // BFS from farthest node
    //LOG("Farthest RegionID: " << informationManager->_regionID[fardestRegion]);
    fardestDepth = 0;
    visited.clear();
    queue.push_back(std::make_pair(fardestRegion, fardestDepth));
    visited.push_back(fardestRegion);
    actualDepth = 0;
    while (!queue.empty()) {
        // pop node
        std::pair<BWTA::Region*, int> v = queue.back();
        queue.pop_back();
        actualDepth = v.second + 1;
 
        // explore all neighbors
        const std::set<BWTA::Chokepoint*> chokes = v.first->getChokepoints();
        for (std::set<BWTA::Chokepoint*>::const_iterator k = chokes.begin(); k != chokes.end(); ++k) {
            // check if bot regions are visited
            const std::pair<BWTA::Region*,BWTA::Region*> regions = (*k)->getRegions();
            if (std::find(visited.begin(), visited.end(), regions.first)==visited.end()) {
                queue.push_back(std::make_pair(regions.first,actualDepth));
                visited.push_back(regions.first);
                if (actualDepth > fardestDepth) {
                    fardestDepth = actualDepth;
                    fardestRegion = regions.first;
                }
            }
            if (std::find(visited.begin(), visited.end(), regions.second)==visited.end()) {
                queue.push_back(std::make_pair(regions.second,actualDepth));
                visited.push_back(regions.second);
                if (actualDepth > fardestDepth) {
                    fardestDepth = actualDepth;
                    fardestRegion = regions.second;
                }
            }
        }
    }
    LOG("[RC] size: " << RCsize << " avgBranching: " << RCavgBranch << "  depth: " << fardestDepth);
    LOG("[R] size: " << Rsize << " avgBranching: " << RavgBranch << "  depth: " << fardestDepth*2);
}

void InformationManager::drawInfluenceLine( Position pos1, Position pos2, const int id )
{
    const int CHOKEPOINT_INFLUENCE_DISTANCE = 4;
    float x1, y1, x2, y2;
    x1 = (float)pos1.x/TILE_SIZE;
    y1 = (float)pos1.y/TILE_SIZE;
    x2 = (float)pos2.x/TILE_SIZE;
    y2 = (float)pos2.y/TILE_SIZE;
    // Bresenham's line algorithm
    const bool steep = (fabs(y2 - y1) > fabs(x2 - x1));
    if(steep) {
        std::swap(x1, y1);
        std::swap(x2, y2);
    }

    if(x1 > x2) {
        std::swap(x1, x2);
        std::swap(y1, y2);
    }

    const float dx = x2 - x1;
    const float dy = fabs(y2 - y1);

    float error = dx / 2.0f;
    const int ystep = (y1 < y2) ? 1 : -1;
    int y = (int)y1;

    const int maxX = (int)x2;

    for(int x=(int)x1; x<maxX; x++) {
        if(steep) {
            informationManager->_regionIdMap.setRectangleTo(y-CHOKEPOINT_INFLUENCE_DISTANCE, x-CHOKEPOINT_INFLUENCE_DISTANCE, 
                                                            y+CHOKEPOINT_INFLUENCE_DISTANCE, x+CHOKEPOINT_INFLUENCE_DISTANCE, id);
        } else {
            informationManager->_regionIdMap.setRectangleTo(x-CHOKEPOINT_INFLUENCE_DISTANCE, y-CHOKEPOINT_INFLUENCE_DISTANCE, 
                                                            x+CHOKEPOINT_INFLUENCE_DISTANCE, y+CHOKEPOINT_INFLUENCE_DISTANCE, id);
        }

        error -= dy;
        if(error < 0) {
            y += ystep;
            error += dx;
        }
    }
}

// TODO add to BWTA
BWTA::Region* InformationManager::getNearestRegion(int x, int y)
{
	//searches outward in a spiral.
	int length = 1;
	int j      = 0;
	bool first = true;
	int dx     = 0;
	int dy     = 1;
	BWTA::Region* tileRegion = NULL;
	while (length < Broodwar->mapWidth()) //We'll ride the spiral to the end
	{
		//if is a valid regions, return it
		tileRegion = BWTA::getRegion(x,y);
		if (tileRegion != NULL) return tileRegion;

		//otherwise, move to another position
		x = x + dx;
		y = y + dy;
		//count how many steps we take in this direction
		j++;
		if (j == length) { //if we've reached the end, its time to turn
			j = 0;	//reset step counter

			//Spiral out. Keep going.
			if (!first)
				length++; //increment step counter if needed


			first =! first; //first=true for every other turn so we spiral out at the right rate

			//turn counter clockwise 90 degrees:
			if (dx == 0) {
				dx = dy;
				dy = 0;
			} else {
				dy = -dx;
				dx = 0;
			}
		}
		//Spiral out. Keep going.
	}
	return tileRegion;
}

void InformationManager::deleteEnemyStartLocation(Position _startPosition)
{
	for(std::set<BWTA::BaseLocation*>::iterator enemyStart=startLocationCouldContainEnemy.begin();enemyStart!=startLocationCouldContainEnemy.end();++enemyStart) {
		if ( (*enemyStart)->getPosition() == _startPosition) {
			startLocationCouldContainEnemy.erase(enemyStart);
			break;
		}
	}
	if (startLocationCouldContainEnemy.size()==1) {
		BWTA::BaseLocation* startBase = *startLocationCouldContainEnemy.begin();
		_enemyBases.insert(startBase);
		_scoutedAnEnemyBase = true;
		_enemyStartBase = startBase;
		_enemyStartPosition = startBase->getPosition();
	}
}

void InformationManager::enemyStartFound(Position _startPosition)
{
// 	LOG("enemyStartFound(" << _startPosition.x << "," << _startPosition.y << ")");
	_scoutedAnEnemyBase = true;
	_enemyStartPosition = _startPosition;

	for (const auto & enemyStart : startLocationCouldContainEnemy) {
		// TODO if it is not the EXACT position this will break
		if (enemyStart->getPosition() == _startPosition) {
			_enemyBases.insert(enemyStart);
			_enemyStartBase = enemyStart;
			startLocationCouldContainEnemy.clear();
			break;
		}
	}
}

void InformationManager::naturalExpandRequest(bool critical)
{
	_expandType = InformationManager::Natural;
	if (critical) {
		criticalBuildRequest(UnitTypes::Terran_Command_Center);
	} else {
		buildRequest(UnitTypes::Terran_Command_Center);
	}
}

void InformationManager::gasExpandRequest(bool critical)
{
	_expandType = InformationManager::Gas;
	if (critical) {
		criticalBuildRequest(UnitTypes::Terran_Command_Center);
	} else {
		buildRequest(UnitTypes::Terran_Command_Center);
	}
}

void InformationManager::onCommandCenterShow(Unit unit)
{
	if (informationManager->_ourBases.empty()) return; //skip initial instance
    if (informationManager->_bbs) return; // skip if we are doing BBS
    //LOG("New " << unit->getType().getName() << " at (" << unit->getPosition().x << "," << unit->getPosition().y << ")");

	for(std::set<BWTA::BaseLocation*>::iterator emptyBase=_emptyBases.begin();emptyBase!=_emptyBases.end();++emptyBase) {
		TilePosition baseLocation = (*emptyBase)->getTilePosition();
		if (baseLocation == unit->getTilePosition()) {
			TilePosition geyserLocation = TilePositions::None;
			if ( !(*emptyBase)->isMineralOnly() ) {
				BWAPI::Unitset::iterator geyser = (*emptyBase)->getGeysers().begin();
				geyserLocation = (*geyser)->getTilePosition();
				criticalBuildRequest(UnitTypes::Terran_Refinery);
			}
			informationManager->_ourBases[*emptyBase] = geyserLocation;
			informationManager->_emptyBases.erase(emptyBase);
			informationManager->_lastBaseScan = informationManager->_emptyBases.begin();
			break;
		}
	}

	// update Proxy position (base closest to the enemy)
	int distance = 9999999;
	int newDistance;
	TilePosition bestLocation;
	for(std::map<BWTA::BaseLocation*, BWAPI::TilePosition>::iterator ourBase=_ourBases.begin();ourBase!=_ourBases.end();++ourBase) {
		newDistance = ourBase->first->getPosition().getApproxDistance(informationManager->_enemyStartPosition);
		if (newDistance < distance) {
			distance = newDistance;
			bestLocation = ourBase->first->getTilePosition();
		}
	}
	_proxyPosition = bestLocation;
    //LOG("Proxy updated at (" << informationManager->_proxyPosition.x << "," << informationManager->_proxyPosition.y << ")");
	_useProxyPosition = true;
}

void InformationManager::onEnemyResourceDepotShow(Unit unit)
{
	for(std::set<BWTA::BaseLocation*>::iterator emptyBase=_emptyBases.begin();emptyBase!=_emptyBases.end();++emptyBase) {
		TilePosition baseLocation = (*emptyBase)->getTilePosition();
		if (baseLocation == unit->getTilePosition()) {
			informationManager->_enemyBases.insert(*emptyBase);
			informationManager->_emptyBases.erase(emptyBase);
			informationManager->_lastBaseScan = informationManager->_emptyBases.begin();
			return;
		}
	}
}

void InformationManager::onCommandCenterDestroy(Unit unit)
{
	for(std::map<BWTA::BaseLocation*, BWAPI::TilePosition>::const_iterator ourBase=_ourBases.begin();ourBase!=_ourBases.end();++ourBase) {
		TilePosition baseLocation = ourBase->first->getTilePosition();
		if (baseLocation == unit->getTilePosition()) {
//TODO e			LOG4CXX_TRACE(_logger, "Remove minerals spots from base (" << ourBase->first << ")");
			workerManager->onBaseDestroy(ourBase->first);
			informationManager->_emptyBases.insert(ourBase->first);
			informationManager->_lastBaseScan = informationManager->_emptyBases.begin();
			informationManager->_ourBases.erase(ourBase); // warning: last thing to do because we will mess the iterator!
			return;
		}
	}
}

void InformationManager::onEnemyResourceDepotDestroy(Unit unit)
{
	for(std::set<BWTA::BaseLocation*>::iterator enemyBase=_enemyBases.begin();enemyBase!=_enemyBases.end();++enemyBase) {
		TilePosition baseLocation = (*enemyBase)->getTilePosition();
		if (baseLocation == unit->getTilePosition()) {
			informationManager->_emptyBases.insert(*enemyBase);
			informationManager->_enemyBases.erase(enemyBase);
			informationManager->_lastBaseScan = informationManager->_emptyBases.begin();
			return;
		}
	}
}

TilePosition InformationManager::getExpandPosition()
{
	if (_expandType == InformationManager::Natural) {
		return informationManager->getNaturalExpandPosition();
	} else {
		return informationManager->getGasExpandPosition();
	}
}

TilePosition InformationManager::getExpandPosition(bool gasRequired)
{
	// TODO improve performance of this (BWTA 2)
	int minDistance = 9999999;
	TilePosition bestTilePosition = TilePositions::None;
	Position baseLocation = BWTA::getStartLocation(Broodwar->self())->getPosition();

	for (const auto& emptyBase : _emptyBases) {
		if (gasRequired && emptyBase->isMineralOnly()) continue;
		if (_ignoreBases.find(emptyBase->getTilePosition()) != _ignoreBases.end()) continue;

		// Consider just home base
		if (Broodwar->hasPath(emptyBase->getPosition(), baseLocation)) {
			// we want a base closer to our main
			int distance = (int)informationManager->_baseDistance[baseDistanceKey(BWTA::getStartLocation(Broodwar->self()), emptyBase)];
			// but far from the enemy
			distance -= (int)informationManager->_baseDistance[baseDistanceKey(informationManager->_enemyStartBase, emptyBase)];
			if (distance < minDistance) {
				minDistance = distance;
				bestTilePosition = emptyBase->getTilePosition();
			}
		}
	}

	return bestTilePosition;
}

bool InformationManager::existExpand(bool gasRequired)
{
	Position baseLocation = BWTA::getStartLocation(Broodwar->self())->getPosition();
	for(std::set<BWTA::BaseLocation*>::iterator emptyBase=_emptyBases.begin();emptyBase!=_emptyBases.end();++emptyBase) {
		if ( _ignoreBases.find( (*emptyBase)->getTilePosition() ) != _ignoreBases.end() ) continue;
		if (gasRequired && (*emptyBase)->isMineralOnly()) continue;
		if (!Broodwar->hasPath((*emptyBase)->getPosition(), baseLocation)) continue;
		return true;
	}
	return false;
}

void InformationManager::reserveMinerals(int minerals) {
	LOG4CXX_TRACE(_logger, "Reserving mienrals: " << minerals);
	_mineralsReserved += minerals;
}

void InformationManager::removeReservedMinerals(int minerals) {
	LOG4CXX_TRACE(_logger, "Removing mienrals: " << minerals);
	_mineralsReserved -= minerals;
	if (_mineralsReserved < 0) {
		LOG4CXX_ERROR(_logger, "Removing more minerals than we have: " << _mineralsReserved);
		_mineralsReserved = 0;
	}
}

void InformationManager::reserveGas(int gas) {
	LOG4CXX_TRACE(_logger, "Reserving gas: " << gas);
	_gasReserved += gas;
}

void InformationManager::removeReservedGas(int gas) {
	LOG4CXX_TRACE(_logger, "Removing gas: " << gas);
	_gasReserved -= gas;
	if (_gasReserved < 0) {
		LOG4CXX_ERROR(_logger, "Removing more gas than we have: " << _gasReserved);
		_gasReserved = 0;
	}
}

int InformationManager::minerals() {
	return Broodwar->self()->minerals() - _mineralsReserved - _frameMineralSpend;
}

int InformationManager::gas() {
	return Broodwar->self()->gas() - _gasReserved - _frameGasSpend;
}

bool InformationManager::haveResources(UnitType type) {
	return ( minerals() >= type.mineralPrice() && gas() >= type.gasPrice() );
}
bool InformationManager::haveResources(TechType type) {
	return ( minerals() >= type.mineralPrice() && gas() >= type.gasPrice() );
}
bool InformationManager::haveResources(UpgradeType type) {
	return ( minerals() >= type.mineralPrice(Broodwar->self()->getUpgradeLevel(type)+1) && gas() >= type.gasPrice(Broodwar->self()->getUpgradeLevel(type)+1) );
}

void InformationManager::frameSpend(UnitType type) {
	_frameMineralSpend += type.mineralPrice();
	_frameGasSpend += type.gasPrice();
}
void InformationManager::frameSpend(TechType type) {
	_frameMineralSpend += type.mineralPrice();
	_frameGasSpend += type.gasPrice();
}
void InformationManager::frameSpend(UpgradeType type) {
	_frameMineralSpend += type.mineralPrice();
	_frameGasSpend += type.gasPrice();
}

void InformationManager::buildRequest(UnitType type) {
	buildRequest(type, false);
}

void InformationManager::buildRequest(UnitType type, bool checkUnic)
{
	if (checkUnic) {
		if ( Broodwar->self()->completedUnitCount(type) > 0 || // if we already build it
			 std::find(_buildRequest.begin(), _buildRequest.end(), type) != _buildRequest.end() || // if we didn't request before
			 buildManager->alreadyBuilding(type) ) { // if we aren't building it
			return;	 
		}
	}
	
	if (type == UnitTypes::Terran_Comsat_Station) {
		buildRequest(UnitTypes::Terran_Barracks, true);
		buildRequest(UnitTypes::Terran_Refinery, true);
		buildRequest(UnitTypes::Terran_Academy, true);
	} else if (type == UnitTypes::Terran_Nuclear_Silo) {
		buildRequest(UnitTypes::Terran_Barracks, true);
		buildRequest(UnitTypes::Terran_Refinery, true);
		buildRequest(UnitTypes::Terran_Factory, true);
		buildRequest(UnitTypes::Terran_Starport, true);
		buildRequest(UnitTypes::Terran_Science_Facility, true);
		buildRequest(UnitTypes::Terran_Covert_Ops, true);
	} else if (type == UnitTypes::Terran_Science_Facility) {
		buildRequest(UnitTypes::Terran_Barracks, true);
		buildRequest(UnitTypes::Terran_Refinery, true);
		buildRequest(UnitTypes::Terran_Factory, true);
		buildRequest(UnitTypes::Terran_Starport, true);
	}

	_buildRequest.push_back(type);
}

void InformationManager::criticalBuildRequest(UnitType type) {
	criticalBuildRequest(type, false);
}

void InformationManager::criticalBuildRequest(UnitType type, bool first)
{
	if ( std::find(_criticalBuildRequest.begin(), _criticalBuildRequest.end(), type) == _criticalBuildRequest.end() ) { // if we didn't request before
		if (first && Broodwar->self()->completedUnitCount(type) > 0) return; // if we already build it
		if ( Broodwar->self()->incompleteUnitCount(type) == 0 ) // if we aren't building it
			if (type == UnitTypes::Terran_Comsat_Station) {
				//Broodwar->printf("URGENTE COMSAT STATION ################################################");
				criticalBuildRequest(UnitTypes::Terran_Academy, true);
				criticalBuildRequest(UnitTypes::Terran_Refinery, true);
				criticalBuildRequest(UnitTypes::Terran_Barracks, true);
				
			}
			_criticalBuildRequest.push_back(type);
	}
}

void InformationManager::addonRequest(UnitType addonType) {
	_addonOrder[addonType.whatBuilds().first].push_back(addonType);
}

void InformationManager::researchRequest(TechType techType)
{
	if ( std::find(_researchOrder[techType.whatResearches()].begin(), _researchOrder[techType.whatResearches()].end(), techType) == _researchOrder[techType.whatResearches()].end() ) { // if we didn't request before
		if (!Broodwar->self()->hasResearched(techType) && !Broodwar->self()->isResearching(techType)) {
			_researchOrder[techType.whatResearches()].push_back(techType);
			// check dependencies
			if (techType == TechTypes::Stim_Packs || techType == TechTypes::Restoration || techType == TechTypes::Optical_Flare) {
				buildRequest(UnitTypes::Terran_Barracks, true);
				buildRequest(UnitTypes::Terran_Refinery, true);
				buildRequest(UnitTypes::Terran_Academy, true);
			} else if (techType == TechTypes::Spider_Mines || techType == TechTypes::Tank_Siege_Mode) {
				// Machine Shop
				buildRequest(UnitTypes::Terran_Barracks, true);
				buildRequest(UnitTypes::Terran_Refinery, true);
				buildRequest(UnitTypes::Terran_Factory, true);
				buildRequest(UnitTypes::Terran_Machine_Shop, true);
			} else if (techType == TechTypes::Cloaking_Field) {
				// Control Tower
				buildRequest(UnitTypes::Terran_Barracks, true);
				buildRequest(UnitTypes::Terran_Refinery, true);
				buildRequest(UnitTypes::Terran_Factory, true);
				buildRequest(UnitTypes::Terran_Starport, true);
				buildRequest(UnitTypes::Terran_Control_Tower, true);
			} else if (techType == TechTypes::EMP_Shockwave || techType == TechTypes::Irradiate) {
				// Science Facility
				buildRequest(UnitTypes::Terran_Barracks, true);
				buildRequest(UnitTypes::Terran_Refinery, true);
				buildRequest(UnitTypes::Terran_Factory, true);
				buildRequest(UnitTypes::Terran_Starport, true);
				buildRequest(UnitTypes::Terran_Science_Facility, true);
			} else if (techType == TechTypes::Yamato_Gun) {
				// Physics Lab
				buildRequest(UnitTypes::Terran_Barracks, true);
				buildRequest(UnitTypes::Terran_Refinery, true);
				buildRequest(UnitTypes::Terran_Factory, true);
				buildRequest(UnitTypes::Terran_Starport, true);
				buildRequest(UnitTypes::Terran_Science_Facility, true);
				buildRequest(UnitTypes::Terran_Physics_Lab, true);
			} else if (techType == TechTypes::Lockdown || techType == TechTypes::Personnel_Cloaking) {
				// CovertOps
				buildRequest(UnitTypes::Terran_Barracks, true);
				buildRequest(UnitTypes::Terran_Refinery, true);
				buildRequest(UnitTypes::Terran_Factory, true);
				buildRequest(UnitTypes::Terran_Starport, true);
				buildRequest(UnitTypes::Terran_Science_Facility, true);
				buildRequest(UnitTypes::Terran_Covert_Ops, true);
			}
		}
	}
}
void InformationManager::upgradeRequest(UpgradeType upgradeType) {
	upgradeRequest(upgradeType, 1);
}

void InformationManager::upgradeRequest(UpgradeType upgradeType, int level)
{
	if ( std::find(_upgradeOrder[upgradeType.whatUpgrades()].begin(), _upgradeOrder[upgradeType.whatUpgrades()].end(), upgradeType) == _upgradeOrder[upgradeType.whatUpgrades()].end() ) { // if we didn't request before
		int actualLevel = Broodwar->self()->getUpgradeLevel(upgradeType);
		if (Broodwar->self()->isUpgrading(upgradeType)) actualLevel++;
		if (actualLevel < level) {
			if ( upgradeType != UpgradeTypes::Terran_Vehicle_Weapons && upgradeType != UpgradeTypes::Terran_Vehicle_Plating &&
				upgradeType != UpgradeTypes::Terran_Ship_Weapons && upgradeType != UpgradeTypes::Terran_Ship_Plating &&
				upgradeType != UpgradeTypes::Terran_Infantry_Weapons && upgradeType != UpgradeTypes::Terran_Infantry_Armor ) {
				for (int i = actualLevel; i < level; ++i) {
					_upgradeOrder[upgradeType.whatUpgrades()].push_back(upgradeType);
				}
			}
			// check dependencies
			if (upgradeType == UpgradeTypes::U_238_Shells || upgradeType == UpgradeTypes::Caduceus_Reactor) {
				buildRequest(UnitTypes::Terran_Barracks, true);
				buildRequest(UnitTypes::Terran_Refinery, true);
				buildRequest(UnitTypes::Terran_Academy, true);
			} else if (upgradeType == UpgradeTypes::Terran_Infantry_Weapons || upgradeType == UpgradeTypes::Terran_Infantry_Armor) {
				buildRequest(UnitTypes::Terran_Engineering_Bay, true);
				if (level > 1) {
					buildRequest(UnitTypes::Terran_Engineering_Bay, true);
					buildRequest(UnitTypes::Terran_Barracks, true);
					buildRequest(UnitTypes::Terran_Refinery, true);
					buildRequest(UnitTypes::Terran_Factory, true);
					buildRequest(UnitTypes::Terran_Starport, true);
					buildRequest(UnitTypes::Terran_Science_Facility, true);
				}
			} else if (upgradeType == UpgradeTypes::Ion_Thrusters || upgradeType == UpgradeTypes::Charon_Boosters) {
				//Machine Shop
				buildRequest(UnitTypes::Terran_Barracks, true);
				buildRequest(UnitTypes::Terran_Refinery, true);
				buildRequest(UnitTypes::Terran_Factory, true);
				buildRequest(UnitTypes::Terran_Machine_Shop, true);
				if (upgradeType == UpgradeTypes::Charon_Boosters)
					buildRequest(UnitTypes::Terran_Armory, true);
			} else if ( upgradeType == UpgradeTypes::Terran_Vehicle_Weapons || upgradeType == UpgradeTypes::Terran_Vehicle_Plating ||
						upgradeType == UpgradeTypes::Terran_Ship_Weapons || upgradeType == UpgradeTypes::Terran_Ship_Plating) {
				//Armory
				buildRequest(UnitTypes::Terran_Barracks, true);
				buildRequest(UnitTypes::Terran_Refinery, true);
				buildRequest(UnitTypes::Terran_Factory, true);
				buildRequest(UnitTypes::Terran_Armory, true);
				if (level > 1) {
					buildRequest(UnitTypes::Terran_Starport, true);
					buildRequest(UnitTypes::Terran_Science_Facility, true);
				}
			} else if (upgradeType == UpgradeTypes::Apollo_Reactor) {
				//Control Tower
				buildRequest(UnitTypes::Terran_Barracks, true);
				buildRequest(UnitTypes::Terran_Refinery, true);
				buildRequest(UnitTypes::Terran_Factory, true);
				buildRequest(UnitTypes::Terran_Starport, true);
				buildRequest(UnitTypes::Terran_Control_Tower, true);
			} else if (upgradeType == UpgradeTypes::Titan_Reactor) {
				//Science Facility
				buildRequest(UnitTypes::Terran_Barracks, true);
				buildRequest(UnitTypes::Terran_Refinery, true);
				buildRequest(UnitTypes::Terran_Factory, true);
				buildRequest(UnitTypes::Terran_Starport, true);
				buildRequest(UnitTypes::Terran_Science_Facility, true);
			} else if (upgradeType == UpgradeTypes::Colossus_Reactor) {
				//Physics Lab
				buildRequest(UnitTypes::Terran_Barracks, true);
				buildRequest(UnitTypes::Terran_Refinery, true);
				buildRequest(UnitTypes::Terran_Factory, true);
				buildRequest(UnitTypes::Terran_Starport, true);
				buildRequest(UnitTypes::Terran_Science_Facility, true);
				buildRequest(UnitTypes::Terran_Physics_Lab, true);
			} else if (upgradeType == UpgradeTypes::Ocular_Implants || upgradeType == UpgradeTypes::Moebius_Reactor) {
				//Covert Ops
				buildRequest(UnitTypes::Terran_Barracks, true);
				buildRequest(UnitTypes::Terran_Refinery, true);
				buildRequest(UnitTypes::Terran_Factory, true);
				buildRequest(UnitTypes::Terran_Starport, true);
				buildRequest(UnitTypes::Terran_Science_Facility, true);
				buildRequest(UnitTypes::Terran_Covert_Ops, true);
			}
		}
	}
}

void InformationManager::checkRequirements(UnitType type)
{
	buildRequest(UnitTypes::Terran_Barracks, true);
	if (type == UnitTypes::Terran_Medic || type == UnitTypes::Terran_Firebat) {
		buildRequest(UnitTypes::Terran_Refinery, true);
		buildRequest(UnitTypes::Terran_Academy, true);
	} else if (type == UnitTypes::Terran_Vulture) {
		buildRequest(UnitTypes::Terran_Refinery, true);
		buildRequest(UnitTypes::Terran_Factory, true);
	}  else if (type == UnitTypes::Terran_Goliath) {
		buildRequest(UnitTypes::Terran_Refinery, true);
		buildRequest(UnitTypes::Terran_Factory, true);
		buildRequest(UnitTypes::Terran_Armory, true);
	} else if (type == UnitTypes::Terran_Siege_Tank_Tank_Mode) {
		buildRequest(UnitTypes::Terran_Refinery, true);
		buildRequest(UnitTypes::Terran_Factory, true);
	} else if (type == UnitTypes::Terran_Wraith) {
		buildRequest(UnitTypes::Terran_Refinery, true);
		buildRequest(UnitTypes::Terran_Factory, true);
		buildRequest(UnitTypes::Terran_Starport, true);
	} else if (type == UnitTypes::Terran_Dropship) {
		buildRequest(UnitTypes::Terran_Refinery, true);
		buildRequest(UnitTypes::Terran_Factory, true);
		buildRequest(UnitTypes::Terran_Starport, true);
		//informationManager->buildRequest(UnitTypes::Terran_Control_Tower, true);
	} else if (type == UnitTypes::Terran_Science_Vessel) {
		buildRequest(UnitTypes::Terran_Refinery, true);
		buildRequest(UnitTypes::Terran_Factory, true);
		buildRequest(UnitTypes::Terran_Starport, true);
		//informationManager->buildRequest(UnitTypes::Terran_Control_Tower, true);
		buildRequest(UnitTypes::Terran_Science_Facility, true);
	} else if (type == UnitTypes::Terran_Ghost) {
		buildRequest(UnitTypes::Terran_Refinery, true);
		buildRequest(UnitTypes::Terran_Academy, true);
		buildRequest(UnitTypes::Terran_Factory, true);
		buildRequest(UnitTypes::Terran_Starport, true);
		buildRequest(UnitTypes::Terran_Science_Facility, true);
		buildRequest(UnitTypes::Terran_Covert_Ops, true);
	} else if (type == UnitTypes::Terran_Battlecruiser) {
		buildRequest(UnitTypes::Terran_Refinery, true);
		buildRequest(UnitTypes::Terran_Factory, true);
		buildRequest(UnitTypes::Terran_Starport, true);
		//informationManager->buildRequest(UnitTypes::Terran_Control_Tower, true);
		buildRequest(UnitTypes::Terran_Science_Facility, true);
		buildRequest(UnitTypes::Terran_Physics_Lab, true);
	}
}

void InformationManager::seenCloakedEnemy(TilePosition pos)
{
	if ( std::find(_cloakedEnemyPositions.begin(), _cloakedEnemyPositions.end(), pos) == _cloakedEnemyPositions.end() ) { // if we didn't request before
		_cloakedEnemyPositions.insert(pos);
	}
}


double InformationManager::get_enemy_ground_dps(BWAPI::TilePosition pos, int cycle)
{
	int dx = BWAPI::Broodwar->mapWidth();

	if (cycle>_last_cycle_dps_map_was_recomputed) recompute_enemy_dps(cycle);
	return _ground_enemy_dps_map[pos.x + pos.y*dx];
}


double InformationManager::get_enemy_air_dps(BWAPI::TilePosition pos, int cycle)
{
	int dx = BWAPI::Broodwar->mapWidth();
	if (cycle>_last_cycle_dps_map_was_recomputed) recompute_enemy_dps(cycle);
	return _air_enemy_dps_map[pos.x + pos.y*dx];
}


double InformationManager::get_enemy_ground_dps(int x, int y, int cycle)
{
	int dx = BWAPI::Broodwar->mapWidth();
	//DEBUG("[TEST] Cycle " << cycle << " Last cycle " << _last_cycle_dps_map_was_recomputed);
	if (cycle>_last_cycle_dps_map_was_recomputed) recompute_enemy_dps(cycle);
	return _ground_enemy_dps_map[x + y*dx];
}


double InformationManager::get_enemy_air_dps(int x, int y, int cycle)
{
	int dx = BWAPI::Broodwar->mapWidth();
	if (cycle>_last_cycle_dps_map_was_recomputed) recompute_enemy_dps(cycle);
	return _air_enemy_dps_map[x + y*dx];
}


void InformationManager::recompute_enemy_dps(int cycle)
{
	int dx = BWAPI::Broodwar->mapWidth();
	int dy = BWAPI::Broodwar->mapHeight();

	if (_ground_enemy_dps_map==0) _ground_enemy_dps_map = new double[dx*dy];
	if (_air_enemy_dps_map==0) _air_enemy_dps_map = new double[dx*dy];

	// clear the map:
	for(int i = 0;i<dx*dy;i++) _ground_enemy_dps_map[i] = _air_enemy_dps_map[i] = 0;

	UnitToCache allEnemies = visibleEnemies;
	allEnemies.insert(seenEnemies.begin(), seenEnemies.end());

	for (auto unitCache : allEnemies) {
		BWAPI::TilePosition pos(unitCache.second.position);
		BWAPI::UnitType unit_t(unitCache.second.type);
		double air_dps = 0;
		double ground_dps = 0;
		if (unit_t == UnitTypes::Terran_Firebat ||
			unit_t == UnitTypes::Protoss_Zealot) {
			ground_dps = unit_t.groundWeapon().damageAmount()*2*(24.0/unit_t.groundWeapon().damageCooldown());
		} else {
			ground_dps = unit_t.groundWeapon().damageAmount()*(24.0/unit_t.groundWeapon().damageCooldown());
		}
		air_dps = unit_t.airWeapon().damageAmount()*(24.0/unit_t.airWeapon().damageCooldown());

		//int gmax_range = unit_t.groundWeapon().maxRange()/32;
		int gmax_range = Broodwar->enemy()->weaponMaxRange(unit_t.groundWeapon()) / 32 + 3;
		// simulate potential fields
		if (unit_t == UnitTypes::Protoss_Zealot || unit_t == UnitTypes::Zerg_Zergling) gmax_range = 5;
		int gmin_range = unit_t.groundWeapon().minRange()/32;
		//int amax_range = unit_t.airWeapon().maxRange()/32;
		int amax_range = Broodwar->enemy()->weaponMaxRange(unit_t.airWeapon()) / 32 + 3;
		int amin_range = unit_t.airWeapon().minRange()/32;
		if (ground_dps>0)  { 
			gmax_range++;	// To ensure rounding up
			if (unit_t != UnitTypes::Terran_Marine) gmax_range++;
		}
		if (air_dps>0) amax_range += 4;	// To ensure rounding up
		int max_range = (gmax_range>amax_range ? gmax_range:amax_range);
		int min_range = (gmin_range<amin_range ? gmin_range:amin_range);
	
		int gmax_range_sq = gmax_range*gmax_range;
		int gmin_range_sq = gmin_range*gmin_range;
		int amax_range_sq = amax_range*amax_range;
		int amin_range_sq = amin_range*amin_range;

//		DEBUG("Adding " << unit_t.getName() << " " << ground_dps << " with ranges " << gmin_range << "-" << gmax_range << " to ground dps map");

		for(int ix = -max_range;ix<=max_range;ix++) {
			if (pos.x+ix>=0 && pos.x+ix<dx) {
				for(int iy = -max_range;iy<=max_range;iy++) {
					if (pos.y+iy>=0 && pos.y+iy<dy) {
						int d = ix*ix + iy*iy;
						if (ground_dps>0 && d>=gmin_range_sq && d<=gmax_range_sq) 
							_ground_enemy_dps_map[pos.x+ix + (pos.y+iy)*dx] += ground_dps;
						if (air_dps>0 && d>=amin_range_sq && d<=amax_range_sq) 
							_air_enemy_dps_map[pos.x+ix + (pos.y+iy)*dx] += air_dps;
					}
				}
			}
		}
	}

/*
	// show it:
	{
		double max_dps = 0;
		for(int i = 0;i<dx*dy;i++) if (_ground_enemy_dps_map[i]>max_dps) max_dps = _ground_enemy_dps_map[i];
		if (max_dps>0) {
			char *line;
			line = new char[dx+1];
			DEBUG("ground dps map, max = " << max_dps);
			for(int y = 0;y<dy;y++) {
				
				for(int x = 0;x<dx;x++) {
					line[x] = '0' + int((_ground_enemy_dps_map[y*dx + x]*9)/max_dps);
				}
				line[dx]=0;
				DEBUG(line);
			}
			delete line;
		}
	}
*/
	_last_cycle_dps_map_was_recomputed = cycle;
}

void InformationManager::drawAirDPSMap()
{
	for(int x=0; x < Broodwar->mapWidth(); ++x) {
		for(int y=0; y < Broodwar->mapHeight(); ++y) {
			if (_air_enemy_dps_map[x + y*BWAPI::Broodwar->mapWidth()] != 0)
				Broodwar->drawTextMap(x*TILE_SIZE+16,y*TILE_SIZE+16,"%0.2f",_air_enemy_dps_map[x + y*BWAPI::Broodwar->mapWidth()]);
		}
	}
}

void InformationManager::drawGroundDPSMap()
{
	for(int x=0; x < Broodwar->mapWidth(); ++x) {
		for(int y=0; y < Broodwar->mapHeight(); ++y) {
			if (_ground_enemy_dps_map[x + y*BWAPI::Broodwar->mapWidth()] != 0)
				Broodwar->drawTextMap(x*TILE_SIZE+16,y*TILE_SIZE+16,"%0.2f",_ground_enemy_dps_map[x + y*BWAPI::Broodwar->mapWidth()]);
		}
	}
}

void InformationManager::markEnemyAsSeen(Unit unit)
{
	if (unit) {
		UnitToCache::iterator found = visibleEnemies.find(unit);
		if (found == visibleEnemies.end()) {
			LOG4CXX_TRACE(_logger, "Enemy " << unit->getType() << " not in visibleEnemies list! Probably not an enemy");
		} else {
			seenEnemies[unit] = visibleEnemies[unit];
			visibleEnemies.erase(unit);
		}
	}
}

void InformationManager::markEnemyAsVisible(Unit unit)
{
	if (unit) {
		seenEnemies.erase(unit); // we don't care if it wasn't in the list
		visibleEnemies[unit] = unitCache_t(unit->getType(), unit->getPosition());
	}
}

void InformationManager::updateLastSeenUnitPosition()
{
	// if a seenEnemy is visible means that it's an unassigned enemy
	for (auto unitCache : seenEnemies) {
		if (unitCache.first->canMove() && unitCache.first->isVisible()) {
			unitCache.second.position = unitCache.first->getPosition();
		}
	}

	for (auto unitCache : visibleEnemies) {
		if (!unitCache.first->isVisible() && !Broodwar->isFlagEnabled(Flag::CompleteMapInformation)) {
			LOG4CXX_ERROR(_logger, "Enemy in the visibleEnemies list NOT visible");
		} else if (unitCache.first->canMove()) {
			unitCache.second.position = unitCache.first->getPosition();
		}
	}
}

UnitToCache::iterator InformationManager::deleteSeenEnemy(UnitToCache::iterator unit)
{
	UnitToCache::iterator nextEnemy = seenEnemies.erase(unit);
	return nextEnemy;
}

void InformationManager::scanBases()
{
	if (_cloakedEnemyPositions.empty()) {
		if (!_emptyBases.empty()) {
			if (_lastBaseScan == _emptyBases.end()) {
				_lastBaseScan = _emptyBases.begin();
			}
			for (UnitToTimeMap::iterator comsat = _comsatStation.begin(); comsat != _comsatStation.end(); ++comsat) {
				if ( /*(comsat->first->getEnergy() >= 100 && !informationManager->_turretDefenses) ||*/
					 /*(*/comsat->first->getEnergy() == 200 /*&& informationManager->_turretDefenses)*/ ) {
					Position pos = (*_lastBaseScan)->getPosition();
					bool scanned = useScanner(comsat->first, pos);
					if (scanned) {
						_lastBaseScan++;
						if (_lastBaseScan == _emptyBases.end()) {
							_lastBaseScan = _emptyBases.begin();
						}
					}
				}
			}
		} else if (!_ignoreBases.empty()) {
			if (_lastIgnoreBaseScan == _ignoreBases.end()) {
				_lastIgnoreBaseScan = _ignoreBases.begin();
			}
			for (UnitToTimeMap::iterator comsat = _comsatStation.begin(); comsat != _comsatStation.end(); ++comsat) {
				if (comsat->first->getEnergy() >= 100) {
					Position pos = Position(*_lastIgnoreBaseScan);
					bool scanned = useScanner(comsat->first, pos);
					if (scanned) {
						_lastIgnoreBaseScan++;
						if (_lastIgnoreBaseScan == _ignoreBases.end()) {
							_lastIgnoreBaseScan = _ignoreBases.begin();
						}
					}
				}
			}
		}
	}
}



bool InformationManager::useScanner(Unit comsat, Position pos) 
{
	if (Broodwar->getFrameCount() - _comsatStation[comsat] > SCANNER_SWEEP_FREQUENCY) {
		//Broodwar->printf("Using Scanner Sweep on base (%d,%d)", pos.x, pos.y);
		comsat->useTech(TechTypes::Scanner_Sweep, pos);
		_comsatStation[comsat] = Broodwar->getFrameCount();
		return true;
	}
	return false;
}

void InformationManager::onMissileTurretShow(Unit unit)
{
	_missileTurrets[unit] = BWTA::getRegion(unit->getTilePosition());
}

void InformationManager::onMissileTurretDestroy(Unit unit)
{
	_missileTurrets.erase(unit);
}

void InformationManager::printDebugBox(BWAPI::Position topLeft, BWAPI::Position bottomRight, std::string message)
{
	_topLeft = topLeft;
	_bottomRight = bottomRight;
	Broodwar->printf(message.c_str());
	Broodwar->setScreenPosition(topLeft);
}