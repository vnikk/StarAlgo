// whole file © Alberto Uriarte
#pragma once
#include "CombatSimulator.h"

// HP details:
//    - using max HP and max shield
//  - not considering armor (neither upgrades)
// DPS details:
//    - not considering AOE damage
//    - not considering damage type
//  - not considering upgrades (armor, weapon)
// Each army produce full DPS through all the combat (we don't remove DPS on unit destroy)
// We remove units from the survivor army from their order in the vector (which is arbitrary)

class CombatSimSustained : public CombatSimulator
{
public:
    CombatSimSustained(std::vector<DPF_t>* maxDPF, comp_f comparator1 = nullptr, comp_f comparator2 = nullptr);
    virtual ~CombatSimSustained() {}
    virtual CombatSimulator* clone() const { return new CombatSimSustained(*this); } // Virtual constructor (copying)

    virtual int getCombatLength(GameState::army_t* army);
    virtual void simulateCombat(GameState::army_t* armyInCombat, GameState::army_t* army, int frames = 0);

private:
    struct combatStats_t {
        double airDPF; // DPF = Damage Per Frame
        double groundDPF;
        double bothAirDPF;
        double bothGroundDPF;
        double airHP;
        double groundHP;
        double airHPextra;
        double groundHPextra;
        combatStats_t() :airDPF(0.0), groundDPF(0.0), bothAirDPF(0.0), bothGroundDPF(0.0),
            airHP(0.0), groundHP(0.0), airHPextra(0.0), groundHPextra(0.0){}
    };

    void getCombatLength(const combatStats_t& friendStats, const combatStats_t& enemyStats);
    void getExtraCombatLength(const combatStats_t& friendStats, const combatStats_t& enemyStats);
    combatStats_t getCombatStats(const UnitGroupVector &army);
    void removeSomeUnitsFromArmy(UnitGroupVector* winnerInCombat, UnitGroupVector* winnerUnits, combatStats_t* loserStats, double frames);
};
