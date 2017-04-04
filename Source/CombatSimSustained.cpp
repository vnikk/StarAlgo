#include "stdafx.h"
#include "CombatSimSustained.h"

CombatSimSustained::CombatSimSustained(std::vector<DPF_t>* maxDPF, comp_f comparator1, comp_f comparator2)
	: _maxDPF(maxDPF),
	_timeToKillEnemy(0), 
	_timeToKillFriend(0)
{
	_comparator1 = comparator1;
	_comparator2 = comparator2;
}

CombatSimSustained::combatStats_t CombatSimSustained::getCombatStats(const UnitGroupVector &army)
{
	combatStats_t combatStats;

	for (auto unitGroup : army) {
		int typeId = unitGroup->unitTypeId;
		// auto siege tanks TODO only if researched!!
		if (typeId == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) {
			typeId = BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode;
		}
		combatStats.bothAirDPF += unitGroup->numUnits * (*_maxDPF)[typeId].bothAir;
		combatStats.bothGroundDPF += unitGroup->numUnits * (*_maxDPF)[typeId].bothGround;
		combatStats.airDPF += unitGroup->numUnits * (*_maxDPF)[typeId].air;
		combatStats.groundDPF += unitGroup->numUnits * (*_maxDPF)[typeId].ground;

		// if unit cannot attack, we don't consider its HP to "beat" (to exclude dummy buildings)
		BWAPI::UnitType unitType(typeId);
		if (unitType.canAttack() || unitType.isSpellcaster() || unitType.spaceProvided() > 0) {
			if (unitType.isFlyer()) combatStats.airHP			+= unitGroup->numUnits * unitGroup->HP;
			else					combatStats.groundHP		+= unitGroup->numUnits * unitGroup->HP;
		} else {
			if (unitType.isFlyer()) combatStats.airHPextra		+= unitGroup->numUnits * unitGroup->HP;
			else					combatStats.groundHPextra	+= unitGroup->numUnits * unitGroup->HP;
		}
	}

	return combatStats;
}


void CombatSimSustained::getCombatLength(combatStats_t friendStats, combatStats_t enemyStats)
{
	// groups that can attack ground and air help to kill the group that it takes more time to kill
	double timeToKillEnemyAir = (enemyStats.airHP > 0) ? (friendStats.airDPF == 0) ? INT_MAX : enemyStats.airHP / friendStats.airDPF : 0;
	double timeToKillEnemyGround = (enemyStats.groundHP > 0) ? (friendStats.groundDPF == 0) ? INT_MAX : enemyStats.groundHP / friendStats.groundDPF : 0;
	if (friendStats.bothAirDPF > 0) {
		if (timeToKillEnemyAir > timeToKillEnemyGround) {
			double combinetDPF = friendStats.airDPF + friendStats.bothAirDPF;
			timeToKillEnemyAir = (enemyStats.airHP > 0) ? (combinetDPF == 0) ? INT_MAX : enemyStats.airHP / combinetDPF : 0;
		} else {
			double combinetDPF = friendStats.groundDPF + friendStats.bothGroundDPF;
			timeToKillEnemyGround = (enemyStats.groundHP > 0) ? (combinetDPF == 0) ? INT_MAX : enemyStats.groundHP / combinetDPF : 0;
		}
	}

	double timeToKillFriendAir = (friendStats.airHP > 0) ? (enemyStats.airDPF == 0) ? INT_MAX : friendStats.airHP / enemyStats.airDPF : 0;
	double timeToKillFriendGround = (friendStats.groundHP > 0) ? (enemyStats.groundDPF == 0) ? INT_MAX : friendStats.groundHP / enemyStats.groundDPF : 0;
	if (enemyStats.bothAirDPF > 0) {
		if (timeToKillFriendAir > timeToKillEnemyGround) {
			double combinetDPF = enemyStats.airDPF + enemyStats.bothAirDPF;
			timeToKillFriendAir = (friendStats.airHP > 0) ? (combinetDPF == 0) ? INT_MAX : friendStats.airHP / combinetDPF : 0;
		} else {
			double combinetDPF = enemyStats.groundDPF + enemyStats.bothGroundDPF;
			timeToKillFriendGround = (friendStats.groundHP > 0) ? (combinetDPF == 0) ? INT_MAX : friendStats.groundHP / combinetDPF : 0;
		}
	}

	if (timeToKillEnemyAir == INT_MAX && timeToKillEnemyGround > 0) _timeToKillEnemy = (int)timeToKillEnemyGround;
	else if (timeToKillEnemyGround == INT_MAX && timeToKillEnemyAir > 0) _timeToKillEnemy = (int)timeToKillEnemyAir;
	else _timeToKillEnemy = (int)(std::max)(timeToKillEnemyAir, timeToKillEnemyGround);
	
	if (timeToKillFriendAir == INT_MAX && timeToKillFriendGround > 0) _timeToKillFriend = (int)timeToKillFriendGround;
	else if (timeToKillFriendGround == INT_MAX && timeToKillFriendAir > 0) _timeToKillFriend = (int)timeToKillFriendAir;
	else _timeToKillFriend = (int)(std::max)(timeToKillFriendAir, timeToKillFriendGround);
}

