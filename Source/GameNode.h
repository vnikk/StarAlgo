#pragma once

#include "GameState.h"
#include "ActionGenerator.h"
#include <vector>

class GameState;

// © me & Alberto Uriarte
class GameNode {
public:
    playerActions_t actions;
    double          totalEvaluation;
    double          totalVisits;
    GameState       gameState;
    int             playerTurn; // 1 : max, 0 : min, -1: Game-over
    ActionGenerator moveGenerator;
    int             nextPlayerInSimultaneousNode;
    double          actionsProbability;

    double                 depth;
    GameNode*              parent;
    std::vector<GameNode*> children;

    GameNode(GameNode* parent0, GameState gameState0)
    : parent(parent0), gameState(gameState0), totalEvaluation(0), totalVisits(0), depth(0),
      nextPlayerInSimultaneousNode(0), playerTurn(-1), actionsProbability(0.0), moveGenerator(&gameState0)
    {}

    GameNode* createChild(const playerActions_t& action);
    GameNode* bestChild(int maxDepth);
    double    nodeValue(GameNode* node);
    void      deleteSubtree();
    void      printNodeError(const std::string& errorMsg);
    bool      isTerminal();

    // bandit policies
    GameNode* eGreedy();
    GameNode* eGreedyInformed();
    GameNode* UCB();
    GameNode* PUCB();

    static GameNode* newGameNode(GameState &gs, GameNode* parent = nullptr);
};
