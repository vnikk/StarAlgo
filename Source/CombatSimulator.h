#pragma once
#include <random>
#include <algorithm>

#include "GameState.h"
#include "CombatInfo.h"
#include "TargetSorting.h"

typedef std::vector<unitGroup_t*> UnitGroupVector;
typedef std::function<bool(const unitGroup_t* a, const unitGroup_t* b)> comp_f;

// © me & Alberto Uriarte
class CombatSimulator
{
public:
    virtual ~CombatSimulator() {}

    virtual CombatSimulator* clone() const = 0;  // Virtual constructor (copying)

    enum GroupDiversity { AIR, GROUND, BOTH };
    GroupDiversity getGroupDiversity(UnitGroupVector* groups);

    comp_f comparator1;
    comp_f comparator2;

    void sortGroups(UnitGroupVector* groups, comp_f comparator, UnitGroupVector* attackers);

    /// <summary>Retrieves the expected duration of the combat.</summary>
    /// <param name="army">Abstraction of enemy and self units.</param>
    /// <returns>The minimum time (in frames) required to destroy one of the armies.</returns>
    virtual int getCombatLength(GameState::army_t* army) = 0;

    /// <summary>Simulate a combat between army.enemy and army.friendly.</summary>
    /// <param name="armyInCombat">List of units involved in the combat.</param>
    /// <param name="army">Whole army where the units killed will be removed.</param>
    /// <param anme="frames">Number of frames to simulate (0 = until one army is destroyed).</param>
    virtual void simulateCombat(GameState::army_t* armyInCombat, GameState::army_t* army, int frames = 0) = 0;


    // © Alberto Uriarte
    /// <summary>Remove one goup from a list</summary>
    /// <param name="groupToRemove">Pointer of the group to be removed.</param>
    /// <param name="groups">Pointer of the list of groups from we will remove the groups.</param>
    /// <returns>True if the group was removed.</returns>
    bool removeGroup(unitGroup_t* groupToRemove, UnitGroupVector* groups)
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

    // © Alberto Uriarte
    /// <summary>Remove multiple goups from a list</summary>
    /// <param name="groupsToRemove">List of pointer of groups to be removed.</param>
    /// <param name="groups">Pointer of the list of groups from we will remove the groups.</param>
    void removeAllGroups(UnitGroupVector* groupsToRemove, UnitGroupVector* groups)
    {
        for (const auto& groupToDelete : *groupsToRemove) removeGroup(groupToDelete, groups);
        groupsToRemove->clear();
    }

    /// <summary>Remove multiple military goups from a list. It assumes that the list is sorted
    /// (not military units at the end)</summary>
    /// <param name="groupsToRemove">List of pointer of groups to be removed.</param>
    /// <param name="groups">Pointer of the list of groups from we will remove the groups.</param>
    void removeAllMilitaryGroups(UnitGroupVector* groupsToRemove, UnitGroupVector* groups);


    /// <summary>Check if the combat can be simulated</summary>
    /// <param name="armyInCombat">List of units involved in the combat.</param>
    /// <param name="army">Whole army (all units in armyInCombat should appear here).</param>
    /// <returns>True if the combat can be simulated.</returns>
    bool canSimulate(GameState::army_t* armyInCombat, GameState::army_t* army);

    // © me & Alberto Uriarte
    /// Remove Harmless indestructible units
    void removeHarmlessIndestructibleUnits(GameState::army_t* armyInCombat);

protected:
    std::vector<DPF_t>* maxDPF;

    int timeToKillEnemy;
    int timeToKillFriend;
    int extraTimeToKillEnemy;
    int extraTimeToKillFriend;
};
