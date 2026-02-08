#include "bot_dropoff_planner.hpp"

// Compute total halite in a square area around a position (used for dropoff placement)
int count_halite_in_area(const Position& center, GameMap* game_map, int radius) {
    int total_halite = 0;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            Position pos = game_map->normalize(center + Position{ dx, dy });
            total_halite += game_map->at(pos)->halite;
        }
    }
    return total_halite;
}

bool try_build_dropoff(
    const shared_ptr<Ship>& ship,
    const shared_ptr<Player>& me,
    GameMap* game_map,
    int turns_remaining,
    vector<Command>& command_queue,
    vector<vector<bool>>& next_turn_occupied
) {
    // Dropoff construction logic (step 7)
    // Construction is considered only if we have the budget and enough time left
    // Keeping a security margin (SHIP_COST) to be able to spawn after if needed
    if (me->halite >= DROPOFF_COST + constants::SHIP_COST &&
        turns_remaining > 100 &&
        me->dropoffs.size() < MAX_DROPOFFS)
    {
        // Check 1 : Distance with the shipyard
        int dist_to_yard = game_map->calculate_distance(ship->position, me->shipyard->position);

        // Check 2 : Distance with other dropoffs
        bool too_close = false;
        for (const auto& dropoff : me->dropoffs) {
            if (game_map->calculate_distance(ship->position, dropoff.second->position) < MIN_DIST_DROPOFF) {
                too_close = true;
                break;
            }
        }

        // If we're far enough from existing structures and the cell doesn't already have a structure
        if (dist_to_yard >= MIN_DIST_DROPOFF && !too_close && !game_map->at(ship)->has_structure()) {

            // Check 3 : Halite in the area (radius of 4)
            int local_halite = count_halite_in_area(ship->position, game_map, 4);

            if (local_halite >= REQUIRED_HALITE_RADIUS) {
                // Build a dropoff here
                command_queue.push_back(ship->make_dropoff());

                // IMPORTANT: Virtually deduct the cost right away
                // "Virtually" deducting halite to avoid multiple ships deciding to build dropoffs on the same turn
                me->halite -= DROPOFF_COST;

                // Marking the cell as occupied (dropoff is a structure)
                next_turn_occupied[ship->position.y][ship->position.x] = true;

                return true; // Skip the rest of the logic for this ship since it's now building a dropoff
            }
        }
    }

    return false;
}
