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

extern std::mt19937 gen; // random number generator
extern std::string configPath; // TODO remove?

extern unsigned int combatsSimulated;

#define DEBUG(Message) fileLog << __FILE__ ":" << __LINE__ << ": " << Message << std::endl
#define LOG(Message) fileLog << Message << std::endl

class UnitInfoStatic; // move to TargetSorting.h?
//class InformationManager; // move to ActionGenerator.cpp?
//class BuildManager; // move to InformationManager.cpp?
extern UnitInfoStatic* unitStatic;
//extern InformationManager* informationManager;
//extern BuildManager* buildManager;

bool isAggressiveSpellcaster(BWAPI::UnitType unitType);

bool isPassiveBuilding(const unitGroup_t* group);

bool canAttackAirUnits(BWAPI::UnitType unitType);

bool canAttackGroundUnits(BWAPI::UnitType unitType);

bool canAttackType(BWAPI::UnitType unitTypeAttacking, BWAPI::UnitType unitTypeTarget);

std::string LoadConfigString(const char *pszKey, const char *pszItem, const char *pszDefault);

std::string intToString(const int& number);

/*
TODO copy nova config
void NovaAIModule::loadIniConfig()
{
TCHAR currentPath[MAX_PATH];
GetCurrentDirectory(MAX_PATH, currentPath);
configPath = std::string(currentPath) + "\\bwapi-data\\AI\\Nova.ini";

std::string highLevel = LoadConfigString("high_level_search", "high_level_search", "OFF");
if (highLevel == "ON") {
HIGH_LEVEL_SEARCH = true;
HIGH_LEVEL_REFRESH = LoadConfigInt("high_level_search", "refresh", 400);
SEARCH_ALGORITHM = LoadConfigString("high_level_search", "algorithm", "ABCD");
}

}
*/
