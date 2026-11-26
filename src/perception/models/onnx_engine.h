#pragma once

#include <expected>
#include <memory>
#include <string>
#include <vector>
#include <span>
#include <cstddef>

#include <onnxruntime_cxx_api.h>


// * ONNX Runtime engine
// ? The CPU counterpart to TrtEngine. Same public interface, same ownership
// ? model, same zero-allocation-per-inference contract.
// ?
// ! WHY THIS EXISTS
// ? A .engine file is a TensorRT plan for ONE GPU architecture and ONE
// ? TensorRT version. The sim container has neither CUDA nor TensorRT, so it
// ? cannot load one. A .onnx file is portable and runs on CPU.
// ?
// ! ZERO ALLOCATION PER INFERENCE
// ? The input and output tensors are bound ONCE in create() via Ort::IoBinding.
// ? `infer()` reuses those bindings, so no tensor is allocated per call.
// ? The naive `Session::Run(inputs, outputs)` overload allocates a fresh output
// ? tensor every time - that is the trap this avoids. See [#41].
class OnnxEngine {
public:
    enum class InitError {
        EngineLoadFailed,
        DynamicShapeUnsupported,
        BufferAllocationFailed,
    };

private:
    struct M {                                                                          // Assembly struct
        // ONNX Runtime
        // ? `Ort::Env` is NOT owned here - see sharedEnv() in the .cpp. One Env
        // ? per process is the documented recommendation, and two models
        // ? (person + head) would otherwise create two.
        std::unique_ptr<Ort::Session> session;
        std::unique_ptr<Ort::IoBinding> binding;

        // ? These WRAP hostInput/hostOutput. `CreateTensor` does not copy - it
        // ? stores the pointer. The vectors are sized once and never resized,
        // ? so the pointers stay valid for the engine's whole life.
        std::unique_ptr<Ort::Value> inputTensor;
        std::unique_ptr<Ort::Value> outputTensor;

        // Host buffers
        // ? Allocated ONCE in create(), sized from the model's tensor shapes.
        std::vector<float> hostInput;
        std::vector<float> hostOutput;

        // Shapes
        std::string inputName;
        std::string outputName;
        std::size_t inputBytes;
        std::size_t outputBytes;
        std::vector<std::size_t> outputShape;                                           // e.g. {1, 300, 6}
    } m;

    explicit OnnxEngine(M&& m) : m(std::move(m)) {}

public:
    [[nodiscard]] static std::expected<OnnxEngine, InitError> create(
        const std::string& modelPath,
        const std::string& inputName,
        const std::string& outputName
    );

    ~OnnxEngine() = default;

    // ? Default move is correct here, unlike TrtEngine: every member is a
    // ? unique_ptr or a vector. There is no raw handle to null out.
    OnnxEngine(OnnxEngine&&) noexcept = default;
    OnnxEngine& operator=(OnnxEngine&&) noexcept = delete;                              // engines are initialized into their final owners
    OnnxEngine(const OnnxEngine&) = delete;
    OnnxEngine& operator=(const OnnxEngine&) = delete;

    // * Inference
    // ! `inputBuffer()` returns a view into a member. Valid until the next
    // ! call to infer(). Do not store it.
    // ! `infer()` returns a view into a member. Valid until the next call.
    // ! Synchronous: it blocks until the session finishes.
    [[nodiscard]] inline std::span<float> inputBuffer() noexcept { return m.hostInput; }
    [[nodiscard]] std::span<const float> infer();

    // * Accessors
    [[nodiscard]] inline const std::vector<std::size_t>& outputShape() const noexcept { return m.outputShape; }
    [[nodiscard]] inline std::size_t inputElementCount() const noexcept { return m.inputBytes / sizeof(float); }
};
