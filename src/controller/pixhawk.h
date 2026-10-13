#pragma once

#include <memory>

#include "logging.h"



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

