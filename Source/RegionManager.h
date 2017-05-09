#pragma once
#include "BWAPI.h"
#include "BWTA.h"

class RegionManager {
public:
    RegionManager();
    BWAPI::Position getCenterRegionId(int regionId);

    std::map<BWTA::Region*, int> _regionID;
    std::map<int, BWTA::Region*> _regionFromID;
    BWTA::RectangleArray<uint8_t> _regionIdMap;
    BWTA::RectangleArray<int> _distanceBetweenRegions;
    // TODO maybe use boost::multi_index instead of two std::map?
    std::map<BWTA::Chokepoint*, int> _chokePointID;
    std::map<int, BWTA::Chokepoint*> _chokePointFromID;
    bool _onlyRegions = true;

    void initRegion();
    BWTA::Region* getNearestRegion(int x, int y);
private:
    void initRegionIdMap();
    void setRegionDistance();
};