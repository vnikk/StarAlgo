#pragma once
#include <random>
#include <algorithm>

#include "GameState.h"
#include "CombatInfo.h"
#include "TargetSorting.h"
//#include "InformationManager.h"

typedef std::vector<unitGroup_t*> UnitGroupVector;
typedef std::function<bool(const unitGroup_t* a, const unitGroup_t* b)> comp_f;

class CombatSimulator
{
public:
	virtual CombatSimulator* clone() const = 0;  // Virtual constructor (copying) 
	virtual ~CombatSimulator() {}

	/// <summary>Retrieves the expected duration of the combat.</summary>
	/// <param name="army">Abstraction of enemy and self units.</param>
	/// <returns>The minimum time (in frames) required to destroy one of the armies.</returns>
	virtual int getCombatLength(GameState::army_t* army) = 0;

	/// <summary>Simulate a combat between army.enemy and army.friendly.</summary>
	/// <param name="armyInCombat">List of units involved in the combat.</param>
	/// <param name="army">Whole army where the units killed will be removed.</param>
	/// <param anme="frames">Number of frames to simulate (0 = until one army is destroyed).</param>
	virtual void simulateCombat(GameState::army_t* armyInCombat, GameState::army_t* army, int frames = 0) = 0;


	/// <summary>Remove one goup from a list</summary>
	/// <param name="groupToRemove">Pointer of the group to be removed.</param>
	/// <param name="groups">Pointer of the list of groups from we will remove the groups.</param>
	/// <returns>True if the group was removed.</returns>
	inline const bool removeGroup(unitGroup_t* groupToRemove, UnitGroupVector* groups)
	{
		auto groupFound = std::find(groups->begin(), groups->end(), groupToRemove);
		if (groupFound != groups->end()) {
			delete *groupFound;
			groups->erase(groupFound);
			return true;
		} else {
			DEBUG("[ERROR] Group unit not found");
			return false;
		}
	}

	/// <summary>Remove multiple goups from a list</summary>
	/// <param name="groupsToRemove">List of pointer of groups to be removed.</param>
	/// <param name="groups">Pointer of the list of groups from we will remove the groups.</param>
	inline const void removeAllGroups(UnitGroupVector* groupsToRemove, UnitGroupVector* groups)
	{
		for (const auto& groupToDelete : *groupsToRemove) removeGroup(groupToDelete, groups);
		groupsToRemove->clear();
	}

	/// <summary>Remove multiple military goups from a list. It assumes that the list is sorted 
	/// (not military units at the end)</summary>
	/// <param name="groupsToRemove">List of pointer of groups to be removed.</param>
	/// <param name="groups">Pointer of the list of groups from we will remove the groups.</param>
	inline const void removeAllMilitaryGroups(UnitGroupVector* groupsToRemove, UnitGroupVector* groups);
	

	/// <summary>Check if the combat can be simulated</summary>
	/// <param name="armyInCombat">List of units involved in the combat.</param>
	/// <param name="army">Whole army (all units in armyInCombat should appear here).</param>
	/// <returns>True if the combat can be simulated.</returns>
	bool canSimulate(GameState::army_t* armyInCombat, GameState::army_t* army);

	/// Remove Harmless indestructible units
	inline const void removeHarmlessIndestructibleUnits(GameState::army_t* armyInCombat)
	{
		bool friendlyCanAttackGround = false;
		bool friendlyCanAttackAir = false;
		bool enemyCanAttackGround = false;
		bool enemyCanAttackAir = false;
		bool friendlyOnlyGroundUnits = true;
		bool friendlyOnlyAirUnits = true;
		bool enemyOnlyGroundUnits = true;
		bool enemyOnlyAirUnits = true;
		// a (potential) harmless group is a unit type that cannot attack any enemy unit, 
		// i.e. they cannot attack both (air/ground) units
		UnitGroupVector friendlyHarmlessGroups;
		UnitGroupVector enemyHarmlessGroups;

		// collect stats
		for (const auto& group : armyInCombat->friendly) {
			BWAPI::UnitType uType(group->unitTypeId);
			if (canAttackAirUnits(uType)) friendlyCanAttackAir = true;
			if (canAttackGroundUnits(uType)) friendlyCanAttackGround = true;
			if (uType.isFlyer()) friendlyOnlyGroundUnits = false;
			else friendlyOnlyAirUnits = false;
			if (!canAttackAirUnits(uType) || !canAttackGroundUnits(uType)) friendlyHarmlessGroups.push_back(group);
		}
		for (const auto& group : armyInCombat->enemy) {
			BWAPI::UnitType uType(group->unitTypeId);
			if (canAttackAirUnits(uType)) enemyCanAttackAir = true;
			if (canAttackGroundUnits(uType)) enemyCanAttackGround = true;
			if (uType.isFlyer()) enemyOnlyGroundUnits = false;
			else enemyOnlyAirUnits = false;
			if (!canAttackAirUnits(uType) || !canAttackGroundUnits(uType)) enemyHarmlessGroups.push_back(group);
		}

		// remove units that cannot deal damage and cannot receive damage
		for (auto& group : friendlyHarmlessGroups) {
			BWAPI::UnitType uType(group->unitTypeId);
			if ((uType.isFlyer() && !enemyCanAttackAir) || (!uType.isFlyer() && !enemyCanAttackGround)) {
				// we cannot receive damage
				if ((!canAttackAirUnits(uType) && !canAttackGroundUnits(uType)) ||
					(canAttackAirUnits(uType) && enemyOnlyGroundUnits) ||
					(canAttackGroundUnits(uType) && enemyOnlyAirUnits)) {
					// wee cannot deal damage, therefore, remove unit
					armyInCombat->friendly.erase(std::remove(armyInCombat->friendly.begin(), armyInCombat->friendly.end(), group), armyInCombat->friendly.end());
				}
			}
		}
		for (auto& group : enemyHarmlessGroups) {
			BWAPI::UnitType uType(group->unitTypeId);
			if ((uType.isFlyer() && !friendlyCanAttackAir) || (!uType.isFlyer() && !friendlyCanAttackGround)) {
				// we cannot receive damage
				if ((!canAttackAirUnits(uType) && !canAttackGroundUnits(uType)) ||
					(canAttackAirUnits(uType) && friendlyOnlyGroundUnits) || 
					(canAttackGroundUnits(uType) && friendlyOnlyAirUnits)) {
					// wee cannot deal damage, therefore, remove unit
					armyInCombat->enemy.erase(std::remove(armyInCombat->enemy.begin(), armyInCombat->enemy.end(), group), armyInCombat->enemy.end());
				}
			}
		}
	}

	enum GroupDiversity { AIR, GROUND, BOTH };
	GroupDiversity getGroupDiversity(UnitGroupVector* groups);

	comp_f _comparator1;
	comp_f _comparator2;

	inline const void sortGroups(UnitGroupVector* groups, comp_f comparator, UnitGroupVector* attackers);
	
};