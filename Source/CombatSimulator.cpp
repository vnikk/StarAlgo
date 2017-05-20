#include "stdafx.h"
#include "CombatSimulator.h"

// © Alberto Uriarte
inline void CombatSimulator::removeAllMilitaryGroups(UnitGroupVector* groupsToRemove, UnitGroupVector* groups)
{
    for (const auto& groupToDelete : *groupsToRemove) {
        if (isPassiveBuilding(groupToDelete)) { break; }
        removeGroup(groupToDelete, groups);
    }
}

// © me & Alberto Uriarte
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

// © me & Alberto Uriarte
CombatSimulator::GroupDiversity CombatSimulator::getGroupDiversity(UnitGroupVector* groups)
{
    bool hasAirUnits    = false;
    bool hasGroundUnits = false;
    for (const auto& group : *groups) {
        BWAPI::UnitType gType(group->unitTypeId);
        if (gType.isFlyer()) { hasAirUnits = true; }
        else { hasGroundUnits = true; }
    }
    if (hasAirUnits && hasGroundUnits) { return GroupDiversity::BOTH; }
    if (hasAirUnits)                   { return GroupDiversity::AIR; }
    return GroupDiversity::GROUND;
}

// © me & Alberto Uriarte
void CombatSimulator::sortGroups(UnitGroupVector* groups, comp_f comparator, UnitGroupVector* attackers)
{
    if (groups->size() <= 1) { return; } // nothing to sort
    // sort by default (buildings and non-attacking units at the end)
    std::sort(groups->begin(), groups->end(), sortByBuilding);
    // find first passive building and sort until it
    auto passiveBuilding = std::find_if(groups->begin(), groups->end(), isPassiveBuilding);

    if (comparator == nullptr) {
        // by default use the learned priorities
        GroupDiversity groupDiversity = getGroupDiversity(attackers);
        switch (groupDiversity) {
            case AIR:     std::sort(groups->begin(), passiveBuilding, sortByTypeAir); break;
            case GROUND:  std::sort(groups->begin(), passiveBuilding, sortByTypeGround); break;
            case BOTH:    std::sort(groups->begin(), passiveBuilding, sortByTypeBoth); break;
        }
    }
    else {
        std::sort(groups->begin(), passiveBuilding, comparator);
    }
}

void CombatSimulator::removeHarmlessIndestructibleUnits(GameState::army_t* armyInCombat)
{
        bool friendlyCanAttackGround = false;
        bool friendlyCanAttackAir    = false;
        bool enemyCanAttackGround    = false;
        bool enemyCanAttackAir       = false;
        bool friendlyOnlyGroundUnits = true;
        bool friendlyOnlyAirUnits    = true;
        bool enemyOnlyGroundUnits    = true;
        bool enemyOnlyAirUnits       = true;
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
