#pragma once

#include <vector>

//#include "Timer.h"
//#include "GameState.h"
//#include "ActionGenerator.h"
//#include "AbstractOrder.h"
#include "EvaluationFunction.h"
#include <BWAPI.h>
#include <limits.h>
#include <algorithm>

class SimpleGameNode {
public:
    std::vector<SimpleGameNode*> children;
    float heuristicValue;
    // SimpleGameNode* parent;
    SimpleGameNode()
    : heuristicValue(0)
    {}
};

class Compare {
public:
    bool operator()(playerActions_t a, playerActions_t b) {
        return 
    }
};

double evaluate(GameState& gs) {
    double score = 0.0f;

    for (const auto& unitGroup : gs._army.friendly) {
        score += BWAPI::UnitType(unitGroup->unitTypeId).destroyScore() * unitGroup->numUnits;
    }

    for (const auto& unitGroup : gs._army.enemy) {
        score -= BWAPI::UnitType(unitGroup->unitTypeId).destroyScore() * unitGroup->numUnits;
    }

    return score;
}

bool isTerminal(SimpleGameNode* node) {
    // GameState.gameover()
}

playerActions_t negamax(SimpleGameNode* node, int depth, short color = 1 /* to alternate results */) {
    if (depth == 0 || isTerminal(node))
        return color * evaluate(/*game state*/);

    playerActions_t bestValue = INT_MIN;
    for (auto child : node->children) {
        playerActions_t v = -negamax(child, depth - 1, -color)
        bestValue = std::max(bestValue, v, Compare());
    }
    return bestValue;
}