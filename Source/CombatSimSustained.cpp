#include "stdafx.h"
#include "CombatSimSustained.h"

// © Alberto Uriarte
CombatSimSustained::CombatSimSustained(std::vector<DPF_t>* maxDPF, comp_f comparator1, comp_f comparator2)
{
    this->maxDPF = maxDPF;
    timeToKillEnemy  = 0;
    timeToKillFriend = 0;
    comparator1 = comparator1;
    comparator2 = comparator2;
}

// © me & Alberto Uriarte
CombatSimSustained::combatStats_t CombatSimSustained::getCombatStats(const UnitGroupVector &army)
{
    combatStats_t combatStats;
    for (auto unitGroup : army) {
        int typeId = unitGroup->unitTypeId;
        if (typeId == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) {
            typeId = BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode;
        }
        combatStats.bothAirDPF    += unitGroup->numUnits * (*maxDPF)[typeId].bothAir;
        combatStats.bothGroundDPF += unitGroup->numUnits * (*maxDPF)[typeId].bothGround;
        combatStats.airDPF        += unitGroup->numUnits * (*maxDPF)[typeId].air;
        combatStats.groundDPF     += unitGroup->numUnits * (*maxDPF)[typeId].ground;

        // if unit cannot attack, we don't consider its HP to "beat" (to exclude dummy buildings)
        BWAPI::UnitType unitType(typeId);
        if (unitType.canAttack() || unitType.isSpellcaster() || unitType.spaceProvided() > 0) {
            if (unitType.isFlyer()) { combatStats.airHP    += unitGroup->numUnits * unitGroup->HP; }
            else                    { combatStats.groundHP += unitGroup->numUnits * unitGroup->HP; }
        } else {
            if (unitType.isFlyer()) { combatStats.airHPextra    += unitGroup->numUnits * unitGroup->HP; }
            else                    { combatStats.groundHPextra += unitGroup->numUnits * unitGroup->HP; }
        }
    }
    return combatStats;
}

// © me & Alberto Uriarte
void CombatSimSustained::getCombatLength(const combatStats_t& friendStats, const combatStats_t& enemyStats)
{
    // groups that can attack ground and air help to kill the group that it takes more time to kill
    double timeToKillEnemyAir    = (enemyStats.airHP > 0) ? (friendStats.airDPF == 0) ? INT_MAX : enemyStats.airHP / friendStats.airDPF : 0;
    double timeToKillEnemyGround = (enemyStats.groundHP > 0) ? (friendStats.groundDPF == 0) ? INT_MAX : enemyStats.groundHP / friendStats.groundDPF : 0;
    if (friendStats.bothAirDPF > 0) {
        if (timeToKillEnemyAir > timeToKillEnemyGround) {
            double combinetDPF = friendStats.airDPF + friendStats.bothAirDPF;
            timeToKillEnemyAir = (enemyStats.airHP > 0) ? (combinetDPF == 0) ? INT_MAX : enemyStats.airHP / combinetDPF : 0;
        }
        else {
            double combinetDPF = friendStats.groundDPF + friendStats.bothGroundDPF;
            timeToKillEnemyGround = (enemyStats.groundHP > 0) ? (combinetDPF == 0) ? INT_MAX : enemyStats.groundHP / combinetDPF : 0;
        }
    }

    double timeToKillFriendAir    = (friendStats.airHP > 0) ? (enemyStats.airDPF == 0) ? INT_MAX : friendStats.airHP / enemyStats.airDPF : 0;
    double timeToKillFriendGround = (friendStats.groundHP > 0) ? (enemyStats.groundDPF == 0) ? INT_MAX : friendStats.groundHP / enemyStats.groundDPF : 0;
    if (enemyStats.bothAirDPF > 0) {
        if (timeToKillFriendAir > timeToKillEnemyGround) {
            double combinetDPF  = enemyStats.airDPF + enemyStats.bothAirDPF;
            timeToKillFriendAir = (friendStats.airHP > 0) ? (combinetDPF == 0) ? INT_MAX : friendStats.airHP / combinetDPF : 0;
        }
        else {
            double combinetDPF     = enemyStats.groundDPF + enemyStats.bothGroundDPF;
            timeToKillFriendGround = (friendStats.groundHP > 0) ? (combinetDPF == 0) ? INT_MAX : friendStats.groundHP / combinetDPF : 0;
        }
    }
    if (timeToKillEnemyAir == INT_MAX && timeToKillEnemyGround > 0) { timeToKillEnemy = timeToKillEnemyGround; }
    else if (timeToKillEnemyGround == INT_MAX && timeToKillEnemyAir > 0) { timeToKillEnemy = timeToKillEnemyAir; }
    else { timeToKillEnemy = std::max(timeToKillEnemyAir, timeToKillEnemyGround); }

    if (timeToKillFriendAir == INT_MAX && timeToKillFriendGround > 0) { timeToKillFriend = timeToKillFriendGround; }
    else if (timeToKillFriendGround == INT_MAX && timeToKillFriendAir > 0) { timeToKillFriend = timeToKillFriendAir; }
    else { timeToKillFriend = std::max(timeToKillFriendAir, timeToKillFriendGround); }
}

