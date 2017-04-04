#pragma once

#include <vector>

#include "Timer.h"
#include "GameState.h"
#include "ActionGenerator.h"
#include "EvaluationFunction.h"
#include "../../../../../g/BWAPI/include/BWAPI.h"

// TODO
// #define HTML_LOG // uncomment to print MCTS HTML output, uncomment also DEBUG_ORDERS in AbstractOrder.h
// #define DEPTH_STATS
// #define BRANCHING_STATS

class MCTSCD
{
public:
	struct gameNode_t {
		std::vector<playerActions_t> actions; // action of each child
		double totalEvaluation;
		double totalVisits;
		struct gameNode_t* parent; // to back propagate the stats
		std::vector<struct gameNode_t*> children; // to delete the tree
		GameState gs;
		double depth;
		int player; // 1 : max, 0 : min, -1: Game-over
		ActionGenerator moveGenerator;
		int nextPlayerInSimultaneousNode;
		std::vector<GameState> simulations; // only used for HTML_LOG
		double prob;

		gameNode_t(struct gameNode_t* parent0, GameState gs0) : parent(parent0), gs(gs0),
			totalEvaluation(0), totalVisits(0), depth(0), nextPlayerInSimultaneousNode(0),
			player(-1), prob(0.0) {}
	};
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
	gameNode_t* newGameNode(GameState &gs, gameNode_t* parent = nullptr);
	gameNode_t* bestChild(gameNode_t* currentNode);
	double nodeValue(gameNode_t* node);
	void simulate(GameState* gs, int time, int nextSimultaneous);
	void deleteAllChildren(gameNode_t* node);
	void printNodeError(std::string errorMsg, gameNode_t* node);
	gameNode_t* createChild(gameNode_t* parentNode, playerActions_t action);

	// bandit policies
	gameNode_t* eGreedy(gameNode_t* currentNode);
	gameNode_t* eGreedyInformed(gameNode_t* currentNode);
	gameNode_t* UCB(gameNode_t* currentNode);
	gameNode_t* PUCB(gameNode_t* currentNode);
};