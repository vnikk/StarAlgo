#include "stdafx.h"
#include "CombatSimDecreased.h"

// © Alberto Uriarte
CombatSimDecreased::CombatSimDecreased(std::vector<std::vector<double> >* unitTypeDPF, std::vector<DPF_t>* maxDPF, comp_f comparator1, comp_f comparator2)
: unitTypeDPF(unitTypeDPF)
{
    this->maxDPF = maxDPF;
    comparator1 = comparator1;
    comparator2 = comparator2;
}

// © me & Alberto Uriarte
void CombatSimDecreased::simulateCombat(GameState::army_t* armyInCombat, GameState::army_t* army, int frames)
{
    removeHarmlessIndestructibleUnits(armyInCombat);
    if (!canSimulate(armyInCombat, army)) { return; }

    // this will define the order to kill units from the set
    sortGroups(&armyInCombat->friendly, comparator1, &armyInCombat->enemy);
    sortGroups(&armyInCombat->enemy, comparator2, &armyInCombat->friendly);

    UnitGroupVector::iterator friendToKill = armyInCombat->friendly.begin();
    float friendHP = (*friendToKill)->HP;

    UnitGroupVector::iterator enemyToKill = armyInCombat->enemy.begin();
    float enemyHP = (*enemyToKill)->HP;

    double enemyDPF, friendDPF;
    bool armyDestroyed = false;
    double timeToKillFriend = DBL_MAX;
    double timeToKillEnemy  = DBL_MAX;

    double framesToSimulate = (frames)? frames : DBL_MAX;

    while (!armyDestroyed && framesToSimulate > 0) { // we end the loop when one army is empty or we cannot attack each other
        // calculate time to kill one unit of the target group
        if (enemyToKill != armyInCombat->enemy.end()) {
            timeToKillEnemy = getTimeToKillUnit(armyInCombat->friendly, (*enemyToKill)->unitTypeId, enemyHP, friendDPF);
        }
        else { timeToKillEnemy = DBL_MAX; }
        // check if we can attack the enemy
        while (timeToKillEnemy == DBL_MAX && enemyToKill != armyInCombat->enemy.end()) {
            ++enemyToKill;
            if (enemyToKill == armyInCombat->enemy.end()) { break; } // if it was the last unit, end the loop
            enemyHP = (*enemyToKill)->HP;
            timeToKillEnemy = getTimeToKillUnit(armyInCombat->friendly, (*enemyToKill)->unitTypeId, enemyHP, friendDPF);
        }

        if (friendToKill != armyInCombat->friendly.end()) {
            timeToKillFriend = getTimeToKillUnit(armyInCombat->enemy, (*friendToKill)->unitTypeId, friendHP, enemyDPF);
        }
        else { timeToKillFriend = DBL_MAX; }
        while (timeToKillFriend == DBL_MAX && friendToKill != armyInCombat->friendly.end()) {
            ++friendToKill;
            if (friendToKill == armyInCombat->friendly.end()) { break; } // if it was the last unit, end the loop
            friendHP = (*friendToKill)->HP;
            timeToKillFriend = getTimeToKillUnit(armyInCombat->enemy, (*friendToKill)->unitTypeId, friendHP, enemyDPF);
        }

        if (timeToKillEnemy == DBL_MAX && timeToKillFriend == DBL_MAX) {
            //Stopping combat because we cannot kill each other
            break;
        } else if (std::abs(timeToKillEnemy - timeToKillFriend) <= 0.001) { // draw
            // kill both units
            armyDestroyed  = killUnit(friendToKill, armyInCombat->friendly, &army->friendly, enemyHP, friendHP, timeToKillFriend, friendDPF);
            // friend was already killed
            armyDestroyed |= killUnit(enemyToKill, armyInCombat->enemy, &army->enemy, friendHP, enemyHP, timeToKillEnemy, 0.0);
            framesToSimulate -= timeToKillEnemy;
        } else if (timeToKillEnemy < timeToKillFriend) { // friend won
            armyDestroyed = killUnit(enemyToKill, armyInCombat->enemy, &army->enemy, friendHP, enemyHP, timeToKillEnemy, enemyDPF);
            framesToSimulate -= timeToKillFriend;
        } else if (timeToKillEnemy > timeToKillFriend) { // enemy won
            armyDestroyed = killUnit(friendToKill, armyInCombat->friendly, &army->friendly, enemyHP, friendHP, timeToKillFriend, friendDPF);
            framesToSimulate -= timeToKillEnemy;
        }
        if (!armyDestroyed && (enemyHP <= 0 || friendHP <= 0)) {
            DEBUG("This should not happen");
        }
    }
}

