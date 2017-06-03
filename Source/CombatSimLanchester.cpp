#include "stdafx.h"
#include "CombatSimLanchester.h"

// © Alberto Uriarte
CombatSimLanchester::CombatSimLanchester(std::vector<DPF_t>* maxDPF, comp_f comparator1, comp_f comparator2)
: winner(0), easyCombat(false)
{
    this->maxDPF = maxDPF;
    comparator1 = comparator1;
    comparator2 = comparator2;
}

// © me & Alberto Uriarte
CombatSimLanchester::combatStats_t CombatSimLanchester::getCombatStats(const UnitGroupVector &army)
{
    combatStats_t combatStats;

    for (auto unitGroup : army) {
        int typeId = unitGroup->unitTypeId;
        if (typeId == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) {
            typeId = BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode;
        }
        combatStats.bothAirDPF += unitGroup->numUnits * (*maxDPF)[typeId].bothAir;
        combatStats.bothGroundDPF += unitGroup->numUnits * (*maxDPF)[typeId].bothGround;
        combatStats.airDPF += unitGroup->numUnits * (*maxDPF)[typeId].air;
        combatStats.groundDPF += unitGroup->numUnits * (*maxDPF)[typeId].ground;

        // if unit cannot attack, we don't consider its HP to "beat" (to exclude dummy buildings)
        BWAPI::UnitType unitType(typeId);
        if (unitType.canAttack() || unitType.isSpellcaster() || unitType.spaceProvided() > 0) {
            if (unitType.isFlyer()) {
                combatStats.airHP += unitGroup->numUnits * unitGroup->HP;
                combatStats.airUnitsSize += unitGroup->numUnits;
            } else	{
                combatStats.groundHP += unitGroup->numUnits * unitGroup->HP;
                combatStats.groundUnitsSize += unitGroup->numUnits;
            }
        } else {
            if (unitType.isFlyer()) {
                combatStats.airHPextra += unitGroup->numUnits * unitGroup->HP;
                combatStats.airUnitsSizeExtra += unitGroup->numUnits;
            } else {
                combatStats.groundHPextra += unitGroup->numUnits * unitGroup->HP;
                combatStats.groundUnitsSizeExtra += unitGroup->numUnits;
            }
        }
    }

    return combatStats;
}

// © me & Alberto Uriarte
void CombatSimLanchester::getExtraCombatLength(const combatStats_t& friendStats, const combatStats_t& enemyStats)
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

    if (timeToKillEnemyAir == INT_MAX && timeToKillEnemyGround > 0) extraTimeToKillEnemy = (int)timeToKillEnemyGround;
    else if (timeToKillEnemyGround == INT_MAX && timeToKillEnemyAir > 0) extraTimeToKillEnemy = (int)timeToKillEnemyAir;
    else extraTimeToKillEnemy = std::max(timeToKillEnemyAir, timeToKillEnemyGround);

    if (timeToKillFriendAir == INT_MAX && timeToKillFriendGround > 0) extraTimeToKillFriend = (int)timeToKillFriendGround;
    else if (timeToKillFriendGround == INT_MAX && timeToKillFriendAir > 0) extraTimeToKillFriend = (int)timeToKillFriendAir;
    else extraTimeToKillFriend = std::max(timeToKillFriendAir, timeToKillFriendGround);
}

// © me & Alberto Uriarte

