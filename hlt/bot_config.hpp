#pragma once

#include "game.hpp"
#include "constants.hpp"
#include "log.hpp"

using namespace std;
using namespace hlt;

// Spawn tuning for 64x64 4 player games
const int MAX_SHIPS = 24;            // Prevent over-fleeting and self-congestion
const int STOP_SPAWN_TURNS = 140;    // Stop spawning when game is getting late
const int HALITE_RESERVE = 1000;     // Keep some halite after spawning for flexibility
const int CONGESTION_RADIUS = 2;     // Manhattan distance around shipyard
const int CONGESTION_LIMIT = 3;      // If too many ships are nearby, do not spawn

// Mining targeting tuning
const int SEARCH_RADIUS = 8;         // How far a ship looks for a good mining cell
const int MIN_TARGET_HALITE = 120;   // Ignore very poor cells as targets
const int STAY_MINE_THRESHOLD = 100; // Stay still if current cell has enough halite

// Dropoff tuning
const int DROPOFF_COST = 4000;
const int MIN_DIST_DROPOFF = 15;     // Mini distance between two dropoffs
const double REQUIRED_HALITE_RADIUS = 10000.0; // Total halite required in the area around the dropoff
const int MAX_DROPOFFS = 3;          // Arbitrary limit on number of dropoffs to prevent over-expansion
const int MIN_SHIPS_RADIUS = 2;           // Minimum number of allied ships required in the area around the dropoff to consider building it
