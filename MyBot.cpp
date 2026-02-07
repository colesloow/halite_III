#include "hlt/game.hpp"
#include "hlt/constants.hpp"
#include "hlt/log.hpp"

#include <random>
#include <ctime>
#include <unordered_map>

using namespace std;
using namespace hlt;

#ifdef _DEBUG
# define LOG(X) log::log(X);
#else
# define LOG(X)
#endif // DEBUG

enum class ShipState {
    MINING,
    RETURNING
};

// Spawn tuning for 64x64 4 player games (step 5)
const int MAX_SHIPS = 24;            // Prevent over-fleeting and self-congestion
const int STOP_SPAWN_TURNS = 140;    // Stop spawning when game is getting late
const int HALITE_RESERVE = 1000;     // Keep some halite after spawning for flexibility
const int CONGESTION_RADIUS = 2;     // Manhattan distance around shipyard
const int CONGESTION_LIMIT = 3;      // If too many ships are nearby, do not spawn

// Mining targeting tuning (step 3 v2)
const int SEARCH_RADIUS = 8;         // How far a ship looks for a good mining cell
const int MIN_TARGET_HALITE = 120;   // Ignore very poor cells as targets
const int STAY_MINE_THRESHOLD = 100; // Stay still if current cell has enough halite

// Choose the best mining target around a ship
Position pick_mining_target(const Position& ship_position, GameMap* game_map_ptr) {

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

            int distance_to_cell =
                game_map_ptr->calculate_distance(ship_position, candidate_position);

            // Base score = halite divided by distance
            double score =
                static_cast<double>(halite_on_cell) /
                static_cast<double>(distance_to_cell + 1);

            // Penalize poor cells instead of ignoring them
            if (halite_on_cell < MIN_TARGET_HALITE) {
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

// Dropoff tuning (step 7)
const int DROPOFF_COST = 4000;
const int MIN_DIST_DROPOFF = 15;     // Mini distance between two dropoffs 
const double REQUIRED_HALITE_RADIUS = 7000.0; // Total halite required in the area around the dropoff (5x5)
const int MAX_DROPOFFS = 3;           // Arbitrary limit on number of dropoffs to prevent over-expansion

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

int main(int argc, char* argv[]) {
    unsigned int rng_seed;
    if (argc > 1) {
        rng_seed = static_cast<unsigned int>(stoul(argv[1]));
    }
    else {
        rng_seed = static_cast<unsigned int>(time(nullptr));
    }
    mt19937 rng(rng_seed);

    Game game;

	// Map to memorize the state of each ship between turns (step 2)
    unordered_map<EntityId, ShipState> ship_status;
    // Map to memorize a mining target for each ship between turns
    unordered_map<EntityId, Position> ship_target;


    // Do any expensive pre-processing here; the per-turn 2s time limit starts after ready()
    game.ready("Colinatole");

    for (;;) {
        game.update_frame();
        int turns_remaining = constants::MAX_TURNS - game.turn_number;

        shared_ptr<Player> me = game.me;
        unique_ptr<GameMap>& game_map = game.game_map;

        // Cleanup: remove entries for ships that have been destroyed
        for (auto status = ship_status.begin();
            status != ship_status.end(); ) {

            EntityId ship_id = status->first;

            // If the ship no longer exists in the current fleet, remove its state
            if (me->ships.find(ship_id) == me->ships.end()) {
                status = ship_status.erase(status);
            }
            else {
                ++status;
            }
        }

        // Cleanup: remove targets for ships that have been destroyed
        for (auto target = ship_target.begin();
            target != ship_target.end(); ) {

            EntityId ship_id = target->first;

            if (me->ships.find(ship_id) == me->ships.end()) {
                target = ship_target.erase(target);
            }
            else {
                ++target;
            }
        }

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
                    int local_halite = count_halite_in_area(ship->position, game_map.get(), 4);

                    if (local_halite >= REQUIRED_HALITE_RADIUS) {
						// Build a dropoff here
                        command_queue.push_back(ship->make_dropoff());

                        // IMPORTANT: On déduit virtuellement le coût tout de suite
                        // "Virtually" deducting halite to avoid multiple ships deciding to build dropoffs on the same turn
                        me->halite -= DROPOFF_COST;

                        // Marking the cell as occupied (dropoff is a structure)
                        next_turn_occupied[ship->position.y][ship->position.x] = true;

						continue; // Skip the rest of the logic for this ship since it's now building a dropoff
                    }
                }
            }

			// Initialize ship state if ship is new
            if (ship_status.find(id) == ship_status.end()) {
                ship_status[id] = ShipState::MINING;
            }

            // Initialize mining target if ship is new
            if (ship_target.find(id) == ship_target.end()) {
                ship_target[id] = ship->position;
            }

			// Step 6 (done): Endgame recall: force returning when remaining turns are low
            int dist_to_yard = game_map->calculate_distance(ship->position, me->shipyard->position);
			if (turns_remaining < dist_to_yard + 10) { // 10 is a safety margin
                ship_status[id] = ShipState::RETURNING;
            }

			// Step 2 (done): Add persistent per-ship state machine (MINING/RETURNING)
            if (ship_status[id] == ShipState::RETURNING) {
                if (ship->position == me->shipyard->position) {
					// If we're on the shipyard, we go back to mining
                    ship_status[id] = ShipState::MINING;
                }
                else if (ship->halite == 0) {
					// Secure: if we have no halite, we should mine more before returning
                    ship_status[id] = ShipState::MINING;
                }
            }
            else {
                if (ship->halite >= constants::MAX_HALITE * 0.95) {
                    // If we're 95% full, we return to the shipyard to deposit
                    ship_status[id] = ShipState::RETURNING;
                }
            }

            // Targeting (step 4)
            Direction intended_direction = Direction::STILL;

			// Moving logic based on state
            if (ship_status[id] == ShipState::RETURNING) {
				// Go back to shipyard
                intended_direction = game_map->naive_navigate(ship, me->shipyard->position);
            }
			else { // Step 3 v2: target-based mining

                int halite_here = game_map->at(ship)->halite;

                // If current cell is rich enough, stay and mine
                if (halite_here >= STAY_MINE_THRESHOLD) {
                    intended_direction = Direction::STILL;
                }
                else {

                    Position current_target = ship_target[id];
                    int target_halite = game_map->at(current_target)->halite;

                    // If target reached or became poor, choose a new one
                    if (ship->position == current_target || target_halite < MIN_TARGET_HALITE) {
                        ship_target[id] = pick_mining_target(ship->position, game_map.get());
                        current_target = ship_target[id];
                    }

                    // Move toward the target
                    vector<Direction> possible_directions =
                        game_map->get_unsafe_moves(ship->position, current_target);

                    Direction chosen_direction = Direction::STILL;

                    for (const auto& dir : possible_directions) {

                        Position candidate =
                            game_map->normalize(ship->position.directional_offset(dir));

                        if (next_turn_occupied[candidate.y][candidate.x]) {
                            continue;
                        }

                        if (game_map->at(candidate)->is_occupied()) {
                            continue;
                        }

                        chosen_direction = dir;
                        break;
                    }

                    // If we cannot move toward the target, try any free cardinal move to avoid deadlocks
                    if (chosen_direction == Direction::STILL) {
                        for (const auto& dir : ALL_CARDINALS) {
                            Position candidate = game_map->normalize(ship->position.directional_offset(dir));

                            if (next_turn_occupied[candidate.y][candidate.x]) {
                                continue;
                            }

                            if (game_map->at(candidate)->is_occupied()) {
                                continue;
                            }

                            chosen_direction = dir;
                            break;
                        }
                    }

                    intended_direction = chosen_direction;
                }
            }

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

            command_queue.push_back(final_command);
            // TODO(step 8): Consider enemy proximity (risk, inspiration)
        }

        // Step 5 (done): Improve spawn logic (stop earlier, avoid congestion)
        Position yard_pos = me->shipyard->position;

        // Count our ships close to shipyard to avoid congestion
        int nearby_ships = 0;
        for (const auto& ship_entry : me->ships) {
            std::shared_ptr<Ship> ship = ship_entry.second;
            int dist = game_map->calculate_distance(ship->position, yard_pos);
            if (dist <= CONGESTION_RADIUS) {
                nearby_ships++;
            }
        }

        // Spawn based on conditions
        bool can_spawn =
            (turns_remaining > STOP_SPAWN_TURNS) &&
            (me->ships.size() < static_cast<size_t>(MAX_SHIPS)) &&
            (me->halite >= constants::SHIP_COST + HALITE_RESERVE) &&
            (nearby_ships < CONGESTION_LIMIT) &&
            (!next_turn_occupied[yard_pos.y][yard_pos.x]);

        if (can_spawn) {
            command_queue.push_back(me->shipyard->spawn());
            // Marking shipyard position as occupied for the next turn to prevent collisions with newly spawned ship
            next_turn_occupied[yard_pos.y][yard_pos.x] = true;
        }

        if (!game.end_turn(command_queue)) {
            break;
        }
    }

    return 0;
}
