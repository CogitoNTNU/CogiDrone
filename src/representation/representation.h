#pragma once

#include <vector>


class Representation {
private:
    int data;         // Example private member variable
    void _update();

public:
    Representation();
    ~Representation();

    void start();
    void update();

};