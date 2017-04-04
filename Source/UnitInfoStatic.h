#pragma once

#include "CombatInfo.h"

// TODO spell casters have special DPS!!!!

class UnitInfoStatic
{
public:
	UnitInfoStatic();

	std::vector<std::vector<double> > typeDPF;
	std::vector<DPF_t> DPF;
	std::vector<HP_t> HP;
};
