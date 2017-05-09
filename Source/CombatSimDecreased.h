#pragma once
#include "CombatSimulator.h"

// We compute the DPS and the HP as a whole for each army
// We don't consider individual HP, always using max
// We don't consider AOE damage
// We remove unit during combat, when a unit is removed, we update the overall new DPS and HP
// The target selection is sequential from the vector (i.e. the order is arbitrary)
// The minimum time to kill a unit is at least the minimum cooldown in the army group (this has been disable!! problems with both units killed)
// We don't allow "multi-attack", i.e. :
//  1 - all units in the army focus their attacks to the same target
//  2 - if you cannot attack a target (flying?) but you can attack another, you lost your turn to attack
// We don't consider upgrades for DPS
// TODO combat time is from CombatSimulatorBASIC (method 1)
//   - to get the combat length we actually need to simulate the combat: store the result of the combat to avoid re-simulate

class CombatSimDecreased : public CombatSimulator
{
public:
    CombatSimDecreased(std::vector<std::vector<double> >* unitTypeDPF, std::vector<DPF_t>* _maxDPF, comp_f comparator1 = nullptr, comp_f comparator2 = nullptr);
    virtual ~CombatSimDecreased() {}
    virtual CombatSimulator* clone() const override { return new CombatSimDecreased(*this); } // Virtual constructor (copying) 

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
        combatStats_t() :airDPF(0.0), groundDPF(0.0), bothAirDPF(0.0), bothGroundDPF(0.0),
            airHP(0.0), groundHP(0.0), airHPextra(0.0), groundHPextra(0.0){}
    };

    std::vector<std::vector<double> >* _unitTypeDPF;
    std::vector<DPF_t>* _maxDPF;

    int _timeToKillEnemy;
    int _timeToKillFriend;
    int _extraTimeToKillEnemy;
    int _extraTimeToKillFriend;

    void getCombatLength(combatStats_t friendStats, combatStats_t enemyStats);
    void getExtraCombatLength(combatStats_t friendStats, combatStats_t enemyStats);
    combatStats_t getCombatStats(const UnitGroupVector &army);

    double getTimeToKillUnit(const UnitGroupVector &unitsInCombat, uint8_t enemyType, float enemyHP, double &DPF);
    bool killUnit(UnitGroupVector::iterator &unitToKill, UnitGroupVector &unitsInCombat, UnitGroupVector* unitsList, float &HPsurvivor, float &HPkilled, double timeToKill, double DPF);
};
