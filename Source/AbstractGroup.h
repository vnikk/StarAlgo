#pragma once

#include <cstdint>
#include <vector>

struct unitGroup_t {
    uint8_t unitTypeId;
    uint8_t numUnits;
    uint8_t regionId;
    uint8_t orderId;
    uint8_t targetRegionId;
    int endFrame; // expected frame when the order is done
    int startFrame; // frame when the order was issue
    float HP; // group's average HP

    unitGroup_t(uint8_t unit, uint8_t size, uint8_t region, uint8_t order, uint8_t targetRegion, int initFrame, float _HP)
        :unitTypeId(unit), numUnits(size), regionId(region), orderId(order),
        targetRegionId(targetRegion), endFrame(0), startFrame(initFrame), HP(_HP){}
    unitGroup_t(uint8_t unit, uint8_t size, uint8_t region, uint8_t order, uint8_t targetRegion, int frame, int initFrame, float _HP)
        :unitTypeId(unit), numUnits(size), regionId(region), orderId(order),
        targetRegionId(targetRegion), endFrame(frame), startFrame(initFrame), HP(_HP){}
};
typedef std::vector<unitGroup_t*> unitGroupVector;
