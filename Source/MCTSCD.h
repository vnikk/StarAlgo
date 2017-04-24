#pragma once

#include <vector>

#include "Timer.h"
#include "GameState.h"
#include "ActionGenerator.h"
#include "EvaluationFunction.h"
#include "../../../../../g/BWAPI/include/BWAPI.h"

// TODO
// #define DEPTH_STATS
// #define BRANCHING_STATS

class MCTSCD
{
public:
	struct GameNode {
		std::vector<playerActions_t> actions; // action of each child
		double totalEvaluation;
		double totalVisits;
		struct GameNode* parent; // to back propagate the stats
		std::vector<struct GameNode*> children; // to delete the tree
		GameState gs;
		double depth;
		int playerTurn; // 1 : max, 0 : min, -1: Game-over
		ActionGenerator moveGenerator;
		int nextPlayerInSimultaneousNode;
		double prob;

		GameNode(struct GameNode* parent0, GameState gs0) : parent(parent0), gs(gs0),
			totalEvaluation(0), totalVisits(0), depth(0), nextPlayerInSimultaneousNode(0),
			playerTurn(-1), prob(0.0) {}
	};
	// TODO hide default?
	MCTSCD();
	MCTSCD(int maxDepth, int maxSimulations, int maxSimulationTime, EvaluationFunction* ef);
	playerActions_t start(GameState gs);

private:
	int _maxDepth;
#ifdef DEPTH_STATS
	int _maxDepthReached;
	int _maxDepthRolloutReached;
#endif
#ifdef BRANCHING_STATS
	Statistic _branching;
	Statistic _branchingRollout;
#endif
	int _maxMissplacedUnits;
	EvaluationFunction* _ef;
	int _maxSimulations;
	int _maxSimulationTime;
	GameState* _rootGameState;

	// timers
	Timer timerUTC;

	playerActions_t startSearch(int cutOffTime);
	GameNode* newGameNode(GameState &gs, GameNode* parent = nullptr);
	GameNode* bestChild(GameNode* currentNode);
	double nodeValue(GameNode* node);
	void simulate(GameState* gs, int time, int nextSimultaneous);
	void deleteAllChildren(GameNode* node);
	void printNodeError(std::string errorMsg, GameNode* node);
	GameNode* createChild(GameNode* parentNode, playerActions_t action);

	// bandit policies
	GameNode* eGreedy(GameNode* currentNode);
	GameNode* eGreedyInformed(GameNode* currentNode);
	GameNode* UCB(GameNode* currentNode);
	GameNode* PUCB(GameNode* currentNode);
};