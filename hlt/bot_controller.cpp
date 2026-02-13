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

    // Collision grid, empty grid initialized to false (indicating all cells are initially unoccupied)
    vector<vector<bool>> next_turn_occupied(game_map->height, vector<bool>(game_map->width, false));

    // Inspiration map (per turn): a cell is inspird if >= 2 enemy ships are within manhattan distance 4
    static const int INSPIRATION_RADIUS = 4;
    static const int INSPIRATION_SHIPS_REQUIRED = 2;

    vector<vector<uint8_t>> enemy_count(game_map->height, vector<uint8_t>(game_map->width, 0));
    vector<vector<bool>> inspired(game_map->height, vector<bool>(game_map->width, false));

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
            next_turn_occupied[pos.y][pos.x] = true;

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
    for (const auto& ship_iterator : me->ships) {
        shared_ptr<Ship> ship = ship_iterator.second;

        int origin_halite = game_map->at(ship)->halite;
        int move_cost = (origin_halite + constants::MOVE_COST_RATIO - 1) / constants::MOVE_COST_RATIO;

        bool will_stay_still = false;

        // Reason 1 : Not enough fuel to move
        if (ship->halite < move_cost) {
            will_stay_still = true;
        }
        // Reason 2 : Very profitable mining
        else {
            mem_.ensure_initialized(ship);
            if (mem_.ship_status[ship->id] == ShipState::MINING) {
                bool is_ship_inspired = inspired[ship->position.y][ship->position.x];
                int effective_halite = origin_halite * (is_ship_inspired ? INSPIRED_MULTIPLIER : 1);

                if (effective_halite >= STAY_MINE_THRESHOLD) {
                    will_stay_still = true;
                }
            }
        }

        if (will_stay_still) {
            next_turn_occupied[ship->position.y][ship->position.x] = true;
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
            intended_direction = decide_returning_direction(ship, me, game_map.get(), next_turn_occupied, is_ship_inspired);
        }
        else {
            intended_direction = decide_mining_direction(ship, game_map.get(), mem_, next_turn_occupied, inspired);
        }

        intended_direction = apply_move_cost_safety(ship, game_map.get(), intended_direction);

        command_queue.push_back(
            finalize_and_reserve_move(ship, game_map.get(), intended_direction, next_turn_occupied)
        );
        // TODO(step 8): Consider enemy proximity (risk, inspiration)
    }

    try_spawn(me, game_map.get(), turns_remaining, next_turn_occupied, command_queue);

    return command_queue;
}