// © me & Alberto Uriarte
// return true if we need to end the combat (army empty)
bool CombatSimDecreased::killUnit(UnitGroupVector::iterator &unitToKill, UnitGroupVector &unitsInCombat, UnitGroupVector* unitsList, float &HPsurvivor, float &HPkilled, double timeToKill, double DPF)
{
    bool isArmyDestroyed = false;
    // remove unit killed
    (*unitToKill)->numUnits = (*unitToKill)->numUnits - 1;

    if ((*unitToKill)->numUnits == 0) { // if last unit in group
        // delete from general units list
        removeGroup(*unitToKill, unitsList);
        // delete and set new target (sequential from army vector)
        unitToKill      = unitsInCombat.erase(unitToKill);
        isArmyDestroyed = (unitToKill == unitsInCombat.end());
    }

    if (!isArmyDestroyed) {
        // reset HP of killed
        HPkilled = (*unitToKill)->HP;
        // reduce HP of survivor
        HPsurvivor -= float(timeToKill * DPF);
    }
    return isArmyDestroyed;
}

// © me & Alberto Uriarte
double CombatSimDecreased::getTimeToKillUnit(const UnitGroupVector &unitsInCombat, uint8_t enemyType, float enemyHP, double &DPF)
{
    DPF = 0.0;
    for (auto unitGroup : unitsInCombat) {
        int typeId = unitGroup->unitTypeId;
        // auto siege tanks
        if (typeId == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) {
            typeId = BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode;
        }
        if ((*unitTypeDPF)[typeId][enemyType] > 0) {
            DPF += unitGroup->numUnits * (*unitTypeDPF)[typeId][enemyType];
        }
    }
    return ((DPF == 0) ? DBL_MAX : (double)enemyHP / DPF);
}

