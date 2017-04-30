#include "stdafx.h"

#include "MCTSCD.h"
#include "GameState.h"

#include <random>
#include <iomanip>

const double UCB_C = 5;        // this is the constant that regulates exploration vs exploitation, it must be tuned for each domain
const double EPSILON = 0.2; // e-greedy strategy

//MCTSCD::MCTSCD() {}

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
    GameNode* tree = newGameNode(*_rootGameState);

    // if root node only has one possible action, return it
    ActionGenerator moveGenerator(_rootGameState, true); // by default MAX player (friendly)
    if (moveGenerator._size == 1) {
        //         return moveGenerator.getNextAction();
        return moveGenerator.getUniqueRandomAction();
    }

    // while withing computational budget
    for (int i = 0; i<_maxSimulations; ++i) {
        // tree policy, get best child
        GameNode* leaf = tree->bestChild(_maxDepth);

        if (leaf) {
            // default policy, run simulation
            //             LOG("SIMULATION");
            GameState gs2 = leaf->gs; // copy the game state to run simulation
            simulate(&gs2, cutOffTime, leaf->nextPlayerInSimultaneousNode);

            // use game frame time as a reduction factor
            int time = gs2._time - _rootGameState->_time;
            double evaluation = _ef->evaluate(gs2)*pow(0.999, time / 10.0);
            //             double evaluation = _ef->evaluate(gs2);

            // backup
            while (leaf != nullptr) {
                leaf->totalEvaluation += evaluation;
                leaf->totalVisits++;
                leaf = leaf->parent;
            }
        }
    }

    // return best child
    int mostVisitedIdx = -1;
    GameNode* mostVisited = nullptr;
    for (unsigned int i = 0; i<tree->children.size(); ++i) {
        GameNode* child = tree->children[i];
        if (mostVisited == nullptr || child->totalVisits > mostVisited->totalVisits) {
            mostVisited = child;
            mostVisitedIdx = i;
        }
    }

    playerActions_t bestActions;
    if (mostVisitedIdx != -1) {
        bestActions = tree->actions[mostVisitedIdx];
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

        // execute action
        gs->execute(unitsAction, moveGenerator._player);
        gs->moveForward();
        depth++;

        // look next player to move
        nextPlayer = gs->getNextPlayerToMove(nextPlayerInSimultaneousNode);
    }
#ifdef DEPTH_STATS
    if (_maxDepthRolloutReached < depth) _maxDepthRolloutReached = depth;
#endif
}

MCTSCD::GameNode* MCTSCD::newGameNode(GameState &gs, GameNode* parent)
{
    MCTSCD::GameNode* newNode = new MCTSCD::GameNode(parent, gs);

    if (parent != nullptr) {
        newNode->depth = parent->depth + 1;
        newNode->nextPlayerInSimultaneousNode = parent->nextPlayerInSimultaneousNode;
#ifdef DEPTH_STATS
        if (_maxDepthReached < newGameNode->depth) _maxDepthReached = (int)newGameNode->depth;
#endif
    }

    // get the next player to move
    newNode->playerTurn = newNode->gs.getNextPlayerToMove(newNode->nextPlayerInSimultaneousNode);
    // if it's a leaf we are done
    if (newNode->playerTurn == -1) return newNode;
    // generate the player possible moves
    newNode->moveGenerator = ActionGenerator(&newNode->gs, newNode->playerTurn != 0);

    // if we only have one possible action, execute it and move forward
    int iterations = 0;
    while (newNode->moveGenerator._size == 1) {
        iterations++;

        // execute the only action
        playerActions_t unitsAction = newNode->moveGenerator.getNextAction();
        newNode->gs.execute(unitsAction, newNode->moveGenerator._player);
        newNode->gs.moveForward();

        // prepare the node
        // get the next player to move
        newNode->playerTurn = newNode->gs.getNextPlayerToMove(newNode->nextPlayerInSimultaneousNode);
        // if it's a leaf we are done
        if (newNode->playerTurn == -1) return newNode;
        // generate the player possible moves
        newNode->moveGenerator = ActionGenerator(&newNode->gs, newNode->playerTurn != 0);

#ifdef _DEBUG
        // DEBUG if still only one action, we can end in an infinite loop
        if (newNode->moveGenerator._size == 1 && iterations > 1) {
            DEBUG("Still only 1 action at iteration " << iterations);
        }
        if (newNode->moveGenerator._size < 1) {
            DEBUG("PARENT GAME STATE");
            DEBUG(gs.toString());
            DEBUG("LAST GAME STATE");
            DEBUG(newNode->gs.toString());
            DEBUG("Wrong number of actions");
        }
#endif
    }

#ifdef BRANCHING_STATS
    _branching.add(newNode->moveGenerator._size);
#endif
    return newNode;
}

