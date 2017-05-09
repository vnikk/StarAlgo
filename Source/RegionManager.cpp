#include "stdafx.h"
#include "RegionManager.h"

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


RegionManager::RegionManager() {
    initRegionIdMap();
    initRegion();
    setRegionDistance();
}

BWAPI::Position RegionManager::getCenterRegionId(int regionId)
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

void RegionManager::initRegionIdMap() // TODO rename and check with following one
{
    int dx = BWAPI::Broodwar->mapWidth();
    int dy = BWAPI::Broodwar->mapHeight();
    _regionIdMap.resize(dx, dy);
    _regionIdMap.setTo(0);

    // -- Fill map to regionID variable
    for (int x = 0; x < BWAPI::Broodwar->mapWidth(); ++x) {
        for (int y = 0; y < BWAPI::Broodwar->mapHeight(); ++y) {
            BWTA::Region* tileRegion = BWTA::getRegion(x, y);
            if (tileRegion == NULL) tileRegion = getNearestRegion(x, y);
            _regionIdMap[x][y] = _regionID[tileRegion];
        }
    }
}

// TODO where should this be called from?
void RegionManager::initRegion()
{
    const std::set<BWTA::Region*> &regions_unsort = BWTA::getRegions();
    std::set<BWTA::Region*, SortByXY> regions(regions_unsort.begin(), regions_unsort.end());

    // -- Assign region to identifiers
    int id = 0;
    for (std::set<BWTA::Region*, SortByXY>::const_iterator r = regions.begin(); r != regions.end(); ++r) {
        _regionID[*r] = id;
        _regionFromID[id] = *r;
        id++;
    }

    for (int x = 0; x < BWAPI::Broodwar->mapWidth(); ++x) {
        for (int y = 0; y < BWAPI::Broodwar->mapHeight(); ++y) {
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
BWTA::Region* RegionManager::getNearestRegion(int x, int y)
{
    //searches outward in a spiral.
    int length = 1;
    int j = 0;
    bool first = true;
    int dx = 0;
    int dy = 1;
    BWTA::Region* tileRegion = NULL;
    while (length < BWAPI::Broodwar->mapWidth()) //We'll ride the spiral to the end
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

void RegionManager::setRegionDistance()
{
    _onlyRegions = true;
    int distance = 0;
    const std::set<BWTA::Region*>& regions_unsort = BWTA::getRegions();
    std::set<BWTA::Region*, SortByXY> regions(regions_unsort.begin(), regions_unsort.end());

    // -- Precalculate distance between regions
    _distanceBetweenRegions.resize(regions.size(), regions.size());
    for (const auto& r1 : regions) {
        for (const auto& r2 : regions) {
            BWAPI::Position pos1(r1->getCenter());
            BWAPI::Position pos2(r2->getCenter());
            int id1 = _regionID[r1];
            int id2 = _regionID[r2];
            // sometimes the center of the region is in an unwalkable area
            if (BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(pos1)) &&
                BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(pos2))) {
                distance = BWTA::getGroundDistance(BWAPI::TilePosition(pos1), BWAPI::TilePosition(pos2));
            }
            else { // TODO improve this to get a proper position and compute groundDistance
                // 					if (!Broodwar->isWalkable(BWAPI::WalkPosition(pos1))) {
                // 						LOG("Center of Region " << id1 << " isn't walkable");
                // 					}
                // 					if (!Broodwar->isWalkable(BWAPI::WalkPosition(pos2))) {
                // 						LOG("Center of Region " << id2 << " isn't walkable");
                // 					}
                distance = pos1.getDistance(pos2);
            }
            // 				LOG("Storing ["<<id1<<"]["<<id2<<"] = " << distance);
            _distanceBetweenRegions[id1][id2] = distance;
        }
    }
}
