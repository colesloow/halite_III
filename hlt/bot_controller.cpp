#include "bot_controller.hpp"

#include "bot_dropoff_planner.hpp"
#include "bot_mining.hpp"
#include "bot_navigation.hpp"
#include "bot_spawn.hpp"

#include <cstdint>

#ifdef _DEBUG
# define LOG(X) log::log(X);
#else
# define LOG(X)
#endif

BotController::BotController(mt19937& rng)
    : rng_(rng) {
}

vector<Command> BotController::play_turn(Game& game) {
    int turns_remaining = constants::MAX_TURNS - game.turn_number;

    shared_ptr<Player> me = game.me;
    unique_ptr<GameMap>& game_map = game.game_map;

    mem_.cleanup_dead_ships(me);

    int dynamic_max_ships = (game_map->width * game_map->height) / 18; // ~1 ship for 18 cells

    // Collision grid, empty grid initialized to false (indicating all cells are initially unoccupied)
    vector<vector<bool>> next_turn_occupied(game_map->height, vector<bool>(game_map->width, false));

    vector<vector<uint8_t>> enemy_count(game_map->height, vector<uint8_t>(game_map->width, 0));
    vector<vector<bool>> inspired(game_map->height, vector<bool>(game_map->width, false));

	// Danger map (enemy position + 4 adjacent cells)
    vector<vector<bool>> danger_map(game_map->height, vector<bool>(game_map->width, false));

    auto add_enemy_influence = [&](const Position& epos) {
        // Count enemies in a diamond (manhattan) radius around each enemy position.
        for (int dy = -INSPIRATION_RADIUS; dy <= INSPIRATION_RADIUS; ++dy) {
            int rem = INSPIRATION_RADIUS - std::abs(dy);
            for (int dx = -rem; dx <= rem; ++dx) {
                Position p(epos.x + dx, epos.y + dy);
                p = game_map->normalize(p);
                uint8_t& c = enemy_count[p.y][p.x];
                if (c < 255) ++c;
            }
        }
        };

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

            // Enemy cell is dangerous and occupied
            next_turn_occupied[pos.y][pos.x] = true;
            danger_map[pos.y][pos.x] = true;

			// The 4 adjacent cells are also dangerous (potentially occupied next turn)
            for (const auto& dir : ALL_CARDINALS) {
                Position adj = game_map->normalize(pos.directional_offset(dir));
                danger_map[adj.y][adj.x] = true;
            }

            // Inspiration counting (uses current enemy positions)
            add_enemy_influence(pos);
        }
    }

    for (int y = 0; y < game_map->height; ++y) {
        for (int x = 0; x < game_map->width; ++x) {
            inspired[y][x] = (enemy_count[y][x] >= INSPIRATION_SHIPS_REQUIRED);
        }
    }

    // Collision prevention: pre-pass for still ships (marking allied ships that will necessarily stay still in advance)
    for (const auto& ship_pair : me->ships) {
        Position pos = ship_pair.second->position;
        next_turn_occupied[pos.y][pos.x] = true;
    }

    // Anti-clumping grid
    vector<vector<bool>> claimed_targets(game_map->height, vector<bool>(game_map->width, false));

	// Pre-filling with targets of ships that are already in MINING mode
    for (const auto& ship_iterator : me->ships) {
        shared_ptr<Ship> ship = ship_iterator.second;
        mem_.ensure_initialized(ship);

        if (mem_.ship_status[ship->id] == ShipState::MINING) {
            Position target = mem_.ship_target[ship->id];
			// If the ship is not already on its target, it reserves it
            if (ship->position != target) {
                claimed_targets[target.y][target.x] = true;
            }
        }
    }

	// main ship loop
    for (const auto& ship_iterator : me->ships) {
        shared_ptr<Ship> ship = ship_iterator.second;
        EntityId id = ship->id;

        // Freeing the cell while thinking
        // even if we end up staying still, we will reserve it again with finalize_and_reserve_move
        next_turn_occupied[ship->position.y][ship->position.x] = false;

        // Dropoff construction logic (step 7)
        // Construction is considered only if we have the budget and enough time left
        // Keeping a security margin (SHIP_COST) to be able to spawn after if needed
        if (try_build_dropoff(ship, me, game_map.get(), turns_remaining, command_queue, next_turn_occupied)) {
            continue; // Skip the rest of the logic for this ship since it's now building a dropoff
        }

        mem_.ensure_initialized(ship);

        update_ship_state(ship, me, game_map.get(), turns_remaining, mem_);

        // Ensure the ship can afford to move from its current cell
        {
            int origin_halite = game_map->at(ship)->halite;
            int ratio = constants::MOVE_COST_RATIO;

            // Engine move cost is based on halite in the origin cell
            int move_cost = (origin_halite + ratio - 1) / ratio;

            // If we cannot afford to move, force STILL this turn.
            // This keeps next_turn_occupied consistnet with what will actually happen in the engine
            if (ship->halite < move_cost) {
                command_queue.push_back(
                    finalize_and_reserve_move(ship, game_map.get(), Direction::STILL, next_turn_occupied)
                );
                continue;
            }
        }

        // Targeting (step 4)
        Direction intended_direction = Direction::STILL;
        bool is_ship_inspired = inspired[ship->position.y][ship->position.x]; // Get inspiration status

        // Moving logic based on state
        if (mem_.ship_status[id] == ShipState::RETURNING) {
            intended_direction = decide_returning_direction(
                ship, me, game_map.get(), next_turn_occupied, danger_map, is_ship_inspired
            );
        }
        else {
            intended_direction = decide_mining_direction(
                ship, game_map.get(), mem_, next_turn_occupied, danger_map, inspired, claimed_targets
            );
        }

        intended_direction = apply_move_cost_safety(ship, game_map.get(), intended_direction);

        command_queue.push_back(
            finalize_and_reserve_move(ship, game_map.get(), intended_direction, next_turn_occupied)
        );
        // TODO(step 8): Consider enemy proximity (risk, inspiration)
    }

    try_spawn(me, game_map.get(), turns_remaining, next_turn_occupied, command_queue, dynamic_max_ships);

    return command_queue;
}
