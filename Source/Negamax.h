#pragma once


#include "GameNode.h"
//#include "Timer.h"
//#include "ActionGenerator.h"
//#include "AbstractOrder.h"
#include <BWAPI.h>
#include <limits.h>
#include <algorithm>
#include <vector>

/*
class SimpleGameNode
{
public:
    playerActions_t actions; // action of current node
    double actionsProbability;
    //std::vector<playerActions_t> actions; // action of each child, TODO make one 
    std::vector<SimpleGameNode*> children;
    double heuristicValue;
    GameState gameState;
    ActionGenerator moveGenerator;

    parent
    visits
    dapth
    playerTurn
    nextPlayerInSimultaneousNode

    // SimpleGameNode* parent;
    SimpleGameNode()
    : heuristicValue(0)
    {}

    SimpleGameNode* createChild(playerActions_t actions);
};
SimpleGameNode* SimpleGameNode::createChild(playerActions_t actions) {
    if (!actions.empty()) {
        GameState gs2 = gameState.cloneIssue(actions, moveGenerator._player);
        SimpleGameNode* newChild = newGameNode(gs2, this);
        newChild->actions = actions;
        children.push_back(newChild);
        return newChild;
    }
    else {
        printNodeError("Error generating action for first child");
        return this;
    }
}
*/

int evaluate(GameState& gameState) // TODO check units only in regions, we are present in!
{
    // quiet region
    if (gameState._army.enemy.empty()) return 0;

    int score = 0.0f;

    for (const auto& unitGroup : gameState._army.friendly) {
        score += BWAPI::UnitType(unitGroup->unitTypeId).destroyScore() * unitGroup->numUnits;
    }
    for (const auto& unitGroup : gameState._army.enemy) {
        score -= BWAPI::UnitType(unitGroup->unitTypeId).destroyScore() * unitGroup->numUnits;
    }
    return score;
}

GameNode* search(GameNode* node, int depth/*, short color = 1*/)// color alternates results of each level; 
{
    if (depth == 0 || node->isTerminal()) { 
        node->totalEvaluation = evaluate(node->gameState); // TODO mark with color?
        return node;
    }

    if (node->children.empty()) {
        double prob;
        while (node->moveGenerator.hasMoreActions()) {
            playerActions_t actions = node->moveGenerator.getNextActionProbability(prob); // TODO what actions
            GameNode* child = node->createChild(actions);
            child->actionsProbability = prob;
        }
    }

    int bestValue = DBL_MIN;
    GameNode* bestNode;
    for (auto child : node->children) {
        GameNode* tmpNode = search(child, depth - 1/*, -color*/); // the depth limits the search like BFS does
        if (tmpNode->totalEvaluation < bestValue) {
            bestValue = tmpNode->totalEvaluation;
            bestNode = tmpNode;
        }
    }
    return bestNode;
}

playerActions_t negamax(GameState gs, int depth)
{
    GameNode root(nullptr, gs); // newGameNode
    return search(&root, depth)->actions;
    //return root.moveGenerator.getRandomAction();
}
