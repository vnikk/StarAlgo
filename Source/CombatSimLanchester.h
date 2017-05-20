// whole file © Alberto Uriarte
#pragma once
#include "CombatSimulator.h"

// Assumptions:
// Using Lanchester's Square Laws, i.e. all units of an army attack at the same time
// Not allowing reinforcements
// Army efficiency of heterogeneous army computed as the average (average air + average ground, units that can attack both attack simultaneously air and ground)
// On draw both armies are killed
// We don't consider individual HP, always using max
// We don't consider AOE damage
// Target selection from "comparator" function
// It NOT distinguish between flying units or not!!!

class CombatSimLanchester : public CombatSimulator
{
public:
    CombatSimLanchester(std::vector<DPF_t>* maxDPF, comp_f comparator1 = nullptr, comp_f comparator2 = nullptr);
    virtual ~CombatSimLanchester() {}
    CombatSimLanchester * clone() const { return new CombatSimLanchester(*this); } // Virtual constructor (copying)

    int getCombatLength(GameState::army_t* army);
    void simulateCombat(GameState::army_t* armyInCombat, GameState::army_t* army, int frames = 0);

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
        int groundUnitsSize;
        int airUnitsSize;
        int groundUnitsSizeExtra;
        int airUnitsSizeExtra;

        combatStats_t()
        : airDPF(0.0), groundDPF(0.0), bothAirDPF(0.0), bothGroundDPF(0.0), airHP(0.0), groundHP(0.0), airHPextra(0.0),
          groundHPextra(0.0), groundUnitsSize(0), airUnitsSize(0), groundUnitsSizeExtra(0), airUnitsSizeExtra(0)
        {}
    };

    // Lanchester parameters
    struct lanchester_t {
        double x0;	// number of allied units at time 0
        double y0;	// number of enemy units at time 0
        double a;	// number of allied units killed per frame per enemy unit
        double b;	// number of enemy units killed per frame per allied unit
        double I;	// intensity of the combat $\sqrt{ab}$
        double R_a;	// relative effectiveness for army X $\sqrt{\frac{a}{b}}$
        double R_b;	// relative effectiveness for army Y $\sqrt{\frac{b}{a}}$

        lanchester_t()
        : x0(0.0), y0(0.0), a(0.0), b(0.0), I(0.0), R_a(0.0), R_b(0.0)
        {}
    } lanchester;

    combatStats_t friendStats;
    combatStats_t enemyStats;

    int winner;
    int combatLength;
    bool easyCombat;

    inline void getExtraCombatLength(const combatStats_t& friendStats, const combatStats_t& enemyStats);
    combatStats_t getCombatStats(const UnitGroupVector &army);
    void removeUnits(int numUnitsToRemove, UnitGroupVector &unitsInCombat, UnitGroupVector* generalList);
};
