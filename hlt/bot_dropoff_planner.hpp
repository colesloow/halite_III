#pragma once

#include "game.hpp"
#include "constants.hpp"
#include "log.hpp"

#include "bot_config.hpp"

using namespace std;
using namespace hlt;

// Compute total halite in a square area around a position (used for dropoff placement)
int count_halite_in_area(const Position& center, GameMap* game_map, int radius);

bool try_build_dropoff(
    const shared_ptr<Ship>& ship,
    const shared_ptr<Player>& me,
    GameMap* game_map,
    int turns_remaining,
    vector<Command>& command_queue,
    vector<vector<bool>>& next_turn_occupied
);
