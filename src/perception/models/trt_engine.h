#pragma once

#include <expected>
#include <memory>
#include <string>
#include <vector>
#include <cstddef>

#include <NvInfer.h>
#include <cuda_runtime_api.h>


// * TensorRT engine
// ? Owns one serialized TensorRT plan and everything needed to run it:
// ? runtime, engine, execution context, device buffers, CUDA stream.
// ?
// ! THE ENGINE IS BUILT ON THE JETSON, NOT SHIPPED
// ? A .engine file is a serialized plan for ONE GPU architecture and ONE
// ? TensorRT version. An engine built on an RTX 4090 will not load on an Orin,
// ? and one built under TRT 8.6 will not load under TRT 10. The build step is:
// ?     models/best.pt   --(yolo export format=onnx)-->  models/best.onnx
// ?     models/best.onnx --(trtexec --fp16)-->           models/best.engine
// ? See [#35].
// ?
// ! EXPORT WITHOUT NMS
// ? `nms=False` (the ultralytics default). With `nms=True` the ONNX graph
// ? embeds the EfficientNMS_TRT plugin, which needs libnvinfer_plugin.so at
// ? runtime and makes the engine non-portable. NMS runs on the CPU instead -
// ? ~1 ms for 8400 anchors.
// ?
// ! VERSION SENSITIVITY
// ? This targets TensorRT 10 (JetPack 6+). TRT 8 uses enqueueV2 with a bindings
// ? array instead of enqueueV3 with named tensor addresses, and getBindingDimensions
// ? instead of getTensorShape. The difference is isolated in infer() and create().
class TrtEngine {
public:
    enum class InitError {
        EngineLoadFailed,
        ContextCreateFailed,
        TensorNameNotFound,
        StreamCreateFailed,
        BufferAllocationFailed,
    };

private:
    // * RAII for the TensorRT and CUDA handles
    // ? TensorRT objects are created with `new`. In TensorRT 10 their destructors
    // ? are public, so `delete` is correct and `destroy()` is deprecated.
    // ! On TensorRT 8 the destructors are protected and `destroy()` is required.
    // ! If this ever needs to build against TRT 8, change this deleter to call
    // ! `object->destroy()`.
    struct TrtDeleter {
        template <typename T>
        void operator()(T* object) const noexcept {
            delete object;
        }
    };

    struct CudaDeleter {
        void operator()(void* devicePointer) const noexcept {
            if (devicePointer) cudaFree(devicePointer);
        }
    };

    struct M {                                                                          // Assembly struct
        // TensorRT
        std::unique_ptr<nvinfer1::IRuntime, TrtDeleter> runtime;
        std::unique_ptr<nvinfer1::ICudaEngine, TrtDeleter> engine;
        std::unique_ptr<nvinfer1::IExecutionContext, TrtDeleter> context;

        // CUDA
        cudaStream_t stream = nullptr;
        std::unique_ptr<void, CudaDeleter> deviceInput;
        std::unique_ptr<void, CudaDeleter> deviceOutput;

        // Shapes
        std::string inputName;
        std::string outputName;
        std::size_t inputBytes = 0;
        std::size_t outputBytes = 0;
        std::vector<std::size_t> outputShape;                                           // e.g. {1, 10, 8400}
    } m;

    explicit TrtEngine(M&& m) : m(std::move(m)) {}

    // ? Returns expected, unlike the other subsystems' wire(): cudaMalloc and
    // ? cudaStreamCreate can genuinely fail, and a silently-null device buffer
    // ? would segfault on the first inference.
    [[nodiscard]] std::expected<void, InitError> wire();

public:
    [[nodiscard]] static std::expected<TrtEngine, InitError> create(
        const std::string& enginePath,
        const std::string& inputName,
        const std::string& outputName
    );

    ~TrtEngine();

    TrtEngine(TrtEngine&&) noexcept = default;                                          // movable - handles transfer
    TrtEngine& operator=(TrtEngine&&) noexcept = default;
    TrtEngine(const TrtEngine&) = delete;                                               // non-copyable (one context)
    TrtEngine& operator=(const TrtEngine&) = delete;

    // * Inference
    // ! DOUBLE CHECK:
    // TODO: Reduce copy overhead by passing a preallocated output vector to fill, or a pointer to the output buffer.
    // ? Copies `input` to the device, runs the engine, copies the output back.
    // ? Returns the output as a flat float vector; the caller reshapes using
    // ? outputShape().
    // ! Synchronous: it blocks on the stream. That is deliberate - Perception
    // ! runs on the executor thread and the next frame cannot be processed
    // ! until this one is done anyway.
    // ! NOT const: it writes the device buffers and rebinds the context's tensor
    // ! addresses. Marking it const would be a lie.
    [[nodiscard]] std::vector<float> infer(const std::vector<float>& input);

    // * Accessors
    [[nodiscard]] inline const std::vector<std::size_t>& outputShape() const noexcept { return m.outputShape; }
    [[nodiscard]] inline std::size_t inputElementCount() const noexcept { return m.inputBytes / sizeof(float); }
};