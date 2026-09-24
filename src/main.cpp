#include "drone.h"


int main(int argc, const char* argv[]) {
    // Instantiate the Drone class
    auto result = Drone::initiate(argc, argv);

    // Ensure correct initialization
    if (!result) {
        // TODO: 
        //     : - handle: log InitError
        //     : - exit, right here, not 10 minutes into a flight
        return 1;
    }

    // ? `auto drone` is a std::expected<std::unique_ptr<Drone>, InitError>:
    // ? drone : std::expected<std::unique_ptr<Drone>, InitError>
    // ?      └── contains either:
    // ?           ├── unique_ptr<Drone>  ->  Drone      (success)
    // ?           └── InitError                         (failure) 
    // ? 
    // ? To start the drone, we therefore must:
    // ?  1. Dereference the expected    -> gives us the unique_ptr<Drone>
    // ?  2. Dereference the unique_ptr  -> gives us the Drone&
    // ?  3. Call the start() method on the Drone&

    // Move the unique_ptr out of the expected once verified
    std::unique_ptr<Drone> drone = std::move(*result);                                  // Move the unique_ptr out of the expected once verified

    // Start the drone on the main thread, and let it run until the drone is stopped.
    drone->start();
 
    return 0;
}
