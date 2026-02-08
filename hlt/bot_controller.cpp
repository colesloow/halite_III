#include "bot_controller.hpp"

#include "bot_dropoff_planner.hpp"
#include "bot_mining.hpp"
#include "bot_navigation.hpp"
#include "bot_spawn.hpp"

#ifdef _DEBUG
# define LOG(X) log::log(X);
#else
# define LOG(X)
#endif // DEBUG

BotController::BotController(mt19937& rng)
    : rng_(rng) {
}

vector<Command> BotController::play_turn(Game& game) {
    int turns_remaining = constants::MAX_TURNS - game.turn_number;

    shared_ptr<Player> me = game.me;
    unique_ptr<GameMap>& game_map = game.game_map;

    mem_.cleanup_dead_ships(me);

    // Collision grid, empty grid initialized to false (indicating all cells are initially unoccupied)
    vector<vector<bool>> next_turn_occupied(game_map->height, vector<bool>(game_map->width, false));

    vector<Command> command_queue;

    // Marking enemy ship positions as occupied to avoid crashing into them
    // Optional but safe to start with
    // UPGRADE: can change for more aggressive play later
    for (const auto& player_ptr : game.players) {
        if (player_ptr->id == me->id) continue; // Ignoring our own ships for now
        for (const auto& ship_pair : player_ptr->ships) {
            // Marking enemy ship's current position as dangerous (simplification, since they can move)
            // UPGRADE: Marking adjacent cells as well to account for their possible moves
            Position pos = ship_pair.second->position;
            next_turn_occupied[pos.y][pos.x] = true;
        }
    }

    for (const auto& ship_iterator : me->ships) {
        shared_ptr<Ship> ship = ship_iterator.second;
        EntityId id = ship->id;

        // Dropoff construction logic (step 7)
        // Construction is considered only if we have the budget and enough time left
        // Keeping a security margin (SHIP_COST) to be able to spawn after if needed
        if (try_build_dropoff(ship, me, game_map.get(), turns_remaining, command_queue, next_turn_occupied)) {
            continue; // Skip the rest of the logic for this ship since it's now building a dropoff
        }

        mem_.ensure_initialized(ship);

        update_ship_state(ship, me, game_map.get(), turns_remaining, mem_);

        // Targeting (step 4)
        Direction intended_direction = Direction::STILL;

        // Moving logic based on state
        if (mem_.ship_status[id] == ShipState::RETURNING) {
            intended_direction = decide_returning_direction(ship, me, game_map.get());
        }
        else {
            intended_direction = decide_mining_direction(ship, game_map.get(), mem_, next_turn_occupied);
        }

        intended_direction = apply_move_cost_safety(ship, game_map.get(), intended_direction);

        command_queue.push_back(finalize_and_reserve_move(ship, game_map.get(), intended_direction, next_turn_occupied));
        // TODO(step 8): Consider enemy proximity (risk, inspiration)
    }

    try_spawn(me, game_map.get(), turns_remaining, next_turn_occupied, command_queue);

    return command_queue;
}