// © me & Alberto Uriarte
void CombatSimSustained::getExtraCombatLength(const combatStats_t& friendStats, const combatStats_t& enemyStats)
{
    // groups that can attack ground and air help to kill the group that it takes more time to kill
    double timeToKillEnemyAir = (enemyStats.airHPextra > 0) ? (friendStats.airDPF == 0) ? INT_MAX : enemyStats.airHPextra / friendStats.airDPF : 0;
    double timeToKillEnemyGround = (enemyStats.groundHPextra > 0) ? (friendStats.groundDPF == 0) ? INT_MAX : enemyStats.groundHPextra / friendStats.groundDPF : 0;
    if (friendStats.bothAirDPF > 0) {
        if (timeToKillEnemyAir > timeToKillEnemyGround) {
            double combinetDPF = friendStats.airDPF + friendStats.bothAirDPF;
            timeToKillEnemyAir = (enemyStats.airHPextra > 0) ? (combinetDPF == 0) ? INT_MAX : enemyStats.airHPextra / combinetDPF : 0;
        } else {
            double combinetDPF    = friendStats.groundDPF + friendStats.bothGroundDPF;
            timeToKillEnemyGround = (enemyStats.groundHPextra > 0) ? (combinetDPF == 0) ? INT_MAX : enemyStats.groundHPextra / combinetDPF : 0;
        }
    }
    double timeToKillFriendAir    = (friendStats.airHPextra > 0) ? (enemyStats.airDPF == 0) ? INT_MAX : friendStats.airHPextra / enemyStats.airDPF : 0;
    double timeToKillFriendGround = (friendStats.groundHPextra > 0) ? (enemyStats.groundDPF == 0) ? INT_MAX : friendStats.groundHPextra / enemyStats.groundDPF : 0;
    if (enemyStats.bothAirDPF > 0) {
        if (timeToKillFriendAir > timeToKillEnemyGround) {
            double combinetDPF  = enemyStats.airDPF + enemyStats.bothAirDPF;
            timeToKillFriendAir = (friendStats.airHPextra > 0) ? (combinetDPF == 0) ? INT_MAX : friendStats.airHPextra / combinetDPF : 0;
        } else {
            double combinetDPF     = enemyStats.groundDPF + enemyStats.bothGroundDPF;
            timeToKillFriendGround = (friendStats.groundHPextra > 0) ? (combinetDPF == 0) ? INT_MAX : friendStats.groundHPextra / combinetDPF : 0;
        }
    }
    if (timeToKillEnemyAir == INT_MAX && timeToKillEnemyGround > 0) { extraTimeToKillEnemy = timeToKillEnemyGround; }
    else if (timeToKillEnemyGround == INT_MAX && timeToKillEnemyAir > 0) { extraTimeToKillEnemy = timeToKillEnemyAir; }
    else { extraTimeToKillEnemy = std::max(timeToKillEnemyAir, timeToKillEnemyGround); }

    if (timeToKillFriendAir == INT_MAX && timeToKillFriendGround > 0) { extraTimeToKillFriend = timeToKillFriendGround; }
    else if (timeToKillFriendGround == INT_MAX && timeToKillFriendAir > 0) { extraTimeToKillFriend = timeToKillFriendAir; }
    else { extraTimeToKillFriend = std::max(timeToKillFriendAir, timeToKillFriendGround); }
}

// © me & Alberto Uriarte
int CombatSimSustained::getCombatLength(GameState::army_t* army)
{
    removeHarmlessIndestructibleUnits(army);
    combatStats_t friendStats = getCombatStats(army->friendly);
    combatStats_t enemyStats = getCombatStats(army->enemy);
    getCombatLength(friendStats, enemyStats);
    getExtraCombatLength(friendStats, enemyStats);
    if (timeToKillEnemy == INT_MAX || extraTimeToKillEnemy == INT_MAX) { timeToKillEnemy = INT_MAX; }
    else { timeToKillEnemy += extraTimeToKillEnemy; }
    if (timeToKillFriend == INT_MAX || extraTimeToKillFriend == INT_MAX) { timeToKillFriend = INT_MAX; }
    else { timeToKillFriend += extraTimeToKillFriend; }
    return std::min(timeToKillEnemy, timeToKillFriend);
}

