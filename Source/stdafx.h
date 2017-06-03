// whole file © Alberto Uriarte
#pragma once

#define NOMINMAX

#include "AbstractGroup.h"
#include <BWAPI.h>
#include "targetver.h"
#include <windows.h>

#include <fstream>
#include <random>

//speeds up build
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

extern std::ofstream fileLog;

extern std::mt19937 gen;
extern std::string configPath;

extern unsigned int combatsSimulated;

#define DEBUG(Message) fileLog << __FILE__ ":" << __LINE__ << ": " << Message << std::endl
#define LOG(Message) fileLog << Message << std::endl

class UnitInfoStatic;
extern UnitInfoStatic* unitStatic;

bool isAggressiveSpellcaster(BWAPI::UnitType unitType);
bool isPassiveBuilding(const unitGroup_t* group);
bool canAttackAirUnits(BWAPI::UnitType unitType);
bool canAttackGroundUnits(BWAPI::UnitType unitType);
bool canAttackType(BWAPI::UnitType unitTypeAttacking, BWAPI::UnitType unitTypeTarget);

std::string LoadConfigString(const char *pszKey, const char *pszItem, const char *pszDefault);

std::string intToString(const int& number);

int LoadConfigInt(const char *pszKey, const char *pszItem, const int iDefault = 0);

