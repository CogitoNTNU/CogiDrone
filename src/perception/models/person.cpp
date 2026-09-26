#include "perception/models/person.h"



std::expected<Yolo, Yolo::InitError> Yolo::create() {
    // ! Fallible work FIRST: load engine, build context

    // TODO:
    // if (!engine_ok) return std::unexpected(InitError::ModelLoadFailed);

    return Yolo(M{ 
        /* 
        engine, 
        weights 
        */ 
    });
}

// ??:
// // Yolo::~Yolo() = default;                                                                // members clean up (engine handle)

std::vector<Yolo::Detection> Yolo::detect(const Frame& frame) {
    // preprocess -> infer -> decode
    return {};
}