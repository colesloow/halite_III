#pragma once

#include "game.hpp"
#include "constants.hpp"
#include "log.hpp"
#include "bot_ship_memory.hpp"
#include "bot_config.hpp"

using namespace std;
using namespace hlt;

// Inspiration tuning (engine rules: >= 2 enemy ships within manhattan distance 4)
static const int INSPIRATION_RADIUS = 4;
static const int INSPIRATION_SHIPS_REQUIRED = 2;
static const int INSPIRED_MULTIPLIER = 3;

Position pick_mining_target(
    const Position& ship_position,
    GameMap* game_map_ptr,
    const vector<vector<bool>>& inspired
);

Direction decide_mining_direction(
    const shared_ptr<Ship>& ship,
    GameMap* game_map,
    ShipMemory& mem,
    const vector<vector<bool>>& next_turn_occupied,
    const vector<vector<bool>>& danger_map,
    const vector<vector<bool>>& inspired
);
