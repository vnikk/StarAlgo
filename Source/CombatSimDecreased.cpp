#include "stdafx.h"
#include "CombatSimDecreased.h"

CombatSimDecreased::CombatSimDecreased(std::vector<std::vector<double> >* unitTypeDPF, std::vector<DPF_t>* maxDPF, comp_f comparator1, comp_f comparator2)
    :_unitTypeDPF(unitTypeDPF),
    _maxDPF(maxDPF)
{
    _comparator1 = comparator1;
    _comparator2 = comparator2;
}

void CombatSimDecreased::simulateCombat(GameState::army_t* armyInCombat, GameState::army_t* army, int frames)
{
    removeHarmlessIndestructibleUnits(armyInCombat);
    if (!canSimulate(armyInCombat, army)) return;

    // this will define the order to kill units from the set
    sortGroups(&armyInCombat->friendly, _comparator1, &armyInCombat->enemy);
    sortGroups(&armyInCombat->enemy, _comparator2, &armyInCombat->friendly);

    UnitGroupVector::iterator friendToKill = armyInCombat->friendly.begin();
    float friendHP = (*friendToKill)->HP;

    UnitGroupVector::iterator enemyToKill = armyInCombat->enemy.begin();
    float enemyHP = (*enemyToKill)->HP;

    double enemyDPF, friendDPF;
    bool armyDestroyed = false;
    double timeToKillFriend = DBL_MAX;
    double timeToKillEnemy = DBL_MAX;

    double framesToSimulate = (frames)? (double)frames : DBL_MAX;

    while (!armyDestroyed && framesToSimulate > 0) { // we end the loop when one army is empty or we cannot attack each other
        // calculate time to kill one unit of the target group
        if (enemyToKill != armyInCombat->enemy.end())
            timeToKillEnemy = getTimeToKillUnit(armyInCombat->friendly, (*enemyToKill)->unitTypeId, enemyHP, friendDPF);
        else timeToKillEnemy = DBL_MAX;
        // check if we can attack the enemy
        while (timeToKillEnemy == DBL_MAX && enemyToKill != armyInCombat->enemy.end()) {
            ++enemyToKill;
            if (enemyToKill == armyInCombat->enemy.end()) break; // if it was the last unit, end the loop
            enemyHP = (*enemyToKill)->HP;
            timeToKillEnemy = getTimeToKillUnit(armyInCombat->friendly, (*enemyToKill)->unitTypeId, enemyHP, friendDPF);
        }

        if (friendToKill != armyInCombat->friendly.end())
            timeToKillFriend = getTimeToKillUnit(armyInCombat->enemy, (*friendToKill)->unitTypeId, friendHP, enemyDPF);
        else timeToKillFriend = DBL_MAX;
        while (timeToKillFriend == DBL_MAX && friendToKill != armyInCombat->friendly.end()) {
            ++friendToKill;
            if (friendToKill == armyInCombat->friendly.end()) break; // if it was the last unit, end the loop
            friendHP = (*friendToKill)->HP;
            timeToKillFriend = getTimeToKillUnit(armyInCombat->enemy, (*friendToKill)->unitTypeId, friendHP, enemyDPF);
        }

//         if (timeToKillFriend == DBL_MAX)  LOG("Imposible to kill friend");
//         else LOG("Time to kill friend (" << BWAPI::UnitType((*friendToKill)->unitTypeId).getName() << " HP: " << friendHP << "): " << timeToKillFriend);
//         if (timeToKillEnemy == DBL_MAX)  LOG("Imposible to kill enemy");
//         else LOG("Time to kill enemy  (" << BWAPI::UnitType((*enemyToKill)->unitTypeId).getName() << " HP: " << enemyHP << "): " << timeToKillEnemy);
        
        if (timeToKillEnemy == DBL_MAX && timeToKillFriend == DBL_MAX) {
//             LOG("Stopping combat because we cannot kill each other");
            break;
        } else if (std::abs(timeToKillEnemy - timeToKillFriend) <= 0.001) { // draw
            // kill both units
            armyDestroyed = killUnit(friendToKill, armyInCombat->friendly, &army->friendly, enemyHP, friendHP, timeToKillFriend, friendDPF);
            armyDestroyed |= killUnit(enemyToKill, armyInCombat->enemy, &army->enemy, friendHP, enemyHP, timeToKillEnemy, 0.0); // friend was already killed
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
        unitToKill = unitsInCombat.erase(unitToKill);
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

double CombatSimDecreased::getTimeToKillUnit(const UnitGroupVector &unitsInCombat, uint8_t enemyType, float enemyHP, double &DPF)
{
    DPF = 0.0;
    for (auto unitGroup : unitsInCombat) {
        int typeId = unitGroup->unitTypeId;
        // auto siege tanks TODO only if researched!!
        if (typeId == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) {
            typeId = BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode;
        }
        if ((*_unitTypeDPF)[typeId][enemyType] > 0) {
            DPF += unitGroup->numUnits * (*_unitTypeDPF)[typeId][enemyType];
        }
    }

    double timeToKillUnit = (DPF == 0) ? DBL_MAX : (double)enemyHP / DPF;
//     return std::round(timeToKillUnit);
    return timeToKillUnit;
}

CombatSimDecreased::combatStats_t CombatSimDecreased::getCombatStats(const UnitGroupVector &army)
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
        // TODO rethink this!!! (but this doesn't affect to simulation)
        BWAPI::UnitType unitType(typeId);
        if (unitType.canAttack() || unitType.isSpellcaster() || unitType.spaceProvided() > 0) {
            if (unitType.isFlyer()) combatStats.airHP += unitGroup->numUnits * unitGroup->HP;
            else                    combatStats.groundHP += unitGroup->numUnits * unitGroup->HP;
        } else {
            if (unitType.isFlyer()) combatStats.airHPextra += unitGroup->numUnits * unitGroup->HP;
            else                    combatStats.groundHPextra += unitGroup->numUnits * unitGroup->HP;
        }
    }

    return combatStats;
}


void CombatSimDecreased::getCombatLength(combatStats_t friendStats, combatStats_t enemyStats)
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

void CombatSimDecreased::getExtraCombatLength(combatStats_t friendStats, combatStats_t enemyStats)
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

int CombatSimDecreased::getCombatLength(GameState::army_t* army)
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
