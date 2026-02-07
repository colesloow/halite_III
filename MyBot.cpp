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

    // Do any expensive pre-processing here; the per-turn 2s time limit starts after ready()
    game.ready("Colinatole");

    for (;;) {
        game.update_frame();
        shared_ptr<Player> me = game.me;
        unique_ptr<GameMap>& game_map = game.game_map;

        vector<Command> command_queue;

        // TODO(step 3): Replace random wandering with target selection (pick richer cells / local search)
        // TODO(step 4): Add collision avoidance between our own ships (reserve destinations each turn)
        for (const auto& ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;

			// Initialize ship state if ship is new
            if (ship_status.find(id) == ship_status.end()) {
                ship_status[id] = ShipState::MINING;
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
			else { // Step 3 (done): Replace random wandering with target selection (pick richer cells / local search)
				// If current cell is rich enough, stay and mine
                if (game_map->at(ship)->halite > constants::MAX_HALITE / 10) {
                    intended_direction = Direction::STILL;
                }
                else {
					// Look around and pick the one with the most halite ('greedy' local search)
                    int max_halite = -1;
                    Direction best_dir = Direction::STILL;

                    for (const auto& dir : ALL_CARDINALS) {
                        Position target_pos = ship->position.directional_offset(dir);
						// Check if the target cell is not occupied by another ship (collision avoidance)
                        if (!game_map->at(target_pos)->is_occupied()) {
                            int halite_at_target = game_map->at(target_pos)->halite;
                            if (halite_at_target > max_halite) {
                                max_halite = halite_at_target;
                                best_dir = dir;
                            }
                        }
                    }

					// If we found a better cell, move there
                    if (best_dir != Direction::STILL) {
                        intended_direction = best_dir;
                    }
                    else {
                        // Otherwise, fallback to random movement to avoid getting stuck
                        intended_direction = ALL_CARDINALS[rng() % 4];
                    }
                }
            }


            command_queue.push_back(final_command);
            // TODO(step 8): Consider enemy proximity (risk, inspiration)
        }

        // TODO(step 5): Improve spawn logic (stop earlier, avoid congestion)
        if (
            game.turn_number <= 200 &&
            me->halite >= constants::SHIP_COST &&
            !game_map->at(me->shipyard)->is_occupied())
        {
            command_queue.push_back(me->shipyard->spawn());
        }

        // TODO(step 7): Add dropoff creation logic (when/where to convert a ship)
        if (!game.end_turn(command_queue)) {
            break;
        }
    }

    return 0;
}