//my
void MCTSCD::GameNode::deleteAllChildren() // TODO rename to deleteSubtree? 
{
    for (auto& child : children) deleteAllChildren();
    delete this; // https://isocpp.org/wiki/faq/freestore-mgmt#delete-this As long as you're careful, it's OK for an object to commit suicide (delete this).
}

//my
MCTSCD::GameNode* MCTSCD::GameNode::bestChild(int maxDepth)
{
    // Cut the tree policy at a predefined depth
    if (maxDepth && depth >= maxDepth) return this;

    // if gameover return this node
    if (playerTurn == -1) return this;

    // if first time here return this node
    if (totalVisits == 0) return this;

    // Bandit policy (aka Tree policy)
    //     GameNode* best = UCB(this);
    //     GameNode* best = eGreedy(this);
    //     GameNode* best = eGreedyInformed(this);
    GameNode* best = PUCB();

    if (best == nullptr) {
        // No more leafs because this node has no children!
        return this;
    }
    return bestChild(maxDepth);
}

MCTSCD::GameNode* MCTSCD::GameNode::createChild(playerActions_t action)
{
    if (!action.empty()) {
        actions.push_back(action);
        GameState gs2 = gs.cloneIssue(action, moveGenerator._player);
        GameNode* newChild = newGameNode(gs2, this);
        children.push_back(newChild);
        return newChild;
    }
    else {
        printNodeError("Error generating action for first child");
        return this;
    }
}

void MCTSCD::GameNode::printNodeError(std::string errorMsg)
{
    DEBUG(errorMsg);
    LOG(gs.toString());
    LOG("playerTurn: " << playerTurn);
}

MCTSCD::GameNode* MCTSCD::GameNode::eGreedyInformed()
{
    // if no children yet, create one
    if (children.empty()) {
        if (!moveGenerator.hasMoreActions()) {
            printNodeError("Error creating first child");
            return this;
        }
        //         playerActions_t action = currentNode->moveGenerator.getBiasAction();
        playerActions_t action = moveGenerator.getMostProbAction();
        return createChild(action);

    }

    std::uniform_real_distribution<> uniformDist(0, 1);
    double randomNumber = uniformDist(gen);
    //LOG("Random number: " << randomNumber);
    GameNode* best = nullptr;

    if (randomNumber < EPSILON) { // select bias random
        playerActions_t action = moveGenerator.getBiasAction();
        // look if node already generated
        for (size_t i = 0; i < actions.size(); ++i) {
            if (action == actions.at(i)) {
                return children.at(i);
            }
        }
        // else, create the new child
        return createChild(action);
    }
    else { // select max reward
        double bestScore = 0;
        double tmpScore;
        for (const auto& child : children) {
            tmpScore = child->totalEvaluation / child->totalVisits;
            if (playerTurn == 0) tmpScore = -tmpScore; // if min node, reverse score
            if (best == nullptr || tmpScore > bestScore) {
                best = child;
                bestScore = tmpScore;
            }
        }
    }

    return best;
}

