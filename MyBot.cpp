#include "hlt/game.hpp"
#include "hlt/constants.hpp"
#include "hlt/log.hpp"

#include "hlt/bot_controller.hpp"

#include <random>
#include <ctime>

using namespace std;
using namespace hlt;

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

    game.ready("Colinatole");

    BotController bot(rng);

    for (;;) {
        game.update_frame();

        vector<Command> command_queue = bot.play_turn(game);

        if (!game.end_turn(command_queue)) {
            break;
        }
    }

    return 0;
}