// © me & Alberto Uriarte
void CombatSimSustained::simulateCombat(GameState::army_t* armyInCombat, GameState::army_t* army, int frames)
{
    removeHarmlessIndestructibleUnits(armyInCombat);
    if (!canSimulate(armyInCombat, army)) { return; }

    sortGroups(&armyInCombat->friendly, comparator1, &armyInCombat->enemy);
    sortGroups(&armyInCombat->enemy, comparator2, &armyInCombat->friendly);

    combatStats_t friendStats = getCombatStats(armyInCombat->friendly);
    combatStats_t enemyStats = getCombatStats(armyInCombat->enemy);

    // calculate end combat time
    getCombatLength(friendStats, enemyStats);
    int combatLength = std::min(timeToKillEnemy, timeToKillFriend);

    if (frames == 0 || frames >= combatLength) { // at least one army is destroyed
        if (timeToKillEnemy == timeToKillFriend) { // both armies are destroyed
            removeAllMilitaryGroups(&armyInCombat->friendly, &army->friendly);
            removeAllMilitaryGroups(&armyInCombat->enemy, &army->enemy);
        } else { // only one army is destroyed
            UnitGroupVector* loserUnits     = &army->friendly;
            UnitGroupVector* winnerUnits    = &army->enemy;
            UnitGroupVector* loserInCombat  = &armyInCombat->friendly;
            UnitGroupVector* winnerInCombat = &armyInCombat->enemy;
            combatStats_t* loserStats       = &friendStats;
            if (combatLength == timeToKillEnemy) {
                loserUnits     = &army->enemy;
                winnerUnits    = &army->friendly;
                loserInCombat  = &armyInCombat->enemy;
                winnerInCombat = &armyInCombat->friendly;
                loserStats     = &enemyStats;
            }
            removeAllGroups(loserInCombat, loserUnits);
            removeSomeUnitsFromArmy(winnerInCombat, winnerUnits, loserStats, combatLength);
        }
    } else { // both army still alive
        removeSomeUnitsFromArmy(&armyInCombat->friendly, &army->friendly, &enemyStats, frames);
        removeSomeUnitsFromArmy(&armyInCombat->enemy, &army->enemy, &friendStats, frames);
    }
}

// © me & Alberto Uriarte
void CombatSimSustained::removeSomeUnitsFromArmy(UnitGroupVector* winnerInCombat, UnitGroupVector* winnerUnits, combatStats_t* loserStats, double frames)
{
    // Calculate winner losses
    int totalAirDamage = loserStats->airDPF * frames;
    int totalGroundDamage = loserStats->groundDPF * frames;
    // in the case that we can deal both damages, we consider worst case
    int totalBothAirDamage = loserStats->bothAirDPF * frames;
    int totalBothGroundDamage = loserStats->bothGroundDPF * frames;

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
        if (totalAirDamage <= 0 && totalGroundDamage <= 0 && (totalBothAirDamage <= 0 || totalBothGroundDamage <= 0)) { break; }

        unitGroup_t* groupDamaged = *it;
        unitDeleted = false;
        BWAPI::UnitType unitType(groupDamaged->unitTypeId);
        int unitTypeHP = groupDamaged->HP;
        int groupHP    = groupDamaged->HP * groupDamaged->numUnits;
        // define type of damage to receive
        if (unitType.isFlyer()) {
            bothDamageInUse = &totalBothAirDamage;
            damageToDeal    = &totalBothAirDamage;
            usingBothDamage = true;
            if (totalAirDamage > 0) {
                damageToDeal    = &totalAirDamage;
                usingBothDamage = false;
            }
        } else { // unit is ground
            bothDamageInUse = &totalBothGroundDamage;
            damageToDeal    = &totalBothGroundDamage;
            usingBothDamage = true;
            if (totalGroundDamage > 0) {
                damageToDeal    = &totalGroundDamage;
                usingBothDamage = false;
            }
        }

        // compute losses
        if (*damageToDeal >= groupHP) {
            // kill all the units in the group
            unitDeleted    = removeGroup(groupDamaged, winnerUnits);
            *damageToDeal -= groupHP; // adjust damage left
        } else if (usingBothDamage) {
            // kill some units from the group
            unitsToRemove = (int)trunc((double)*damageToDeal / (double)unitTypeHP);
            if (unitsToRemove > 0) {
                groupDamaged->numUnits -= unitsToRemove;
                if (groupDamaged->numUnits == 0) { unitDeleted = removeGroup(groupDamaged, winnerUnits); }
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
                    if (groupDamaged->numUnits == 0) { unitDeleted = removeGroup(groupDamaged, winnerUnits); }
                    *bothDamageInUse -= unitsToRemove * unitTypeHP;
                } else { // the damage isn't enough to kill any unit (but it's absorbed)
                    *bothDamageInUse = 0;
                }
            }
        }
        if (unitDeleted) { it = winnerInCombat->erase(it); }
        else { ++it; }
    }
}
