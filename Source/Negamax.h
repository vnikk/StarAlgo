#pragma once

#include "GameNode.h"
#include <BWAPI.h>
#include <limits.h>
#include <algorithm>
#include <vector>

// heuristic function
int evaluate(GameState& gameState)
{
    // quiet region
    if (gameState.army.enemy.empty()) { return 0; }

    int score = 0.0f;
    for (const auto& unitGroup : gameState.army.friendly) {
        score += BWAPI::UnitType(unitGroup->unitTypeId).destroyScore() * unitGroup->numUnits;
    }
    for (const auto& unitGroup : gameState.army.enemy) {
        score -= BWAPI::UnitType(unitGroup->unitTypeId).destroyScore() * unitGroup->numUnits;
    }
    return score;
}

// recursive part of Negamax
GameNode* search(GameNode* node, int depth)
{
    if (depth == 0 || node->isTerminal()) { 
        node->totalEvaluation = evaluate(node->gameState);
        return node;
    }
    if (node->children.empty()) {
        double prob;
        while (node->moveGenerator.hasMoreActions()) {
            playerActions_t actions = node->moveGenerator.getNextActionProbability(prob);
            GameNode* child = node->createChild(actions);
            child->actionsProbability = prob;
        }
    }
    int bestValue = DBL_MIN;
    GameNode* bestNode;
    for (auto child : node->children) {
        GameNode* tmpNode = search(child, depth - 1);
        if (tmpNode->totalEvaluation < bestValue) {
            bestValue = tmpNode->totalEvaluation;
            bestNode = tmpNode;
        }
    }
    return bestNode;
}

playerActions_t negamax(GameState gs, int depth)
{
    GameNode root(nullptr, gs);
    return search(&root, depth)->actions;
}
