#include "bot_mining.hpp"
#include "bot_navigation.hpp"

Position pick_mining_target(
    const Position& ship_position,
    GameMap* game_map_ptr,
    const vector<vector<bool>>& inspired
) {
    Position best_position = ship_position;
    double best_score = -1.0;

    // Explore a square area around the ship
    for (int offset_y = -SEARCH_RADIUS; offset_y <= SEARCH_RADIUS; ++offset_y) {
        for (int offset_x = -SEARCH_RADIUS; offset_x <= SEARCH_RADIUS; ++offset_x) {

            Position candidate_position(
                ship_position.x + offset_x,
                ship_position.y + offset_y
            );

            candidate_position = game_map_ptr->normalize(candidate_position);

            int halite_on_cell =
                game_map_ptr->at(candidate_position)->halite;

            // Apply inspiration bonus (halite multiplier)
            if (inspired[candidate_position.y][candidate_position.x]) {
                halite_on_cell *= INSPIRED_MULTIPLIER;
            }

            int distance_to_cell =
                game_map_ptr->calculate_distance(ship_position, candidate_position);

            // Base score = halite divided by distance
            double score =
                static_cast<double>(halite_on_cell) /
                static_cast<double>(distance_to_cell + 1);

            // Penalize poor cells instead of ignoring them
            // NOTE: threshold still uses "raw" halite, this just reduces attraction to low-value areas.
            if (game_map_ptr->at(candidate_position)->halite < MIN_TARGET_HALITE) {
                score *= 0.25;
            }

            if (score > best_score) {
                best_score = score;
                best_position = candidate_position;
            }
        }
    }

    return best_position;
}

Direction decide_mining_direction(
    const shared_ptr<Ship>& ship,
    GameMap* game_map,
    ShipMemory& mem,
    const vector<vector<bool>>& next_turn_occupied,
    const vector<vector<bool>>& inspired
) {
    int halite_here = game_map->at(ship)->halite;

    // If current cell is rich enough, stay and mine
    // Apply inspiration bonus as an "effective halite" heuristic
    {
        bool is_inspired_here = inspired[ship->position.y][ship->position.x];
        int effective_halite_here = halite_here * (is_inspired_here ? INSPIRED_MULTIPLIER : 1);

        if (effective_halite_here >= STAY_MINE_THRESHOLD) {
            return Direction::STILL;
        }
    }

    Position current_target = mem.ship_target[ship->id];
    int target_halite_raw = game_map->at(current_target)->halite;

    // If target reached or became poor, choose a new one
    if (ship->position == current_target || target_halite_raw < MIN_TARGET_HALITE) {
        mem.ship_target[ship->id] = pick_mining_target(ship->position, game_map, inspired);
        current_target = mem.ship_target[ship->id];
    }

    return smart_navigate(ship, game_map, current_target, next_turn_occupied);
}
