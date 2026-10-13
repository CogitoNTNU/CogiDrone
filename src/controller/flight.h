#pragma once

#include <memory>

#include "logging.h"


namespace Controller {

class Flight {
private:
    // * Ctors & dtor
    Flight();                                                                           // Default ctor
    ~Flight();                                                                          // Dtor

public: 
    // * Ctors & dtor   
    Flight(const Flight&) = delete;                                                     // Copy ctor: deleted
    Flight& operator=(const Flight&) = delete;                                          // Copy assignment: deleted
    Flight(Flight&&) = delete;                                                          // Move ctor: deleted
    Flight& operator=(Flight&&) = delete;                                               // Move assignment: deleted
};

} // namespace Controller
