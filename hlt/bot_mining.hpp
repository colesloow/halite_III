#pragma once

#include "game.hpp"
#include "constants.hpp"
#include "log.hpp"
#include "bot_ship_memory.hpp"
#include "bot_config.hpp"

using namespace std;
using namespace hlt;

Position pick_mining_target(const Position& ship_position, GameMap* game_map_ptr);

Direction decide_mining_direction(
    const shared_ptr<Ship>& ship,
    GameMap* game_map,
    ShipMemory& mem,
    const vector<vector<bool>>& next_turn_occupied
);
