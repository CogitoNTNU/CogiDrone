#include "perception/models/person.h"



std::expected<Person, Person::InitError> Person::create() {
    // ! Fallible work FIRST: load engine, build context

    // TODO:
    // if (!engine_ok) return std::unexpected(InitError::ModelLoadFailed);

    return Person(M{ 
        /* 
        engine, 
        weights 
        */ 
    });
}

// ??:
// // Person::~Person() = default;                                                                // members clean up (engine handle)

std::vector<Person::Detection> Person::detect(const Frame& frame) {
    // preprocess -> infer -> decode
    return {};
}