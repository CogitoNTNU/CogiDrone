#include "drone.h"


int main(int argc, char* argv[]) {
    // Instantiate the Drone class
    auto drone = Drone::initiate(argc, argv);

    // Ensure correct initialization
    if (!drone) {
        // handle: log InitError, exit — right here, not 10 minutes into a flight
        return 1;
    }

    // Start the drone on the main thread, and let it run until the user presses Ctrl+C or the drone is stopped.
    (*drone)->start();
 
    return 0;
}
