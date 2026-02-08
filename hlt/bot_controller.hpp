#pragma once

#include "game.hpp"
#include "constants.hpp"
#include "log.hpp"

#include "bot_ship_memory.hpp"

#include <random>
#include <vector>

using namespace std;
using namespace hlt;

class BotController {
public:
    explicit BotController(mt19937& rng);

    vector<Command> play_turn(Game& game);

private:
    mt19937& rng_;
    ShipMemory mem_;
};
