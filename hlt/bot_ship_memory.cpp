#include "bot_ship_memory.hpp"

void ShipMemory::cleanup_dead_ships(const shared_ptr<Player>& me) {
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
}

void ShipMemory::ensure_initialized(const shared_ptr<Ship>& ship) {
    EntityId id = ship->id;

    // Initialize ship state if ship is new
    if (ship_status.find(id) == ship_status.end()) {
        ship_status[id] = ShipState::MINING;
    }

    // Initialize mining target if ship is new
    if (ship_target.find(id) == ship_target.end()) {
        ship_target[id] = ship->position;
    }
}
