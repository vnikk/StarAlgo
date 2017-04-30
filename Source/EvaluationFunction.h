#pragma once

#include "GameState.h"

class EvaluationFunction
{
public:
    // TODO Try to make this function "zero sum", i.e. everything that can add points to MAX player, can remove points
    // return the reward of a given state for a MAX player
    float evaluate(const GameState &gs);
};