// © me & Alberto Uriarte
CombatSimDecreased::combatStats_t CombatSimDecreased::getCombatStats(const UnitGroupVector &army)
{
    combatStats_t combatStats;

    for (auto unitGroup : army) {
        int typeId = unitGroup->unitTypeId;
        // auto siege tanks
        if (typeId == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) {
            typeId = BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode;
        }
        auto& maxTypeDPF = maxDPF->at(typeId);
        combatStats.airDPF        += unitGroup->numUnits * maxTypeDPF.air;
        combatStats.groundDPF     += unitGroup->numUnits * maxTypeDPF.ground;
        combatStats.bothAirDPF    += unitGroup->numUnits * maxTypeDPF.bothAir;
        combatStats.bothGroundDPF += unitGroup->numUnits * maxTypeDPF.bothGround;

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
void CombatSimDecreased::getCombatLength(const combatStats_t& friendStats, const combatStats_t& enemyStats)
{
    // groups that can attack ground and air help to kill the group that it takes more time to kill
    double timeToKillEnemyAir    = (enemyStats.airHP > 0) ? (friendStats.airDPF == 0) ? INT_MAX : enemyStats.airHP / friendStats.airDPF : 0;
    double timeToKillEnemyGround = (enemyStats.groundHP > 0) ? (friendStats.groundDPF == 0) ? INT_MAX : enemyStats.groundHP / friendStats.groundDPF : 0;
    if (friendStats.bothAirDPF > 0) {
        if (timeToKillEnemyAir > timeToKillEnemyGround) {
            double combinetDPF = friendStats.airDPF + friendStats.bothAirDPF;
            timeToKillEnemyAir = (enemyStats.airHP > 0) ? (combinetDPF == 0) ? INT_MAX : enemyStats.airHP / combinetDPF : 0;
        } else {
            double combinetDPF    = friendStats.groundDPF + friendStats.bothGroundDPF;
            timeToKillEnemyGround = (enemyStats.groundHP > 0) ? (combinetDPF == 0) ? INT_MAX : enemyStats.groundHP / combinetDPF : 0;
        }
    }

    double timeToKillFriendAir    = (friendStats.airHP > 0) ? (enemyStats.airDPF == 0) ? INT_MAX : friendStats.airHP / enemyStats.airDPF : 0;
    double timeToKillFriendGround = (friendStats.groundHP > 0) ? (enemyStats.groundDPF == 0) ? INT_MAX : friendStats.groundHP / enemyStats.groundDPF : 0;
    if (enemyStats.bothAirDPF > 0) {
        if (timeToKillFriendAir > timeToKillEnemyGround) {
            double combinetDPF  = enemyStats.airDPF + enemyStats.bothAirDPF;
            timeToKillFriendAir = (friendStats.airHP > 0) ? (combinetDPF == 0) ? INT_MAX : friendStats.airHP / combinetDPF : 0;
        } else {
            double combinetDPF     = enemyStats.groundDPF + enemyStats.bothGroundDPF;
            timeToKillFriendGround = (friendStats.groundHP > 0) ? (combinetDPF == 0) ? INT_MAX : friendStats.groundHP / combinetDPF : 0;
        }
    }

    if (timeToKillEnemyAir == INT_MAX && timeToKillEnemyGround > 0) timeToKillEnemy = timeToKillEnemyGround;
    else if (timeToKillEnemyGround == INT_MAX && timeToKillEnemyAir > 0) timeToKillEnemy = timeToKillEnemyAir;
    else timeToKillEnemy = std::max(timeToKillEnemyAir, timeToKillEnemyGround);

    if (timeToKillFriendAir == INT_MAX && timeToKillFriendGround > 0) timeToKillFriend = timeToKillFriendGround;
    else if (timeToKillFriendGround == INT_MAX && timeToKillFriendAir > 0) timeToKillFriend = timeToKillFriendAir;
    else timeToKillFriend = std::max(timeToKillFriendAir, timeToKillFriendGround);
}

// © me & Alberto Uriarte
void CombatSimDecreased::getExtraCombatLength(const combatStats_t& friendStats, const combatStats_t& enemyStats)
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

    if (timeToKillEnemyAir == INT_MAX && timeToKillEnemyGround > 0) extraTimeToKillEnemy = (int)timeToKillEnemyGround;
    else if (timeToKillEnemyGround == INT_MAX && timeToKillEnemyAir > 0) extraTimeToKillEnemy = (int)timeToKillEnemyAir;
    else extraTimeToKillEnemy = std::max(timeToKillEnemyAir, timeToKillEnemyGround);

    if (timeToKillFriendAir == INT_MAX && timeToKillFriendGround > 0) extraTimeToKillFriend = (int)timeToKillFriendGround;
    else if (timeToKillFriendGround == INT_MAX && timeToKillFriendAir > 0) extraTimeToKillFriend = (int)timeToKillFriendAir;
    else extraTimeToKillFriend = std::max(timeToKillFriendAir, timeToKillFriendGround);
}

// © me & Alberto Uriarte
int CombatSimDecreased::getCombatLength(GameState::army_t* army)
{
    removeHarmlessIndestructibleUnits(army);
    combatStats_t friendStats = getCombatStats(army->friendly);
    combatStats_t enemyStats = getCombatStats(army->enemy);
    getCombatLength(friendStats, enemyStats);
    getExtraCombatLength(friendStats, enemyStats);
    if (timeToKillEnemy == INT_MAX || extraTimeToKillEnemy == INT_MAX) timeToKillEnemy = INT_MAX;
    else timeToKillEnemy += extraTimeToKillEnemy;
    if (timeToKillFriend == INT_MAX || extraTimeToKillFriend == INT_MAX) timeToKillFriend = INT_MAX;
    else timeToKillFriend += extraTimeToKillFriend;
    return std::min(timeToKillEnemy, timeToKillFriend);
}
