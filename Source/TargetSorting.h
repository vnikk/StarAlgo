#pragma once

#include "stdafx.h"
#include "AbstractGroup.h"
#include "UnitInfoStatic.h"

#include <algorithm>

// © Alberto Uriarte
struct sortByBuildingClass {
    bool operator() (const unitGroup_t* a, const unitGroup_t* b) {
        return isPassiveBuilding(a) < isPassiveBuilding(b);
    }
};
extern sortByBuildingClass sortByBuilding;

// © Alberto Uriarte
struct sortByScoreClass {
    bool operator() (const unitGroup_t* a, const unitGroup_t* b) {
        BWAPI::UnitType aType(a->unitTypeId);
        BWAPI::UnitType bType(b->unitTypeId);
        return aType.destroyScore() > bType.destroyScore();
    }
};
extern sortByScoreClass sortByScore;

// © me & Alberto Uriarte
struct sortByDpsClass {
    bool operator() (const unitGroup_t* a, const unitGroup_t* b) {
        auto& aDPF           = unitStatic->DPF[a->unitTypeId];
        double maxDpfASingle = std::max(aDPF.air, aDPF.ground);
        double maxDpfABoth   = std::max(aDPF.bothAir, aDPF.bothGround);
        double maxDpfA       = std::max(maxDpfASingle, maxDpfABoth);

        auto& bDPF           = unitStatic->DPF[b->unitTypeId];
        double maxDpfBSingle = std::max(bDPF.air, bDPF.ground);
        double maxDpfBBoth   = std::max(bDPF.bothAir, bDPF.bothGround);
        double maxDpfB       = std::max(maxDpfBSingle, maxDpfBBoth);

        return maxDpfA > maxDpfB;
    }
};
extern sortByDpsClass sortByDps;

// © Alberto Uriarte
struct sortByTypeClass {
    bool operator() (const unitGroup_t* a, const unitGroup_t* b) {
        return _typePriority->at(a->unitTypeId) > _typePriority->at(b->unitTypeId);
    }

    const std::vector<float>* _typePriority;
    sortByTypeClass(std::vector<float>* typePriority) : _typePriority(typePriority) {};
};

extern sortByTypeClass sortByTypeAir;
extern sortByTypeClass sortByTypeGround;
extern sortByTypeClass sortByTypeBoth;
