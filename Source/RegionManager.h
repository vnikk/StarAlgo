#pragma once
#include "BWAPI.h"
#include "BWTA.h"

class RegionManager {
public:
    RegionManager();

    std::map<BWTA::Region*, int> regionID;
    std::map<int, BWTA::Region*> regionFromID;
    BWTA::RectangleArray<uint8_t> regionIdMap;
    BWTA::RectangleArray<int> distanceBetweenRegions;
    std::map<BWTA::Chokepoint*, int> chokePointID;
    std::map<int, BWTA::Chokepoint*> chokePointFromID;
    bool onlyRegions = true;

    BWAPI::Position getCenterRegionId(int regionId);
    BWTA::Region* getNearestRegion(int x, int y);

private:
    void initRegion();
    void setRegionDistance();
};
