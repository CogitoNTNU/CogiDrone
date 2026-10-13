#pragma once

#include <memory>

#include "logging.h"



/**
 * @brief The Pixhawk class represents the Pixhawk flight controller.
 * This class is responsible for communicating with the Pixhawk flight controller 
 * and managing its state; e.g.: Pixhawk.arm();
 */
class Pixhawk {
private:
    // * Ctors & dtor
    Pixhawk();                                                                          // Default ctor
    ~Pixhawk();                                                                         // Dtor
    
public:     
    // * Ctors & dtor       
    Pixhawk(const Pixhawk &) = delete;                                                  // Copy ctor: deleted
    Pixhawk& operator=(const Pixhawk &) = delete;                                       // Copy assignment: deleted
    Pixhawk(Pixhawk &&) = delete;                                                       // Move ctor: deleted
    Pixhawk& operator=(Pixhawk &&) = delete;                                            // Move assignment: deleted
};