int CombatSimLanchester::getCombatLength(GameState::army_t* army)
{
    removeHarmlessIndestructibleUnits(army);
    friendStats = getCombatStats(army->friendly);
    enemyStats = getCombatStats(army->enemy);

    float avgEnemyAirHP     = enemyStats.airHP == 0.0f ? 0.0f : float(enemyStats.airHP) / float(enemyStats.airUnitsSize);
    float avgFriendAirHP    = friendStats.airHP == 0.0f ? 0.0f : float(friendStats.airHP) / float(friendStats.airUnitsSize);
    float avgEnemyGroundHP  = enemyStats.groundHP == 0.0f ? 0.0f : float(enemyStats.groundHP) / float(enemyStats.groundUnitsSize);
    float avgFriendGroundHP = friendStats.groundHP == 0.0f ? 0.0f : float(friendStats.groundHP) / float(friendStats.groundUnitsSize);

    float sumAvgEnemyHP  = avgEnemyAirHP + avgEnemyGroundHP;
    float sumAvgFriendHP = avgFriendAirHP + avgFriendGroundHP;

    lanchester.x0 = friendStats.airUnitsSize + friendStats.groundUnitsSize;
    lanchester.y0 = enemyStats.airUnitsSize + enemyStats.groundUnitsSize;

    float avgEnemyHP  = float((enemyStats.airHP + enemyStats.groundHP) / lanchester.y0);
    float avgFriendHP = float((friendStats.airHP + friendStats.groundHP) / lanchester.x0);

    // if unit cant attack both both{air|ground}DPF is populated, otherwise {air|ground}DPF is populated
    float avgEnemyAirDPF     = float((enemyStats.bothAirDPF + enemyStats.airDPF) / lanchester.y0);
    float avgEnemyGroundDPF  = float((enemyStats.bothGroundDPF + enemyStats.groundDPF) / lanchester.y0);
    float avgFriendAirDPF    = float((friendStats.bothAirDPF + friendStats.airDPF) / lanchester.x0);
    float avgFriendGroundDPF = float((friendStats.bothGroundDPF + friendStats.groundDPF) / lanchester.x0);

    float avgEnemyDPF = avgEnemyAirDPF * (avgFriendAirHP / sumAvgFriendHP) +
        avgEnemyGroundDPF * (avgFriendGroundHP / sumAvgFriendHP);
    float avgFriendDPF = avgFriendAirDPF * (avgEnemyAirHP / sumAvgEnemyHP) +
        avgFriendGroundDPF * (avgEnemyGroundHP / sumAvgEnemyHP);

    lanchester.a = avgEnemyDPF / avgFriendHP;
    lanchester.b = avgFriendDPF / avgEnemyHP;

    if (lanchester.a == 0 && lanchester.b == 0) { // combat is impossible
        winner = 0;
        combatLength = 0;
    } else if (lanchester.a == 0 && lanchester.b > 0) { // enemy cannot attack
        easyCombat = true;
        winner = 1;
        combatLength = static_cast<int>(lanchester.y0 / lanchester.b);
    } else if (lanchester.a > 0 && lanchester.b == 0) { // friendly cannot attack
        easyCombat = true;
        winner = 2;
        combatLength = static_cast<int>(lanchester.x0 / lanchester.a);
    } else {
        // use Lanchester's equations to predict winner and combatLength
        easyCombat = false;
        lanchester.I = sqrt(lanchester.a * lanchester.b);
        lanchester.R_a = sqrt(lanchester.a / lanchester.b);
        lanchester.R_b = sqrt(lanchester.b / lanchester.a);
        double combatLength = 0.0;
        if (lanchester.x0 / lanchester.y0 > lanchester.R_a) {
            // X (friendly) win
            winner = 1;
            double tmp = (lanchester.y0 / lanchester.x0) * lanchester.R_a;
            combatLength = (1 / (2 * lanchester.I)) * log((1 + tmp) / (1 - tmp));
        } else if (lanchester.x0 / lanchester.y0 < lanchester.R_a) {
            // Y (enemy) win 
            winner = 2;
            double tmp = (lanchester.x0 / lanchester.y0) * lanchester.R_b;
            combatLength = (1 / (2 * lanchester.I)) * log((1 + tmp) / (1 - tmp));
        } else {
            // draw
            winner = 3;
            combatLength = lanchester.x0 / lanchester.a; // used as a lower bound time
        }

        if (std::isnan(combatLength)) { // Trying to simulate a combat between armies that cannot kill each other
            winner = 0;
            combatLength = 0;
        } else {
            combatLength = static_cast<int>(std::ceil(combatLength));
        }
    }

    getExtraCombatLength(friendStats, enemyStats);
    switch (winner) {
    case 0: // combat impossible (look if we can destroy extra targets)
        if (extraTimeToKillEnemy) { winner = 1; return extraTimeToKillEnemy; }
        if (extraTimeToKillFriend) { winner = 2; return extraTimeToKillFriend; }
    case 1: // friendly wins
        return combatLength + extraTimeToKillEnemy;
    case 2: // friendly wins
        return combatLength + extraTimeToKillFriend;
    case 3: // draw
    default:
        return combatLength;
    }
}

