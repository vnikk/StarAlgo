#pragma once

#include "GameState.h"

// © Alberto Uriarte
class EvaluationFunction
{
public:
    // return the reward of a given state for a MAX player
    float evaluate(const GameState &gs);
};
