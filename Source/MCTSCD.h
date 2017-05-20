﻿#pragma once

#include "GameNode.h"
#include "RegionManager.h"
#include "GameState.h"
#include "ActionGenerator.h"
#include "EvaluationFunction.h"
#include <BWAPI.h>
#include "AbstractOrder.h"

#include <vector>

// © me & Alberto Uriarte
class MCTSCD
{
public:
    MCTSCD(int maxDepth, int maxSimulations, int maxSimulationTime, EvaluationFunction* ef, RegionManager* regman);
    playerActions_t start(const GameState& gs);

    RegionManager* regman;
    std::map<unsigned short, BWAPI::Unitset> idToSquad;
    unsigned addSquadToGameState(GameState& gs, const BWAPI::Unitset& squad);
private:

    int maxDepth;
#ifdef DEPTH_STATS
    int _maxDepthReached;
    int _maxDepthRolloutReached;
#endif
#ifdef BRANCHING_STATS
    Statistic _branching;
    Statistic _branchingRollout;
#endif
    int maxMissplacedUnits;
    int maxSimulations;
    int maxSimulationTime;
    EvaluationFunction* evalFun;
    GameState* rootGameState;

    playerActions_t startSearch(int cutOffTime);
    void simulate(GameState* gs, int time, int nextSimultaneous);
};
