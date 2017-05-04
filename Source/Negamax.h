#pragma once


#include "GameNode.h"
//#include "Timer.h"
//#include "GameState.h"
#include "ActionGenerator.h"
//#include "AbstractOrder.h"
#include "EvaluationFunction.h"
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

double evaluate(GameState& gameState)
{
    // quiet region
    if (gameState._army.enemy.empty()) return 0;

    double score = 0.0f;

    for (const auto& unitGroup : gameState._army.friendly) {
        score += BWAPI::UnitType(unitGroup->unitTypeId).destroyScore() * unitGroup->numUnits;
    }
    for (const auto& unitGroup : gameState._army.enemy) {
        score -= BWAPI::UnitType(unitGroup->unitTypeId).destroyScore() * unitGroup->numUnits;
    }
    return score;
}

bool isTerminal(GameNode* node)
{
    // or enemy encountered
    return node->gameState.gameover();
}

// TODO not negamax
playerActions_t negamax(int depth)
{
    GameState gs; // TODO define simulator
    GameNode root(nullptr, gs); // newGameNode
    GameNode* node = search(&root, depth);
    // look into root's children, check which one has highest evaluation
    auto bestChild_it = std::max_element(root.children.begin(), root.children.end(),
        [] (GameNode* a, GameNode* b) {
            return a->totalEvaluation < b->totalEvaluation;
        });
    if (bestChild_it != root.children.end()) {
        return (*bestChild_it)->actions; // TODO GameNode to have just playerActions_t, not vector
    }
    return root.moveGenerator.getRandomAction();
}

// color alternates results of each level; 
GameNode* search(GameNode* node, int depth, short color = 1)
{
    if (depth == 0 || isTerminal(node)) { 
        //return color * evaluate(node->gameState);
        node->totalEvaluation = evaluate(node->gameState); // TODO mark with color?
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

    double bestValue = DBL_MIN;
    GameNode* bestNode;
    for (auto child : node->children) {
        GameNode* tmpNode = search(child, depth - 1, -color); // the depth limits the search like BFS does
        if (tmpNode->totalEvaluation < bestValue) {
            bestValue = tmpNode->totalEvaluation;
            bestNode = tmpNode;
        }
    }
    return bestNode;
}