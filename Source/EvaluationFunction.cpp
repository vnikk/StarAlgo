#include "stdafx.h"
#include "EvaluationFunction.h"

float EvaluationFunction::evaluate(const GameState& gs) {
	float score1 = 0.0f;
	float score2 = 0.0f;

	for (const auto& unitGroup : gs._army.friendly) {
		score1 += BWAPI::UnitType(unitGroup->unitTypeId).destroyScore() * unitGroup->numUnits;
	}

	for (const auto& unitGroup : gs._army.enemy) {
		score2 += BWAPI::UnitType(unitGroup->unitTypeId).destroyScore() * unitGroup->numUnits;
	}

	// normalize score to [-1, 1]
	return ((2 * score1) / (score1 + score2)) - 1;
}
