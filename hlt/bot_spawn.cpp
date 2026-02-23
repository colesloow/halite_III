#include "bot_spawn.hpp"

void try_spawn(
    const shared_ptr<Player>& me,
    GameMap* game_map_ptr,
    int turns_remaining,
    vector<vector<bool>>& next_turn_occupied,
    vector<Command>& command_queue,
    int max_ships
) {
    // Improve spawn logic (stop earlier, avoid congestion)
    Position yard_pos = me->shipyard->position;

    // Count our ships close to shipyard to avoid congestion
    int nearby_ships = 0;
    for (const auto& ship_entry : me->ships) {
        shared_ptr<Ship> ship = ship_entry.second;
        int dist = game_map_ptr->calculate_distance(ship->position, yard_pos);
        if (dist <= CONGESTION_RADIUS) {
            nearby_ships++;
        }
    }

    // Spawn based on conditions
    bool can_spawn =
        (turns_remaining > STOP_SPAWN_TURNS) &&
        (me->ships.size() < static_cast<size_t>(max_ships)) &&
        (me->halite >= constants::SHIP_COST + HALITE_RESERVE) &&
        (nearby_ships < CONGESTION_LIMIT) &&
        (!next_turn_occupied[yard_pos.y][yard_pos.x]);

    if (can_spawn) {
        command_queue.push_back(me->shipyard->spawn());
        // Marking shipyard position as occupied for the next turn to prevent collisions with newly spawned ship
        next_turn_occupied[yard_pos.y][yard_pos.x] = true;
    }
}
