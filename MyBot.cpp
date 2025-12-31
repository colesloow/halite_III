#include "hlt/game.hpp"
#include "hlt/constants.hpp"
#include "hlt/log.hpp"

#include <random>
#include <ctime>

using namespace std;
using namespace hlt;

#ifdef _DEBUG
# define LOG(X) log::log(X);
#else
# define LOG(X)
#endif // DEBUG

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

    // Do any expensive pre-processing here; the per-turn 2s time limit starts after ready()
    game.ready("Colinatole");

    for (;;) {
        game.update_frame();
        shared_ptr<Player> me = game.me;
        unique_ptr<GameMap>& game_map = game.game_map;

        vector<Command> command_queue;

        // TODO(step 2): Add persistent per-ship state (MINING/RETURNING) and a return threshold (not only full)
        // TODO(step 3): Replace random wandering with target selection (pick richer cells / local search)
        // TODO(step 4): Add collision avoidance between our own ships (reserve destinations each turn)
        for (const auto& ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;

            // Step 1 (done):If ship is full, go back to shipyard to deposit
            if (ship->is_full()) {
                Direction dir_to_yard = game_map->naive_navigate(ship, me->shipyard->position);
                command_queue.push_back(ship->move(dir_to_yard));
            }
            else if (game_map->at(ship)->halite < constants::MAX_HALITE / 10) {
                Direction random_direction = ALL_CARDINALS[rng() % 4];
                command_queue.push_back(ship->move(random_direction));
            }
            else {
                command_queue.push_back(ship->stay_still());
            }

            // TODO(step 6): Endgame recall: force returning when turns remaining is low
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
