#pragma once

#include "game.hpp"
#include "constants.hpp"
#include "log.hpp"

#include "bot_ship_memory.hpp"

using namespace std;
using namespace hlt;

Direction smart_navigate(
    const shared_ptr<Ship>& ship,
    GameMap* game_map,
    const Position& target,
    const vector<vector<bool>>& next_turn_occupied,
    const vector<vector<bool>>& danger_map
);

Position get_nearest_deposit_position(
    const shared_ptr<Player>& me,
    GameMap* game_map,
    const Position& from
);

void update_ship_state(
    const shared_ptr<Ship>& ship,
    const shared_ptr<Player>& me,
    GameMap* game_map,
    int turns_remaining,
    ShipMemory& mem
);

Direction decide_returning_direction(
    const shared_ptr<Ship>& ship,
    const shared_ptr<Player>& me,
    GameMap* game_map,
    const vector<vector<bool>>& next_turn_occupied,
    const vector<vector<bool>>& danger_map,
    bool is_inspired
);

Direction apply_move_cost_safety(
    const shared_ptr<Ship>& ship,
    GameMap* game_map,
    Direction intended_direction
);

Command finalize_and_reserve_move(
    const shared_ptr<Ship>& ship,
    GameMap* game_map,
    Direction intended_direction,
    vector<vector<bool>>& next_turn_occupied
);
