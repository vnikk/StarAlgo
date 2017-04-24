#include "stdafx.h"
#include "CombatSimulator.h"

inline const void CombatSimulator::removeAllMilitaryGroups(UnitGroupVector* groupsToRemove, UnitGroupVector* groups)
{
	for (const auto& groupToDelete : *groupsToRemove) {
		if (isPassiveBuilding(groupToDelete)) break;
		removeGroup(groupToDelete, groups);
	}
}

bool CombatSimulator::canSimulate(GameState::army_t* armyInCombat, GameState::army_t* army)
{
	if (armyInCombat->friendly.empty() || armyInCombat->enemy.empty() || army->friendly.empty() || army->enemy.empty()) {
		DEBUG("One of the combat sets is empty, combat simulation impossible");
		return false;
	}
	for (const auto& group : armyInCombat->friendly) {
		auto groupFound = std::find(army->friendly.begin(), army->friendly.end(), group);
		if (groupFound == army->friendly.end()) {
			DEBUG("One of the groups in armyInCombat is not present in army");
			return false;
		}
	}
	for (const auto& group : armyInCombat->enemy) {
		auto groupFound = std::find(army->enemy.begin(), army->enemy.end(), group);
		if (groupFound == army->enemy.end()) {
			DEBUG("One of the groups in armyInCombat is not present in army");
			return false;
		}
	}
	return true;
}

CombatSimulator::GroupDiversity CombatSimulator::getGroupDiversity(UnitGroupVector* groups)
{
	bool hasAirUnits = false;
	bool hasGroundUnits = false;
	for (const auto& group : *groups) {
		BWAPI::UnitType gType(group->unitTypeId);
		if (gType.isFlyer()) hasAirUnits = true;
		else hasGroundUnits = true;
	}
	if (hasAirUnits && hasGroundUnits) return GroupDiversity::BOTH;
	if (hasAirUnits) return GroupDiversity::AIR;
	return GroupDiversity::GROUND;
}

inline const void CombatSimulator::sortGroups(UnitGroupVector* groups, comp_f comparator, UnitGroupVector* attackers)
{
	if (groups->size() <= 1) return; // nothing to sort
	// sort by default (buildings and non-attacking units at the end)
	std::sort(groups->begin(), groups->end(), sortByBuilding);
	// find first passive building and sort until it
	auto passiveBuilding = std::find_if(groups->begin(), groups->end(), isPassiveBuilding);

	if (comparator == nullptr) {
		// by default use the learned priorities
		GroupDiversity groupDiversity = getGroupDiversity(attackers);
		switch (groupDiversity) {
		case AIR:	 std::sort(groups->begin(), passiveBuilding, sortByTypeAir); break;
		case GROUND: std::sort(groups->begin(), passiveBuilding, sortByTypeGround); break;
		case BOTH:	 std::sort(groups->begin(), passiveBuilding, sortByTypeBoth); break;
		}
		// by default random
		// 			std::random_device combatRD;
		// 			std::mt19937 combatG(combatRD());
		// 			std::shuffle(groups->begin(), passiveBuilding, combatG);
	}
	else {
		std::sort(groups->begin(), passiveBuilding, comparator);
	}

}
