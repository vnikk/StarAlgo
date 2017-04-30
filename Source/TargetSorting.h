#pragma once

#include "AbstractGroup.h"
#include "UnitInfoStatic.h"

#include <algorithm>

struct sortByBuildingClass {
	bool operator() (const unitGroup_t* a, const unitGroup_t* b) {
		return isPassiveBuilding(a) < isPassiveBuilding(b);
	}
};
extern sortByBuildingClass sortByBuilding;

struct sortByScoreClass {
	bool operator() (const unitGroup_t* a, const unitGroup_t* b) {
		BWAPI::UnitType aType(a->unitTypeId);
		BWAPI::UnitType bType(b->unitTypeId);
		return aType.destroyScore() > bType.destroyScore();
	}
};
extern sortByScoreClass sortByScore;

struct sortByDpsClass {
	bool operator() (const unitGroup_t* a, const unitGroup_t* b) {
		double maxDpfASingle = std::max(unitStatic->DPF[a->unitTypeId].air, unitStatic->DPF[a->unitTypeId].ground);
		double maxDpfABoth = std::max(unitStatic->DPF[a->unitTypeId].bothAir, unitStatic->DPF[a->unitTypeId].bothGround);
		double maxDpfA = std::max(maxDpfASingle, maxDpfABoth);

		double maxDpfBSingle = std::max(unitStatic->DPF[b->unitTypeId].air, unitStatic->DPF[b->unitTypeId].ground);
		double maxDpfBBoth = std::max(unitStatic->DPF[b->unitTypeId].bothAir, unitStatic->DPF[b->unitTypeId].bothGround);
		double maxDpfB = std::max(maxDpfBSingle, maxDpfBBoth);

		return maxDpfA > maxDpfB;
	}
};
extern sortByDpsClass sortByDps;

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
