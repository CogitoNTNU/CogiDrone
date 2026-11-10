#pragma once

#include <expected>
#include <memory>
#include <string>
#include <vector>
#include <span>
#include <cstddef>

#include <NvInfer.h>
#include <cuda_runtime_api.h>


// * TensorRT engine
// ? Owns one serialized TensorRT plan and everything needed to run it:
// ? runtime, engine, execution context, device buffers, CUDA stream, and the
// ? host staging buffers.
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
// ?
// ! ZERO ALLOCATION PER INFERENCE
// ? The host input and output buffers are allocated ONCE in wire() and reused
// ? for the engine's whole life. `infer()` returns a view into the output
// ? buffer, not a new vector. See [#41].
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

        // Host staging buffers
        // ? Allocated ONCE in wire(), sized from the engine's tensor shapes.
        // ? `infer()` reads hostInput and writes hostOutput, so neither is
        // ? allocated per call. This is the whole point of [#41].
        std::vector<float> hostInput;
        std::vector<float> hostOutput;

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

    // ? TrtEngine is movable because it has no address-sensitive state:
    // ? callbacks never capture its `this` pointer, and all resources needed
    // ? for inference are owned by `m`. Factory functions return complete
    // ? TrtEngine values through std::expected and move-construct them into
    // ? their final Yolo/model owners. This is ownership transfer during
    // ? construction, not exposure of a partially initialized object.
    TrtEngine(TrtEngine&& other) noexcept;                                              // movable - handles transfer
    
    TrtEngine& operator=(TrtEngine&&) noexcept = delete;                                // move assignment deleted because engines are initialized into their final owners
    TrtEngine(const TrtEngine&) = delete;                                               // non-copyable (one context)
    TrtEngine& operator=(const TrtEngine&) = delete;

    // * Inference
    // ? Two-step, so the caller writes the input tensor directly into the
    // ? staging buffer with no intermediate copy:
    // ?
    // ?     std::span<float> input = engine.inputBuffer();
    // ?     // ... fill input with exactly inputElementCount() floats ...
    // ?     std::span<const float> output = engine.infer();
    // ?
    // ! `inputBuffer()` returns a view into a member. It is valid until the
    // ! next call to inputBuffer() or infer(). Do not store it.
    // ! `infer()` returns a view into a member. It is valid until the next
    // ! call to infer(). Consume it before then.
    // ! Synchronous: it blocks on the stream. That is deliberate - Perception
    // ! runs on the executor thread and the next frame cannot be processed
    // ! until this one is done anyway.
    // ! NOT const: it writes the device buffers and rebinds the context's tensor
    // ! addresses. Marking it const would be a lie.
    // !
    // ! `inputBuffer()` is NOT const either, and that is not an oversight: a
    // ! const member function sees `m.hostInput` as `const std::vector<float>`,
    // ! from which only `std::span<const float>` can be formed. The caller must
    // ! be able to WRITE into this buffer, so the function cannot be const.
    [[nodiscard]] inline std::span<float> inputBuffer() noexcept { return m.hostInput; }
    [[nodiscard]] std::span<const float> infer();

    // * Accessors
    [[nodiscard]] inline const std::vector<std::size_t>& outputShape() const noexcept { return m.outputShape; }
    [[nodiscard]] inline std::size_t inputElementCount() const noexcept { return m.inputBytes / sizeof(float); }
};