#include "bot_navigation.hpp"

// Returns the closest deposit structure from a given position
Position get_nearest_deposit_position(
    const shared_ptr<Player>& me,
    GameMap* game_map,
    const Position& from
) {
    // Start by assuming the shipyard is the closest deposit
    Position best_pos = me->shipyard->position;

    // Compute distnace from current position to shipyard
    int best_dist = game_map->calculate_distance(from, best_pos);

    // Iterate over all existing dropoffs
    for (const auto& dropoff_entry : me->dropoffs) {
        Position drop_pos = dropoff_entry.second->position;

        // Compute distance from current position to this dropoff
        int d = game_map->calculate_distance(from, drop_pos);

        // If this dropoff is closer than the current best,
        // update best distance and best position
        if (d < best_dist) {
            best_dist = d;
            best_pos = drop_pos;
        }
    }

    return best_pos;
}

void update_ship_state(
    const shared_ptr<Ship>& ship,
    const shared_ptr<Player>& me,
    GameMap* game_map,
    int turns_remaining,
    ShipMemory& mem
) {
    EntityId id = ship->id;

    // Step 6 (done): Endgame recall: force returning when remaining turns are low
    Position nearest_deposit_pos = get_nearest_deposit_position(me, game_map, ship->position);
    int dist_to_deposit = game_map->calculate_distance(ship->position, nearest_deposit_pos);

    if (turns_remaining < dist_to_deposit + 10) { // 10 is a safety margin
        mem.ship_status[id] = ShipState::RETURNING;
    }

    // Step 2 (done): Add persistent per-ship state machine (MINING/RETURNING)
    if (mem.ship_status[id] == ShipState::RETURNING) {
        if (ship->position == nearest_deposit_pos) {
            // If we're on the shipyard, we go back to mining
            mem.ship_status[id] = ShipState::MINING;
        }
        else if (ship->halite == 0) {
            // Secure: if we have no halite, we should mine more before returning
            mem.ship_status[id] = ShipState::MINING;
        }
    }
    else {
        if (ship->halite >= constants::MAX_HALITE * 0.95) {
            // If we're 95% full, we return to the shipyard to deposit
            mem.ship_status[id] = ShipState::RETURNING;
        }
    }
}

Direction decide_returning_direction(
    const shared_ptr<Ship>& ship,
    const shared_ptr<Player>& me,
    GameMap* game_map
) {
    // Moving logic based on state
    // Go back to shipyard
    Position nearest_deposit_pos = get_nearest_deposit_position(me, game_map, ship->position);
    return game_map->naive_navigate(ship, nearest_deposit_pos);
}

Direction apply_move_cost_safety(
    const shared_ptr<Ship>& ship,
    GameMap* game_map,
    Direction intended_direction
) {
    // Movement cost safety: do not issue a move we cannot afford
    if (intended_direction != Direction::STILL) {
        int origin_halite = game_map->at(ship)->halite;
        int ratio = constants::MOVE_COST_RATIO;

        // Engine move cost is based on halite in the origin cell(use ceil division for safety)
        int move_cost = (origin_halite + ratio - 1) / ratio;

        if (ship->halite < move_cost) {
            intended_direction = Direction::STILL;
        }
    }

    return intended_direction;
}

Command finalize_and_reserve_move(
    const shared_ptr<Ship>& ship,
    GameMap* game_map,
    Direction intended_direction,
    vector<vector<bool>>& next_turn_occupied
) {
    // Step 4 (done): Add collision avoidance between our own ships (reserve destinations each turn)
    Command final_command = ship->stay_still(); // Don't move by default (in case we need to stay still due to collisions)
    Position final_target = ship->position;     // Target position we intend to move to (initially our current position)

    // Checking the intended move's target position, if it's occupied, we stay still
    // UPGRADE: Checking adjacent cells for an alternative move

    Position target_pos = game_map->normalize(ship->position.directional_offset(intended_direction));

    // If cell is free in the next turn, we can move there
    if (!next_turn_occupied[target_pos.y][target_pos.x]) {
        final_command = ship->move(intended_direction);
        final_target = target_pos;
    }
    else {
        // Otherwise, we stay still to avoid collision
        // BUT if we stay still, we need to make sure to mark our current position as occupied
        // Since every ship move in order, if we stay still, we will occupy our current cell in the next turn
        // So it should be safe
        final_command = ship->stay_still();
        final_target = ship->position;
    }

    // Marking the final target position as occupied
    next_turn_occupied[final_target.y][final_target.x] = true;

    return final_command;
}
