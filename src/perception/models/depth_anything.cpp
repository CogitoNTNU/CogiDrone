#include "perception/models/depth_anything.h"



std::expected<DepthAnything, DepthAnything::InitError> DepthAnything::create() {
    // ! Fallible work FIRST: load the TensorRT plan

    // TODO: [#40] Load the engine and fail if it is unavailable:
    //     : auto engine = TrtEngine::create("models/depth_anything_v2_small.engine",
    //     :                                "pixel_values", "predicted_depth");
    //     : if (!engine) return std::unexpected(InitError::ModelLoadFailed);

    return DepthAnything(M{ 
        .engine = nullptr                                                            // ? will be wired in [#40]
    });
}