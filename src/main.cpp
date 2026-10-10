#include "drone.h"
#include "logging.h"


int main(int argc, const char* argv[]) {
    // | Set logging level
    setLoggingLevel(LOGGING_LEVEL::DEBUG);

    // Instantiate the Drone class
    auto result = Drone::initiate(argc, argv);

    // Ensure correct initialization
    if (!result) {
        std::cerr << "Drone initialization failed: " << result.error().message << std::endl;
        return 1;
    }

    cogidrone::log(LOGGING_LEVEL::INFO, "Drone initialized successfully");

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
    cogidrone::log(LOGGING_LEVEL::INFO, "Drone object created successfully");
    
    // Start the drone on the main thread, and let it run until the drone is stopped.
    cogidrone::log(LOGGING_LEVEL::INFO, "Starting drone...");
    drone->start();
 
    return 0;
}