MCTSCD::GameNode* MCTSCD::GameNode::eGreedy()
{
    // if no children yet, create one
    if (children.empty()) {
        if (!moveGenerator.hasMoreActions()) {
            printNodeError("Error creating first child");
            return this;
        }
        playerActions_t action = moveGenerator.getUniqueRandomAction();
        return createChild(action);

    }

    GameNode* best = nullptr;

    std::uniform_real_distribution<> uniformDist(0, 1);
    double randomNumber = uniformDist(gen);

    if (randomNumber < EPSILON) {
        // select random child
        // -------------------------
        unsigned int totalChildren;
        double maxUnsignedInt = std::numeric_limits<unsigned int>::max();
        if (moveGenerator._size > maxUnsignedInt) totalChildren = (unsigned int)maxUnsignedInt;
        else totalChildren = (unsigned int)moveGenerator._size;

        unsigned int createdChildren = children.size();
        std::uniform_int_distribution<int> uniformDist(0, totalChildren - 1);
        int randomChoice = uniformDist(gen);

        //if (randomChoice > createdChildren) { // create a new child if random choice is outside created children
        if (totalChildren > createdChildren) { // create a new child if we still have children to create
            playerActions_t action = moveGenerator.getUniqueRandomAction();
            return createChild(action);
        }
        else { // pick one of the created children
            best = children.at(randomChoice);
        }
    }
    else {
        // select max reward child
        // -------------------------
        double bestScore = 0;
        double tmpScore;
        for (const auto& child : children) {
            tmpScore = child->totalEvaluation / child->totalVisits;
            if (playerTurn == 0) tmpScore = -tmpScore; // if min node, reverse score
            if (best == nullptr || tmpScore > bestScore) {
                best = child;
                bestScore = tmpScore;
            }
        }
    }

    return best;
}

MCTSCD::GameNode* MCTSCD::GameNode::UCB()
{
    // WARNING if branching factor too high we will stuck at this depth
    // if non visited children, visit
    if (moveGenerator.hasMoreActions()) {
        playerActions_t action = moveGenerator.getUniqueRandomAction();
        return createChild(action);
    }

    double bestScore = 0;
    double tmpScore;
    GameNode* best = nullptr;
    for (const auto& child : children) {
        tmpScore = nodeValue(child);
        if (best == nullptr || tmpScore > bestScore) {
            best = child;
            bestScore = tmpScore;
        }
    }

    return best;
}

double MCTSCD::GameNode::nodeValue(MCTSCD::GameNode* node)
{
    double exploitation = node->totalEvaluation / node->totalVisits;
    double exploration = sqrt(log(node->parent->totalVisits / node->totalVisits));
    if (node->parent->playerTurn == 1) { // max node:
        //exploitation = (exploitation + evaluation_bound)/(2*evaluation_bound);
    }
    else {
        //exploitation = - (exploitation - evaluation_bound)/(2*evaluation_bound);
        exploitation = -exploitation;
    }

    double tmp = UCB_C*exploitation + exploration;
    return tmp;
}

MCTSCD::GameNode* MCTSCD::GameNode::PUCB()
{
    // if no children yet, create them
    if (children.empty()) {
        //         if (moveGenerator._size > 10000) LOG("Creating " << moveGenerator._size << " nodes");
        double prob;
        while (moveGenerator.hasMoreActions()) {
            playerActions_t action = moveGenerator.getNextActionProbability(prob);
            GameNode* child = createChild(action);
            child->prob = prob;
        }
        //         if (->moveGenerator._size > 10000) LOG("Done.");
    }

    double bestScore = 0;
    double tmpScore;
    GameNode* best = nullptr;
    //     LOG("NEW NODE");
    for (const auto& child : children) {
        // compute score
        double avgReward = 1.0;
        double upperBound = 0.0;
        if (child->totalVisits > 0) {
            avgReward = child->totalEvaluation / child->totalVisits;
            if (child->parent->playerTurn == 0) avgReward = -avgReward; // min node
            // since our reward is form -1 to 1, we need to normalize to 0 to 1
            avgReward = (avgReward + 1) / 2.0;
            upperBound = sqrt((3 * log(child->parent->totalVisits)) / (2 * child->totalVisits));
        }
        double bonusPenalty = 2.0 / child->prob;
        if (child->parent->totalVisits > 1) {
            bonusPenalty *= sqrt(log(child->parent->totalVisits) / child->parent->totalVisits);
        }

        tmpScore = avgReward + upperBound - bonusPenalty;
        //         LOG("Score: " << avgReward << " + " << upperBound << " - " << bonusPenalty << " = " << tmpScore);

        if (best == nullptr || tmpScore > bestScore) {
            best = child;
            bestScore = tmpScore;
        }
    }

    return best;
}
