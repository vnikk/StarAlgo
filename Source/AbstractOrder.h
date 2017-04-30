#pragma once

#include <vector>
#include <cstdint>

// #define DEBUG_ORDERS

struct action_t {
    uint8_t orderID;
    uint8_t targetRegion; // if needed
    action_t(uint8_t _orderID, uint8_t _targetRegion = 0) : orderID(_orderID), targetRegion(_targetRegion) {}
    friend bool operator== (const action_t& lhs, const action_t& rhs) {
        return (lhs.orderID == rhs.orderID && lhs.targetRegion == rhs.targetRegion);
    };
};

struct choice_t {
    unsigned short pos;    // position in the unit vector of the game state
    std::vector<action_t> actions;
#ifdef DEBUG_ORDERS
    uint8_t unitTypeId;
    uint8_t regionId;
    bool isFriendly;
    choice_t(unsigned short _pos, std::vector<action_t> _actions, uint8_t _unitTypeId, uint8_t _regionId, bool _isFriendly)
        : pos(_pos), actions(_actions), unitTypeId(_unitTypeId), regionId(_regionId), isFriendly(_isFriendly) {}
#else
    choice_t(unsigned short _pos, std::vector<action_t> _actions) : pos(_pos), actions(_actions) {}
#endif
};
typedef std::vector<choice_t> choices_t;

struct playerAction_t {
    unsigned short pos;    // position in the unit vector of the game state
    action_t action;

    playerAction_t() : pos(0), action(action_t(0, 0)) {}
    friend bool operator== (const playerAction_t& lhs, const playerAction_t& rhs) {
        return (lhs.pos == rhs.pos && lhs.action == rhs.action);
    };
#ifdef DEBUG_ORDERS
    uint8_t unitTypeId;
    uint8_t regionId;
    bool isFriendly;
    playerAction_t(choice_t order, int orderSelected)
        : pos(order.pos), action(order.actions[orderSelected]), unitTypeId(order.unitTypeId), regionId(order.regionId), isFriendly(order.isFriendly) {}
#else
    playerAction_t(choice_t order, int orderSelected)
    : pos(order.pos), action(order.actions[orderSelected])
    {}
#endif
};
typedef std::vector<playerAction_t> playerActions_t;

namespace abstractOrder {
    static enum order {
        Unknown, Nothing, Idle, Gas, Mineral, Move, Attack, Heal
    };

    static std::string name[8] = {
        "Unknown", "Nothing", "Idle", "Gas", "Mineral", "Move", "Attack", "Heal"
    };

    static order getOrder(std::string orderName) {
        if (orderName.find("Nothing") != std::string::npos) return abstractOrder::Nothing;
        else if (orderName.find("Idle") != std::string::npos) return abstractOrder::Idle;
        else if (orderName.find("Gas") != std::string::npos) return abstractOrder::Gas;
        else if (orderName.find("Mineral") != std::string::npos) return abstractOrder::Mineral;
        else if (orderName.find("Move") != std::string::npos) return abstractOrder::Move;
        else if (orderName.find("Attack") != std::string::npos) return abstractOrder::Attack;
        else if (orderName.find("Heal") != std::string::npos) return abstractOrder::Heal;
        else return abstractOrder::Unknown;
    }
};
