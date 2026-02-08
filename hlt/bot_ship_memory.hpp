#pragma once

#include "game.hpp"
#include "constants.hpp"
#include "log.hpp"

#include <unordered_map>

using namespace std;
using namespace hlt;

enum class ShipState {
    MINING,
    RETURNING
};

struct ShipMemory {
    // Map to memorize the state of each ship between turns (step 2)
    unordered_map<EntityId, ShipState> ship_status;
    // Map to memorize a mining target for each ship between turns
    unordered_map<EntityId, Position> ship_target;

    void cleanup_dead_ships(const shared_ptr<Player>& me);
    void ensure_initialized(const shared_ptr<Ship>& ship);
};
