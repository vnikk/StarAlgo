#include "stdafx.h"
#include "GameNode.h"

#include <random>
#include <iomanip>

const double UCB_C = 5;        // this is the constant that regulates exploration vs exploitation, it must be tuned for each domain
const double EPSILON = 0.2; // e-greedy strategy

// returns a node that has >1 actions, or gameover
//my
void GameNode::deleteAllChildren() // TODO rename to deleteSubtree? 
{
    for (auto& child : children) child->deleteAllChildren();
    delete this; // https://isocpp.org/wiki/faq/freestore-mgmt#delete-this As long as you're careful, it's OK for an object to commit suicide (delete this).
}

//my
GameNode* GameNode::bestChild(int maxDepth)
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
    return best->bestChild(maxDepth);
}

GameNode* GameNode::createChild(playerActions_t action)
{
    if (!action.empty()) {
        GameState gameState2 = gameState.cloneIssue(action, moveGenerator._player);
        GameNode* newChild = newGameNode(gameState2, this);
        newChild->actions = action;
        children.push_back(newChild);
        return newChild;
    }
    else {
        printNodeError("Error generating action for first child");
        return this;
    }
}

void GameNode::printNodeError(std::string errorMsg)
{
    DEBUG(errorMsg);
    LOG(gameState.toString());
    LOG("playerTurn: " << playerTurn);
}

GameNode* GameNode::eGreedyInformed()
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
        for (auto& child : children) {
            if (action == child->actions) {
                return child;
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

GameNode* GameNode::eGreedy()
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

GameNode* GameNode::UCB()
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

double GameNode::nodeValue(GameNode* node)
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

GameNode* GameNode::PUCB()
{
    // if no children yet, create them
    if (children.empty()) {
        //         if (moveGenerator._size > 10000) LOG("Creating " << moveGenerator._size << " nodes");
        double prob;
        while (moveGenerator.hasMoreActions()) {
            playerActions_t action = moveGenerator.getNextActionProbability(prob);
            GameNode* child = createChild(action);
            child->actionsProbability = prob;
        }
        //         if (->moveGenerator._size > 10000) LOG("Done.");
    }

    double bestScore = 0;
    double tmpScore;
    GameNode* best = nullptr;
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
        double bonusPenalty = 2.0 / child->actionsProbability;
        if (child->parent->totalVisits > 1) {
            bonusPenalty *= sqrt(log(child->parent->totalVisits) / child->parent->totalVisits);
        }

        tmpScore = avgReward + upperBound - bonusPenalty;

        if (best == nullptr || tmpScore > bestScore) {
            best = child;
            bestScore = tmpScore;
        }
    }

    return best;
}

GameNode* GameNode::newGameNode(GameState &gameState, GameNode* parent/* = nullptr*/)
{
    GameNode* newNode = new GameNode(parent, gameState);

    if (parent != nullptr) {
        newNode->depth = parent->depth + 1;
        newNode->nextPlayerInSimultaneousNode = parent->nextPlayerInSimultaneousNode;
#ifdef DEPTH_STATS
        if (_maxDepthReached < newGameNode->depth) _maxDepthReached = (int)newGameNode->depth;
#endif
    }

    // get the next player to move
    newNode->playerTurn = newNode->gameState.getNextPlayerToMove(newNode->nextPlayerInSimultaneousNode);
    // if it's a leaf we are done
    if (newNode->playerTurn == -1) return newNode;
    // generate the player possible moves
    newNode->moveGenerator = ActionGenerator(&newNode->gameState, newNode->playerTurn != 0);

#ifdef _DEBUG
    int iterations = 0;
#endif
    // if we only have one possible action, execute it and move forward
    while (newNode->moveGenerator._size == 1) {
        // execute the only action
        playerActions_t unitsAction = newNode->moveGenerator.getNextAction();
        newNode->gameState.execute(unitsAction, newNode->moveGenerator._player);
        newNode->gameState.moveForward();

        // prepare the node
        // get the next player to move
        newNode->playerTurn = newNode->gameState.getNextPlayerToMove(newNode->nextPlayerInSimultaneousNode);
        // if it's a leaf we are done
        if (newNode->playerTurn == -1) return newNode;
        // generate the player possible moves
        newNode->moveGenerator = ActionGenerator(&newNode->gameState, newNode->playerTurn != 0);

#ifdef _DEBUG
        iterations++;
        // DEBUG if still only one action, we can end in an infinite loop
        if (newNode->moveGenerator._size == 1 && iterations > 1) {
            DEBUG("Still only 1 action at iteration " << iterations);
        }
        if (newNode->moveGenerator._size < 1) {
            DEBUG("PARENT GAME STATE");
            DEBUG(gameState.toString());
            DEBUG("LAST GAME STATE");
            DEBUG(newNode->gameState.toString());
            DEBUG("Wrong number of actions");
        }
#endif
    }

#ifdef BRANCHING_STATS
    _branching.add(newNode->moveGenerator._size);
#endif
    return newNode;
}

bool GameNode::isTerminal()
{
    // or enemy encountered (! node->gameState._regionsInCombat.empty())
    return !gameState.getArmiesRegionsIntersection().empty() || gameState.gameover();
}

