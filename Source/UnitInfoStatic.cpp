#include "stdafx.h"
#include "UnitInfoStatic.h"

// © me & Alberto Uriarte
UnitInfoStatic::UnitInfoStatic()
{
    // cache for unitTypeDPF from BWAPI
    double DPFval;
    typeDPF.resize(MAX_UNIT_TYPE);
    for (int i = 0; i < MAX_UNIT_TYPE; ++i) {
        typeDPF[i].resize(MAX_UNIT_TYPE);
        for (int j = 0; j < MAX_UNIT_TYPE; ++j) {
            BWAPI::UnitType unitType(i);
            if (i == BWAPI::UnitTypes::Terran_Bunker) unitType = BWAPI::UnitTypes::Terran_Marine; // consider Bunkers like 4 Marines
            if (i == BWAPI::UnitTypes::Protoss_Reaver) unitType = BWAPI::UnitTypes::Protoss_Scarab; // Reavers "shots" Scarabs
            if (i == BWAPI::UnitTypes::Protoss_Carrier) unitType = BWAPI::UnitTypes::Protoss_Interceptor; // Carrier "shots" Interceptors

            BWAPI::UnitType enemyType(j);
            DPFval = 0.0;

            auto& groundWeapon = unitType.groundWeapon();
            auto& airWeapon = unitType.airWeapon();
            if (!enemyType.isFlyer() && groundWeapon.damageAmount() > 0) {
                DPFval = static_cast<double>(groundWeapon.damageAmount() * groundWeapon.damageFactor() * unitType.maxGroundHits());
                if (groundWeapon.damageType() == BWAPI::DamageTypes::Concussive) {
                    if (enemyType.size() == BWAPI::UnitSizeTypes::Large) DPFval *= 0.25;
                    else if (enemyType.size() == BWAPI::UnitSizeTypes::Medium) DPFval *= 0.5;
                } else if (groundWeapon.damageType() == BWAPI::DamageTypes::Explosive) {
                    if (enemyType.size() == BWAPI::UnitSizeTypes::Small) DPFval *= 0.5;
                    else if (enemyType.size() == BWAPI::UnitSizeTypes::Medium) DPFval *= 0.75;
                }

                if (i == BWAPI::UnitTypes::Terran_Bunker)		 DPFval = (DPFval * 4.0) / groundWeapon.damageCooldown(); // consider Bunkers like 4 Marines
                else if (i == BWAPI::UnitTypes::Protoss_Reaver)  DPFval = DPFval / 60.0; // launches scarabs every 60 frames
                else if (i == BWAPI::UnitTypes::Protoss_Carrier) DPFval = (DPFval * 8.0) / 30.0; // can launch up to 8 interceptors each 30 frames
                else											 DPFval = (DPFval / groundWeapon.damageCooldown());

            } else if (enemyType.isFlyer() && airWeapon.damageAmount() > 0) {
                DPFval = static_cast<double>(airWeapon.damageAmount() * airWeapon.damageFactor() * unitType.maxAirHits());
                if (airWeapon.damageType() == BWAPI::DamageTypes::Concussive) {
                    if (enemyType.size() == BWAPI::UnitSizeTypes::Large) DPFval *= 0.25;
                    else if (enemyType.size() == BWAPI::UnitSizeTypes::Medium) DPFval *= 0.5;
                } else if (airWeapon.damageType() == BWAPI::DamageTypes::Explosive) {
                    if (enemyType.size() == BWAPI::UnitSizeTypes::Small) DPFval *= 0.25;
                    else if (enemyType.size() == BWAPI::UnitSizeTypes::Medium) DPFval *= 0.5;
                }

                if (i == BWAPI::UnitTypes::Terran_Bunker)		 DPFval = (DPFval * 4.0) / airWeapon.damageCooldown(); // consider Bunkers like 4 Marines
                else if (i == BWAPI::UnitTypes::Protoss_Carrier) DPFval = (DPFval * 8.0) / 30.0; // can launch up to 8 interceptors each 30 frames
                else											 DPFval = DPFval / airWeapon.damageCooldown();
            }
            typeDPF[i][j] = DPFval;
        }
    }

    // cache for unit DPF and max HP from BWAPI
    DPF.resize(MAX_UNIT_TYPE);
    HP.resize(MAX_UNIT_TYPE);
    for (int i = 0; i < MAX_UNIT_TYPE; ++i) {
        BWAPI::UnitType unitType(i);
        if (i == BWAPI::UnitTypes::Terran_Bunker) unitType = BWAPI::UnitTypes::Terran_Marine; // consider Bunkers like 4 Marines
        if (i == BWAPI::UnitTypes::Protoss_Reaver) unitType = BWAPI::UnitTypes::Protoss_Scarab; // Reavers "shot" Scarabs
        if (i == BWAPI::UnitTypes::Protoss_Carrier) unitType = BWAPI::UnitTypes::Protoss_Interceptor; // Carrier "shots" Interceptors

        double groundDamage = 0.0;
        double airDamage = 0.0;

        if (unitType.groundWeapon().damageAmount() > 0) {
            int damage = unitType.groundWeapon().damageAmount() * unitType.groundWeapon().damageFactor() * unitType.maxGroundHits();

            if (i == BWAPI::UnitTypes::Terran_Bunker)		 groundDamage = (damage*4.0) / (double)unitType.groundWeapon().damageCooldown(); // consider Bunkers like 4 Marines
            else if (i == BWAPI::UnitTypes::Protoss_Reaver)  groundDamage = damage / 60.0; // launches scarabs every 60 frames
            else if (i == BWAPI::UnitTypes::Protoss_Carrier) groundDamage = (damage * 8.0) / 30.0; // can launch up to 8 interceptors each 30 frames
            else											 groundDamage = damage / (double)unitType.groundWeapon().damageCooldown();
        }
        if (unitType.airWeapon().damageAmount() > 0) {
            int damage = unitType.airWeapon().damageAmount() * unitType.airWeapon().damageFactor() * unitType.maxAirHits();
            
            if (i == BWAPI::UnitTypes::Terran_Bunker)		 airDamage = (damage*4.0) / (double)unitType.groundWeapon().damageCooldown(); // consider Bunkers like 4 Marines
            else if (i == BWAPI::UnitTypes::Protoss_Carrier) airDamage = (damage * 8.0) / 30.0; // can launch up to 8 interceptors each 30 frames
            else											 airDamage = damage / (double)unitType.airWeapon().damageCooldown();
        }

        if (unitType.airWeapon().damageAmount() > 0 && unitType.groundWeapon().damageAmount() > 0) {
            DPF[i].bothGround = groundDamage;
            DPF[i].bothAir = airDamage;
        } else {
            DPF[i].ground = groundDamage;
            DPF[i].air = airDamage;
        }

        HP[i].any = unitType.maxShields() + unitType.maxHitPoints();
        if (unitType.isFlyer()) { HP[i].air = HP[i].any; }
        else                    { HP[i].ground = HP[i].any; }
    }
}