void CombatSimSustained::getExtraCombatLength(combatStats_t friendStats, combatStats_t enemyStats)
{
	// groups that can attack ground and air help to kill the group that it takes more time to kill
	double timeToKillEnemyAir = (enemyStats.airHPextra > 0) ? (friendStats.airDPF == 0) ? INT_MAX : enemyStats.airHPextra / friendStats.airDPF : 0;
	double timeToKillEnemyGround = (enemyStats.groundHPextra > 0) ? (friendStats.groundDPF == 0) ? INT_MAX : enemyStats.groundHPextra / friendStats.groundDPF : 0;
	if (friendStats.bothAirDPF > 0) {
		if (timeToKillEnemyAir > timeToKillEnemyGround) {
			double combinetDPF = friendStats.airDPF + friendStats.bothAirDPF;
			timeToKillEnemyAir = (enemyStats.airHPextra > 0) ? (combinetDPF == 0) ? INT_MAX : enemyStats.airHPextra / combinetDPF : 0;
		} else {
			double combinetDPF = friendStats.groundDPF + friendStats.bothGroundDPF;
			timeToKillEnemyGround = (enemyStats.groundHPextra > 0) ? (combinetDPF == 0) ? INT_MAX : enemyStats.groundHPextra / combinetDPF : 0;
		}
	}

	double timeToKillFriendAir = (friendStats.airHPextra > 0) ? (enemyStats.airDPF == 0) ? INT_MAX : friendStats.airHPextra / enemyStats.airDPF : 0;
	double timeToKillFriendGround = (friendStats.groundHPextra > 0) ? (enemyStats.groundDPF == 0) ? INT_MAX : friendStats.groundHPextra / enemyStats.groundDPF : 0;
	if (enemyStats.bothAirDPF > 0) {
		if (timeToKillFriendAir > timeToKillEnemyGround) {
			double combinetDPF = enemyStats.airDPF + enemyStats.bothAirDPF;
			timeToKillFriendAir = (friendStats.airHPextra > 0) ? (combinetDPF == 0) ? INT_MAX : friendStats.airHPextra / combinetDPF : 0;
		} else {
			double combinetDPF = enemyStats.groundDPF + enemyStats.bothGroundDPF;
			timeToKillFriendGround = (friendStats.groundHPextra > 0) ? (combinetDPF == 0) ? INT_MAX : friendStats.groundHPextra / combinetDPF : 0;
		}
	}

	if (timeToKillEnemyAir == INT_MAX && timeToKillEnemyGround > 0) _extraTimeToKillEnemy = (int)timeToKillEnemyGround;
	else if (timeToKillEnemyGround == INT_MAX && timeToKillEnemyAir > 0) _extraTimeToKillEnemy = (int)timeToKillEnemyAir;
	else _extraTimeToKillEnemy = (int)(std::max)(timeToKillEnemyAir, timeToKillEnemyGround);

	if (timeToKillFriendAir == INT_MAX && timeToKillFriendGround > 0) _extraTimeToKillFriend = (int)timeToKillFriendGround;
	else if (timeToKillFriendGround == INT_MAX && timeToKillFriendAir > 0) _extraTimeToKillFriend = (int)timeToKillFriendAir;
	else _extraTimeToKillFriend = (int)(std::max)(timeToKillFriendAir, timeToKillFriendGround);
}

