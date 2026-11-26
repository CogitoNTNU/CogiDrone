#pragma once


// * Inference backend
// ? One backend per build. They cannot coexist:
// ?   Jetson  -> TensorRT     (GPU, .engine plan, JetPack only)
// ?   Sim/PC  -> ONNX Runtime (CPU, .onnx model, portable)
// ! The sim container has no TensorRT and no CUDA. The Jetson has no ONNX
// ! Runtime. Selecting at build time keeps both out of each other's way.
// ?
// ? Both classes expose the SAME four members, so `Yolo<Layout>` does not know
// ? which one it holds:
// ?     create(path, inputName, outputName) -> std::expected<Engine, InitError>
// ?     inputBuffer()  -> std::span<float>
// ?     infer()        -> std::span<const float>
// ?     outputShape()  -> const std::vector<std::size_t>&
#if defined(COGIDRONE_BACKEND_ONNX)
    #include "perception/models/onnx_engine.h"
    namespace cogidrone {
        using Engine = OnnxEngine;
        inline constexpr std::string_view MODEL_EXTENSION = ".onnx";
    }
#else
    #include "perception/models/trt_engine.h"
    namespace cogidrone {
        using Engine = TrtEngine;
        inline constexpr std::string_view MODEL_EXTENSION = ".engine";
    }
#endif
