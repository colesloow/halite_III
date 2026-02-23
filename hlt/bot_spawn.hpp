#pragma once

#include "game.hpp"
#include "constants.hpp"
#include "log.hpp"

#include "bot_config.hpp"

using namespace std;
using namespace hlt;

void try_spawn(
    const shared_ptr<Player>& me,
    GameMap* game_map,
    int turns_remaining,
    vector<vector<bool>>& next_turn_occupied,
    vector<Command>& command_queue,
    int max_ships
);
