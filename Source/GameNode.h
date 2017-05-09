#pragma once

#include "GameState.h"
#include "ActionGenerator.h"
#include <vector>

class GameState;

class GameNode {
public:
    playerActions_t actions; // actions of node
    //std::vector<playerActions_t> actions; // action of each child
    double totalEvaluation;
    double totalVisits;
    GameNode* parent; // to back propagate the stats
    std::vector<GameNode*> children; // to delete the tree
    GameState gameState;
    double depth;
    int playerTurn; // 1 : max, 0 : min, -1: Game-over
    ActionGenerator moveGenerator;
    int nextPlayerInSimultaneousNode;
    double actionsProbability;

    GameNode(GameNode* parent0, GameState gameState0)
    : parent(parent0), gameState(gameState0), totalEvaluation(0), totalVisits(0), depth(0), nextPlayerInSimultaneousNode(0),
      playerTurn(-1), actionsProbability(0.0)
    {}

    GameNode* createChild(playerActions_t action);
    GameNode* bestChild(int maxDepth);
    double    nodeValue(GameNode* node);
    void      deleteAllChildren();
    void      printNodeError(std::string errorMsg);
    bool      isTerminal();

    // bandit policies
    GameNode* eGreedy();
    GameNode* eGreedyInformed();
    GameNode* UCB();
    GameNode* PUCB();

    static GameNode* newGameNode(GameState &gs, GameNode* parent = nullptr);
};