int CombatSimSustained::getCombatLength(GameState::army_t* army)
{
	removeHarmlessIndestructibleUnits(army);
	combatStats_t friendStats = getCombatStats(army->friendly);
	combatStats_t enemyStats = getCombatStats(army->enemy);
	getCombatLength(friendStats, enemyStats);
	getExtraCombatLength(friendStats, enemyStats);
	if (_timeToKillEnemy == INT_MAX || _extraTimeToKillEnemy == INT_MAX) _timeToKillEnemy = INT_MAX;
	else _timeToKillEnemy += _extraTimeToKillEnemy;
	if (_timeToKillFriend == INT_MAX || _extraTimeToKillFriend == INT_MAX) _timeToKillFriend = INT_MAX;
	else _timeToKillFriend += _extraTimeToKillFriend;
	return (std::min)(_timeToKillEnemy, _timeToKillFriend);
}

void CombatSimSustained::simulateCombat(GameState::army_t* armyInCombat, GameState::army_t* army, int frames)
{
	removeHarmlessIndestructibleUnits(armyInCombat);
	if (!canSimulate(armyInCombat, army)) return;

	sortGroups(&armyInCombat->friendly, _comparator1, &armyInCombat->enemy);
	sortGroups(&armyInCombat->enemy, _comparator2, &armyInCombat->friendly);
	
	combatStats_t friendStats = getCombatStats(armyInCombat->friendly);
	combatStats_t enemyStats = getCombatStats(armyInCombat->enemy);

	// calculate end combat time
	getCombatLength(friendStats, enemyStats);
// 	getExtraCombatLength(friendStats, enemyStats);
// 	if (_timeToKillEnemy == INT_MAX || _extraTimeToKillEnemy == INT_MAX) _timeToKillEnemy = INT_MAX;
// 	else _timeToKillEnemy += _extraTimeToKillEnemy;
// 	if (_timeToKillFriend == INT_MAX || _extraTimeToKillFriend == INT_MAX) _timeToKillFriend = INT_MAX;
// 	else _timeToKillFriend += _extraTimeToKillFriend;
	int combatLength = (std::min)(_timeToKillEnemy, _timeToKillFriend);

	if (frames == 0 || frames >= combatLength) { // at least one army is destroyed
		if (_timeToKillEnemy == _timeToKillFriend) { // both armies are destroyed
			removeAllMilitaryGroups(&armyInCombat->friendly, &army->friendly);
			removeAllMilitaryGroups(&armyInCombat->enemy, &army->enemy);
		} else { // only one army is destroyed
			UnitGroupVector* loserUnits = &army->friendly;
			UnitGroupVector* winnerUnits = &army->enemy;
			UnitGroupVector* loserInCombat = &armyInCombat->friendly;
			UnitGroupVector* winnerInCombat = &armyInCombat->enemy;
			combatStats_t* loserStats = &friendStats;
			if (combatLength == _timeToKillEnemy) {
				loserUnits = &army->enemy;
				winnerUnits = &army->friendly;
				loserInCombat = &armyInCombat->enemy;
				winnerInCombat = &armyInCombat->friendly;
				loserStats = &enemyStats;
			}

			removeAllGroups(loserInCombat, loserUnits);
			removeSomeUnitsFromArmy(winnerInCombat, winnerUnits, loserStats, (double)combatLength);
		}
	} else { // both army still alive
		removeSomeUnitsFromArmy(&armyInCombat->friendly, &army->friendly, &enemyStats, (double)frames);
		removeSomeUnitsFromArmy(&armyInCombat->enemy, &army->enemy, &friendStats, (double)frames);
	}
}

