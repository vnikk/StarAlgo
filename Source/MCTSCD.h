#pragma once

#include "GameNode.h"
#include "RegionManager.h"
#include "Timer.h"
#include "GameState.h"
#include "ActionGenerator.h"
#include "EvaluationFunction.h"
#include <BWAPI.h>

#include <vector>
// TODO
// #define DEPTH_STATS
// #define BRANCHING_STATS

//TODO rename methods
class MCTSCD
{
public:
    //MCTSCD(); // TODO need default?
    MCTSCD(int maxDepth, int maxSimulations, int maxSimulationTime, EvaluationFunction* ef);
    playerActions_t start(const GameState& gs);

    RegionManager regman;
    std::map<unsigned short, BWAPI::Unitset> _idToSquad;
    void addSquadToGameState(GameState& gs, const BWAPI::Unitset& squad); // TODO change to Nova Squad; maybe??
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
    void simulate(GameState* gs, int time, int nextSimultaneous);
};
