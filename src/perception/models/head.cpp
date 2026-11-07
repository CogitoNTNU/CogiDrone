#include "perception/models/head.h"



std::expected<Head, Head::InitError> Head::create() {
    // ! Fallible work FIRST: load engine, build context

    // TODO: [#19] Load the engine and weights, then fail if either is unavailable:
    //     : if (!engine_ok) return std::unexpected(InitError::ModelLoadFailed);

    return Head(M{ 
        /* 
        engine, 
        weights 
        */ 
    });
}

// ??:
// // Head::~Head() = default;                                                                // members clean up (engine handle)

// std::vector<Head::Detection> Head::detect(const Frame& frame) {
//     // preprocess -> infer -> decode
//     return {};
// }