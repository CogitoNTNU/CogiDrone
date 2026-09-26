#include "perception/models/depth_anything.h"



std::expected<DepthAnything, DepthAnything::InitError> DepthAnything::create() {
    // ! Fallible work FIRST: load engine, build context

    // TODO:
    // if (!engine_ok) return std::unexpected(InitError::ModelLoadFailed);

    return DepthAnything(M{ 
        /* 
        engine, 
        weights 
        */ 
    });
}

// ??:
// // DepthAnything::~DepthAnything() = default;                                                                // members clean up (engine handle)

// std::vector<DepthAnything::Detection> DepthAnything::distance(const Frame& frame) {
//     // preprocess -> infer -> decode
//     return {};
}