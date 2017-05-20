#include "stdafx.h"

#include "MCTSCD.h"
#include "GameState.h"

#include <iomanip>

// © me & Alberto Uriarte
MCTSCD::MCTSCD(int maxDepth, int maxSimulations, int maxSimulationTime, EvaluationFunction* ef, RegionManager* regman)
: maxDepth(maxDepth), evalFun(ef), maxSimulations(maxSimulations), maxSimulationTime(maxSimulationTime), regman(regman),
#ifdef DEPTH_STATS
    _maxDepthReached(0),
    _maxDepthRolloutReached(0),
#endif
    maxMissplacedUnits(0),
    rootGameState(nullptr)
{}

// © me & Alberto Uriarte
playerActions_t MCTSCD::start(const GameState& gs)
{
    rootGameState = new GameState(gs);
    combatsSimulated = 0;

    playerActions_t bestAction = startSearch(rootGameState->time + maxSimulationTime);

    // save stats
    int friendlyGroups = gs.getFriendlyGroupsSize();
    int friendlyUnits  = gs.getFriendlyUnitsSize();
    int enemyGroups    = gs.getEnemyGroupsSize();
    int enemyUnits     = gs.getEnemyUnitsSize();
    auto oldPrecision  = fileLog.precision(6);
    auto oldFlags      = fileLog.flags();
    fileLog.setf(std::ios::fixed, std::ios::floatfield);
    LOG("Groups.F: " << std::setw(2) << friendlyGroups << " Groups.E: " << std::setw(2) << enemyGroups
        << " Units.F: " << std::setw(3) << friendlyUnits << " Units.E: " << std::setw(3) << enemyUnits
        << " combats: " << std::setw(5) << combatsSimulated);
#ifdef BRANCHING_STATS
    << " maxBranching: " << std::setw(6) << (int)_branching.getMax()
        << " avgBranching: " << _branching.getMean();
#endif
        fileLog.precision(oldPrecision);
        fileLog.flags(oldFlags);
#ifdef DEPTH_STATS
        LOG(" - MaxDepth: " << _maxDepthReached << " MaxRolloutDepth: " << _maxDepthRolloutReached);
#endif
        return bestAction;
}

// © me & Alberto Uriarte
playerActions_t MCTSCD::startSearch(int cutOffTime)
{
    // create root node
    GameNode* tree = GameNode::newGameNode(*rootGameState);

    // if root node only has one possible action - return it
    ActionGenerator moveGenerator(rootGameState, true); // by default MAX player (friendly)
    if (moveGenerator.actionsSize == 1) { // default value
        tree->deleteSubtree(); // free memory
        return moveGenerator.getUniqueRandomAction();
    }
    // while withing computational budget
    for (int i = 0; i < maxSimulations; ++i) {
        // tree policy, get best child
        GameNode* leaf = tree->bestChild(maxDepth);

        if (leaf) { // always true
            // default policy, run simulation
            GameState gs2 = leaf->gameState; // copy the game state to run simulation
            simulate(&gs2, cutOffTime, leaf->nextPlayerInSimultaneousNode);

            // use game frame time as a reduction factor
            int time = gs2.time - rootGameState->time;
            double evaluation = evalFun->evaluate(gs2) * pow(0.999, time / 10.0);
            // update all parents' values
            while (leaf != nullptr) {
                leaf->totalEvaluation += evaluation;
                leaf->totalVisits++;
                leaf = leaf->parent;
            }
        }
    }

    // get best child
    int mostVisitedIdx = -1;
    GameNode* mostVisited = nullptr;
    for (unsigned int i = 0; i < tree->children.size(); ++i) {
        GameNode* child = tree->children[i];
        if (mostVisited == nullptr || child->totalVisits > mostVisited->totalVisits) {
            mostVisited = child;
            mostVisitedIdx = i;
        }
    }

    playerActions_t bestActions;
    if (mostVisitedIdx != -1) {
        bestActions = tree->children[mostVisitedIdx]->actions;
    }
    else {
        tree->deleteSubtree(); // free memory
        return moveGenerator.getUniqueRandomAction();
    }
#ifdef DEBUG_ORDERS
    // DEBUG check if actions are friendly actions
    for (const auto& actions : tree->actions) {
        for (const auto& action : actions) {
            if (!action.isFriendly) {
                DEBUG("Root actions are for enemey!!");
                // print game state
                DEBUG(rootGameState->toString());
                // print possible actions
                ActionGenerator testActions(rootGameState, true);
                DEBUG(testActions.toString());
            }
        }
    }
#endif
    tree->deleteSubtree(); // free memory
    return bestActions;
}

// © me & Alberto Uriarte
void MCTSCD::simulate(GameState* gs, int time, int nextSimultaneous)
{
    int nextPlayerInSimultaneousNode = nextSimultaneous;
    int depth = 0;
    ActionGenerator moveGenerator(gs);
    int nextPlayer = gs->getNextPlayerToMove(nextPlayerInSimultaneousNode);
    while (nextPlayer != -1 && gs->time < time) {
        moveGenerator = ActionGenerator(gs, nextPlayer != 0);
        // chose random action
        playerActions_t unitsAction = moveGenerator.getBiasAction();

        gs->execute(unitsAction, moveGenerator.isFriendly);
        gs->moveForward();
        depth++;
        // look next player to move
        nextPlayer = gs->getNextPlayerToMove(nextPlayerInSimultaneousNode);
    }
#ifdef DEPTH_STATS
    if (_maxDepthRolloutReached < depth) _maxDepthRolloutReached = depth;
#endif
}

// © me & Alberto Uriarte
unsigned MCTSCD::addSquadToGameState(GameState& gs, const BWAPI::Unitset& squad)
{
    std::map<unsigned short, unsigned int> groupIdFrequency;

    unsigned short abstractGroupID;
    // for each unit on the squad, add it to the game state and save id reference
    for (const auto& squadUnit : squad) {
        if (squadUnit->getType().isWorker()) { continue; } // ignore workers
        abstractGroupID = gs.addFriendlyUnit(squadUnit);
        groupIdFrequency[abstractGroupID]++;
    }
    unsigned int maxFrequency = 0;
    unsigned short bestGroup;
    // assign to the squad the most common group ID
    for (const auto& frequency : groupIdFrequency) {
        if (frequency.second > maxFrequency) {
            bestGroup = frequency.first;
            maxFrequency = frequency.second;
        }
    }
    // one idGroup can have many squads
    if (!idToSquad[bestGroup].empty()) {
        idToSquad[bestGroup].insert(squad.begin(), squad.end());
    }
    else {
        idToSquad[bestGroup] = squad;
    }
    return bestGroup;
}
