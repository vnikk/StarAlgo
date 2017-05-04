#include "stdafx.h"

#include "MCTSCD.h"
#include "GameState.h"

#include <iomanip>

MCTSCD::MCTSCD(int maxDepth, int maxSimulations, int maxSimulationTime, EvaluationFunction* ef)
    :_maxDepth(maxDepth),
    _ef(ef),
    _maxSimulations(maxSimulations),
    _maxSimulationTime(maxSimulationTime),
#ifdef DEPTH_STATS
    _maxDepthReached(0),
    _maxDepthRolloutReached(0),
#endif
    _maxMissplacedUnits(0)
{
}

playerActions_t MCTSCD::start(GameState gs)
{
    _rootGameState = &gs;
    combatsSimulated = 0;

    timerUTC.start();
    playerActions_t bestAction = startSearch(_rootGameState->_time + _maxSimulationTime);
    double timeUCT = timerUTC.stopAndGetTime();

    // save stats
    int friendlyGroups = gs.getFriendlyGroupsSize();
    int friendlyUnits = gs.getFriendlyUnitsSize();
    int enemyGroups = gs.getEnemyGroupsSize();
    int enemyUnits = gs.getEnemyUnitsSize();
    auto oldPrecision = fileLog.precision(6);
    auto oldFlags = fileLog.flags();
    fileLog.setf(std::ios::fixed, std::ios::floatfield);
    LOG("Groups.F: " << std::setw(2) << friendlyGroups << " Groups.E: " << std::setw(2) << enemyGroups
        << " Units.F: " << std::setw(3) << friendlyUnits << " Units.E: " << std::setw(3) << enemyUnits
        << " seconds: " << std::setw(10) << timeUCT
        << " combats: " << std::setw(5) << combatsSimulated);
#ifdef BRANCHING_STATS
    << " maxBranching: " << std::setw(6) << (int)_branching.getMax()
        << " avgBranching: " << _branching.getMean());
#endif

        fileLog.precision(oldPrecision);
        fileLog.flags(oldFlags);
#ifdef DEPTH_STATS
        LOG(" - MaxDepth: " << _maxDepthReached << " MaxRolloutDepth: " << _maxDepthRolloutReached);
#endif

        return bestAction;
}

playerActions_t MCTSCD::startSearch(int cutOffTime)
{
    // create root node
    GameNode* tree = GameNode::newGameNode(*_rootGameState);

    // if root node only has one possible action, return it TODO that means gameover
    ActionGenerator moveGenerator(_rootGameState, true); // by default MAX player (friendly)
    if (moveGenerator._size == 1) {
        tree->deleteAllChildren(); // free memory
        return moveGenerator.getUniqueRandomAction();
    }

    // while withing computational budget
    for (int i = 0; i < _maxSimulations; ++i) {
        // tree policy, get best child
        GameNode* leaf = tree->bestChild(_maxDepth);

        if (leaf) { // always true
            // default policy, run simulation
            //             LOG("SIMULATION");
            GameState gs2 = leaf->gameState; // copy the game state to run simulation
            simulate(&gs2, cutOffTime, leaf->nextPlayerInSimultaneousNode);

            // use game frame time as a reduction factor
            int time = gs2._time - _rootGameState->_time;
            double evaluation = _ef->evaluate(gs2) * pow(0.999, time / 10.0);
            // update all parents' values; TODO good place to put mostVisited
            while (leaf != nullptr) {
                leaf->totalEvaluation += evaluation;
                leaf->totalVisits++;
                leaf = leaf->parent;
            }
        }
    }

    // get best child; TODO no need if maxVisited is controlled from gameState(node), or when parents' values are updated (see above ~88)
    int mostVisitedIdx = -1;
    GameNode* mostVisited = nullptr;
    for (unsigned int i = 0; i < tree->children.size(); ++i) {
        GameNode* child = tree->children[i];
        if (child->totalVisits > mostVisited->totalVisits || mostVisited == nullptr) {
            mostVisited = child;
            mostVisitedIdx = i;
        }
    }

    playerActions_t bestActions;
    if (mostVisitedIdx != -1) {
        bestActions = tree->actions[mostVisitedIdx];
    }
    else {
        tree->deleteAllChildren(); // free memory
        return moveGenerator.getUniqueRandomAction();
    }
#ifdef DEBUG_ORDERS
    // DEBUG check if actions are friendly actions
    for (const auto& actions : tree->actions) {
        for (const auto& action : actions) {
            if (!action.isFriendly) {
                DEBUG("Root actions are for enemey!!");
                // print game state
                DEBUG(_rootGameState->toString());
                // print possible actions
                ActionGenerator testActions(_rootGameState, true);
                DEBUG(testActions.toString());
            }
        }
    }
#endif
    tree->deleteAllChildren(); // free memory
    return bestActions;
}

void MCTSCD::simulate(GameState* gs, int time, int nextSimultaneous)
{
    int nextPlayerInSimultaneousNode = nextSimultaneous;
    int depth = 0;
    ActionGenerator moveGenerator;
    int nextPlayer = gs->getNextPlayerToMove(nextPlayerInSimultaneousNode);
    while (nextPlayer != -1 && gs->_time < time) {
        moveGenerator = ActionGenerator(gs, nextPlayer != 0);
#ifdef BRANCHING_STATS
        _branchingRollout.add(moveGenerator._size);
#endif

        // chose random action
        //         playerActions_t unitsAction = moveGenerator.getRandomAction();
        playerActions_t unitsAction = moveGenerator.getBiasAction();

        gs->execute(unitsAction, moveGenerator._player); // TODO could incorporate moveForward()
        gs->moveForward();
        depth++;

        // look next player to move
        nextPlayer = gs->getNextPlayerToMove(nextPlayerInSimultaneousNode);
    }
#ifdef DEPTH_STATS
    if (_maxDepthRolloutReached < depth) _maxDepthRolloutReached = depth;
#endif
}
