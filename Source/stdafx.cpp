#include "stdafx.h"
#include "InformationManager.h"
#include <windows.h>
#include <winbase.h>

#define WINAPI_PARTITION_DESKTOP 1

static std::mt19937 initGenerator() {
	std::random_device rd; // seed
	return std::mt19937(rd());
}

std::ofstream fileLog;

std::mt19937 gen = initGenerator(); // random number generator
std::string configPath; // TODO remove?

unsigned int combatsSimulated = 0;

bool isAggressiveSpellcaster(BWAPI::UnitType unitType)
{
	return unitType.isSpellcaster() &&
		unitType != BWAPI::UnitTypes::Terran_Medic &&
		unitType != BWAPI::UnitTypes::Terran_Science_Vessel &&
		unitType != BWAPI::UnitTypes::Protoss_Dark_Archon && // although, Feedback spell damage shielded units
		unitType != BWAPI::UnitTypes::Zerg_Queen; // although, Spawn Broodlings spell insta-kill a unit
}

bool canAttackAirUnits(BWAPI::UnitType unitType)
{
	return unitType.airWeapon() != BWAPI::WeaponTypes::None ||
		isAggressiveSpellcaster(unitType) ||
		unitType == BWAPI::UnitTypes::Terran_Bunker;
}

bool canAttackGroundUnits(BWAPI::UnitType unitType)
{
	return unitType.groundWeapon() != BWAPI::WeaponTypes::None ||
		isAggressiveSpellcaster(unitType) ||
		unitType == BWAPI::UnitTypes::Terran_Bunker;
}

bool canAttackType(BWAPI::UnitType unitTypeAttacking, BWAPI::UnitType unitTypeTarget)
{
	if (unitTypeTarget.isFlyer()) return canAttackAirUnits(unitTypeAttacking);
	else return canAttackGroundUnits(unitTypeAttacking);
}

std::string LoadConfigString(const char *pszKey, const char *pszItem, const char *pszDefault)
{
	char buffer[MAX_PATH];
	GetPrivateProfileStringA(pszKey, pszItem, pszDefault ? pszDefault : "", buffer, MAX_PATH, configPath.c_str());
	return std::string(buffer);
}

std::string intToString(const int& number)
{
	std::ostringstream oss;
	oss << number;
	return oss.str();
}