// © me & Alberto Uriarte
void CombatSimLanchester::simulateCombat(GameState::army_t* armyInCombat, GameState::army_t* army, int frames)
{
    removeHarmlessIndestructibleUnits(armyInCombat);
    if (!canSimulate(armyInCombat, army)) return;

    // this will define the order to kill units from the set
    sortGroups(&armyInCombat->friendly, comparator1, &armyInCombat->enemy);
    sortGroups(&armyInCombat->enemy, comparator1, &armyInCombat->friendly);

    int combatLength = getCombatLength(armyInCombat); // it also precomputes all Lanchester parameters

    if (combatLength == 0) // combat impossible
        return;

    if (easyCombat) {
        if (frames >= combatLength || frames == 0) { //simulate until one army destroyed (fight-to-the-finish)
            if (winner == 1) { // friendly won
                removeAllGroups(&armyInCombat->enemy, &army->enemy);
            } else if (winner == 2) { // enemy won
                removeAllGroups(&armyInCombat->friendly, &army->friendly);
            } else {
                DEBUG("This should not happen");
            }
        } else { // partial combat
            int xkilled = static_cast<int>(std::floor(lanchester.a * frames));
            removeUnits(xkilled, armyInCombat->friendly, &army->friendly);
            int ykilled = static_cast<int>(std::floor(lanchester.b * frames));
            removeUnits(ykilled, armyInCombat->enemy, &army->enemy);
        }
    } else {
        if (frames >= combatLength || frames == 0) { //simulate until one army destroyed (fight-to-the-finish)
            if (winner == 1) { // friendly won
                removeAllGroups(&armyInCombat->enemy, &army->enemy);
                double numSurvivors = sqrt((lanchester.x0*lanchester.x0) - ((lanchester.a / lanchester.b)*(lanchester.y0*lanchester.y0)));
                int unitsToRemove = static_cast<int>(lanchester.x0 - std::ceil(numSurvivors));
                removeUnits(unitsToRemove, armyInCombat->friendly, &army->friendly);
            } else if (winner == 2) { // enemy won
                removeAllGroups(&armyInCombat->friendly, &army->friendly);
                double numSurvivors = sqrt((lanchester.y0*lanchester.y0) - ((lanchester.b / lanchester.a)*(lanchester.x0*lanchester.x0)));
                int unitsToRemove = static_cast<int>(lanchester.y0 - std::ceil(numSurvivors));
                removeUnits(unitsToRemove, armyInCombat->enemy, &army->enemy);
            } else if (winner == 3) { // draw
                removeAllMilitaryGroups(&armyInCombat->friendly, &army->friendly);
                removeAllMilitaryGroups(&armyInCombat->enemy, &army->enemy);
            } else {
                DEBUG("Unknown winner");
            }
        } else { // partial combat
            // first simulate combat between opposing forces
            int opposingCombatFrames = std::min(combatLength, frames);
            // cache some computations
            double eIt = std::exp(lanchester.I*opposingCombatFrames);
            double eMinIt = std::exp(-lanchester.I*opposingCombatFrames);
            // x survivors
            double numSurvivors = 0.5 * ((lanchester.x0 - lanchester.R_a * lanchester.y0)*eIt + (lanchester.x0 + lanchester.R_a * lanchester.y0)*eMinIt);
            int unitsToRemove = static_cast<int>(lanchester.x0 - std::ceil(numSurvivors));
            removeUnits(unitsToRemove, armyInCombat->friendly, &army->friendly);
            // y survivors
            numSurvivors = 0.5 * ((lanchester.y0 - lanchester.R_b * lanchester.x0)*eIt + (lanchester.y0 + lanchester.R_b * lanchester.x0)*eMinIt);
            unitsToRemove = static_cast<int>(lanchester.y0 - std::ceil(numSurvivors));
            removeUnits(unitsToRemove, armyInCombat->enemy, &army->enemy);
            if (frames > combatLength) {
                // now from the harmless survivors, compute how many we had time to kill
                int easyCombatFrames = frames - combatLength;
                getCombatLength(armyInCombat); // recompute lanchester parameters from survivors
                if (extraTimeToKillFriend) {
                    float killRatio = 1.0f - (((float)extraTimeToKillFriend - (float)easyCombatFrames) / (float)extraTimeToKillFriend);
                    float friendSize = float(friendStats.airUnitsSizeExtra + friendStats.groundUnitsSizeExtra);
                    int xkilled = static_cast<int>(std::floor(killRatio * friendSize));
                    removeUnits(xkilled, armyInCombat->friendly, &army->friendly);
                }
                if (extraTimeToKillEnemy) {
                    float killRatio = 1.0f - (((float)extraTimeToKillEnemy - (float)easyCombatFrames) / (float)extraTimeToKillEnemy);
                    float enemySize = float(enemyStats.airUnitsSizeExtra + enemyStats.groundUnitsSizeExtra);
                    int ykilled = static_cast<int>(std::floor(killRatio * enemySize));
                    removeUnits(ykilled, armyInCombat->enemy, &army->enemy);
                }
            }
        }
    }
}

// © Alberto Uriarte
void CombatSimLanchester::removeUnits(int numUnitsToRemove, UnitGroupVector &unitsInCombat, UnitGroupVector* generalList)
{
    if (numUnitsToRemove <= 0) { return; }
    for (auto it = unitsInCombat.begin(); it != unitsInCombat.end();) {
        if (numUnitsToRemove < (*it)->numUnits) {
            (*it)->numUnits -= numUnitsToRemove;
            break;
        } else {
            numUnitsToRemove -= (*it)->numUnits;
            removeGroup(*it, generalList);
            it = unitsInCombat.erase(it);
        }
    }
}