void CombatSimSustained::removeSomeUnitsFromArmy(UnitGroupVector* winnerInCombat, UnitGroupVector* winnerUnits, combatStats_t* loserStats, double frames)
{
	// Calculate winner losses
	int totalAirDamage = int(loserStats->airDPF * frames);
	int totalGroundDamage = int(loserStats->groundDPF * frames);
	// in the case that we can deal both damages, we consider worst case //TODO have both cases (air and ground)
	int totalBothAirDamage = int(loserStats->bothAirDPF * frames);
	int totalBothGroundDamage = int(loserStats->bothGroundDPF * frames);

	int* damageToDeal;
	int* bothDamageInUse;
	bool usingBothDamage;
	int unitsToRemove;
	bool unitDeleted;
	// original vectors
	UnitGroupVector inCombat = *winnerInCombat;
	UnitGroupVector units = *winnerUnits;

	for (UnitGroupVector::iterator it = winnerInCombat->begin(); it != winnerInCombat->end();) {
		// stop if we don't have more damage to receive
		if (totalAirDamage <= 0 && totalGroundDamage <= 0 && (totalBothAirDamage <= 0 || totalBothGroundDamage <= 0)) break;

		unitGroup_t* groupDamaged = *it;
		unitDeleted = false;
		BWAPI::UnitType unitType(groupDamaged->unitTypeId);
		int unitTypeHP = groupDamaged->HP;
		int groupHP = groupDamaged->HP * groupDamaged->numUnits;
		// define type of damage to receive
		if (unitType.isFlyer()) {
			bothDamageInUse = &totalBothAirDamage;
			damageToDeal = &totalBothAirDamage;
			usingBothDamage = true;
			if (totalAirDamage > 0) {
				damageToDeal = &totalAirDamage;
				usingBothDamage = false;
			}
		} else { // unit is ground
			bothDamageInUse = &totalBothGroundDamage;
			damageToDeal = &totalBothGroundDamage;
			usingBothDamage = true;
			if (totalGroundDamage > 0) {
				damageToDeal = &totalGroundDamage;
				usingBothDamage = false;
			}
		}

		// compute losses
		if (*damageToDeal >= groupHP) {
			// kill all the units in the group
			unitDeleted = removeGroup(groupDamaged, winnerUnits); 
			*damageToDeal -= groupHP; // adjust damage left
		} else if (usingBothDamage) {
			// kill some units from the group
			unitsToRemove = (int)trunc((double)*damageToDeal / (double)unitTypeHP);
			if (unitsToRemove > 0) {
				groupDamaged->numUnits -= unitsToRemove;
				if (groupDamaged->numUnits == 0) unitDeleted = removeGroup(groupDamaged, winnerUnits);
				*damageToDeal -= unitsToRemove * unitTypeHP;
			} else { // the damage isn't enough to kill any unit (but it's absorbed)
				*damageToDeal = 0;
			}
		} else {
			// combining specific and both damage
			*bothDamageInUse += *damageToDeal; // notice that this is safe since we are going to use all the added damage
			*damageToDeal = 0;
			if (*bothDamageInUse >= groupHP) {
				// kill all the units in the group
				unitDeleted = removeGroup(groupDamaged, winnerUnits);
				*bothDamageInUse -= groupHP; // adjust damage left
			} else {
				// kill some units from the group
				unitsToRemove = (int)trunc((double)*bothDamageInUse / (double)unitTypeHP);
				if (unitsToRemove > 0) {
					groupDamaged->numUnits -= unitsToRemove;
					if (groupDamaged->numUnits == 0) unitDeleted = removeGroup(groupDamaged, winnerUnits);
					*bothDamageInUse -= unitsToRemove * unitTypeHP;
				} else { // the damage isn't enough to kill any unit (but it's absorbed)
					*bothDamageInUse = 0;
				}
			}
		}
		if (unitDeleted) it = winnerInCombat->erase(it);
		else ++it;
	}
}
