#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <BWAPI.h>

struct unitInfo {
    int ID;
    std::string typeName;
    int typeID;
    int HP;
    int shield;
    int x;
    int y;
    unitInfo() : ID(0), typeName(""), typeID(BWAPI::UnitTypes::None), HP(0), shield(0){}
};

struct unitKilledInfo {
    int unitID;
    int frame;
    int armyID;
};

struct combatInfo {
    int startFrame, endFrame;
    std::map<BWAPI::UnitType, int> armySize1, armySize2;
    // <unitID, unitInfo>
    std::map<int, unitInfo> armyUnits1, armyUnitsEnd1, armyUnits2, armyUnitsEnd2;
    std::vector<unitKilledInfo> kills;
};

const int MAX_UNIT_TYPE = BWAPI::UnitTypes::Enum::MAX;

struct DPF_t {
    // DPF if unit can only attack one type of units
    double air;
    double ground;
    // DPF if unit can attack both type of units (may use different weapons for each type)
    double bothAir;
    double bothGround;
    DPF_t() : air(0.0), ground(0.0), bothAir(0.0), bothGround(0.0){}
};

struct HP_t {
    uint16_t air;        // store the HP if the unit isFlyer
    uint16_t ground;    // store the HP if the unit isNotFlyer
    uint16_t any;        // store the HP regardless the unit's type
    HP_t() : air(0), ground(0), any(0) {}
};
