#include "stdafx.h"
#include "CombatSimulator.h"

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
