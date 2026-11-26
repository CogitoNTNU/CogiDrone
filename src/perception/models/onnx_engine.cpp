#include "perception/models/onnx_engine.h"

#include "logging.h"


namespace {
// ? One Env per process. ORT documents this, and two models (person + head)
// ? would otherwise create two. Function-local static: constructed on first
// ? use, destroyed at exit after every Session that references it.
[[nodiscard]] Ort::Env& sharedEnv() {
    static Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "cogidrone");
    return env;
}

// ? ONNX Runtime's C++ API throws Ort::Exception. The rest of this codebase
// ? uses std::expected, so every ORT call is wrapped in try/catch.
// ? Returns an empty vector on a dynamic dimension - ultralytics exports with
// ? `imgsz=640` produce static shapes, so a dynamic dim means the export was
// ? wrong, not that we need to handle it.
[[nodiscard]] std::vector<std::int64_t> staticShape(
    const Ort::TypeInfo& typeInfo,
    const char* errorMessage
) {
    const auto SHAPE = typeInfo.GetTensorTypeAndShapeInfo().GetShape();
    for (const std::int64_t DIM : SHAPE) {
        if (DIM < 0) {
            cogidrone::log(LOGGING_LEVEL::ERROR, errorMessage);
            return {};
        }
    }
    return SHAPE;
}
} // namespace


std::expected<OnnxEngine, OnnxEngine::InitError> OnnxEngine::create(
    const std::string& modelPath,
    const std::string& inputName,
    const std::string& outputName
) {
    try {
        Ort::SessionOptions options;
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        // ? Intra-op threads default to the core count, which is what we want
        // ? for a 640x640 conv net. Left unset deliberately.

        auto session = std::make_unique<Ort::Session>(
            sharedEnv(), modelPath.c_str(), options
        );

        // * Shapes
        const auto INPUT_SHAPE = staticShape(
            session->GetInputTypeInfo(0),
            "ONNX input has a dynamic dimension - re-export with a fixed imgsz"
        );
        const auto OUTPUT_SHAPE = staticShape(
            session->GetOutputTypeInfo(0),
            "ONNX output has a dynamic dimension - re-export with a fixed imgsz"
        );
        if (INPUT_SHAPE.empty() || OUTPUT_SHAPE.empty()) {
            return std::unexpected(InitError::DynamicShapeUnsupported);
        }

        std::size_t inputElements = 1;
        for (const std::int64_t D : INPUT_SHAPE) inputElements *= static_cast<std::size_t>(D);

        std::size_t outputElements = 1;
        std::vector<std::size_t> outputShape;
        outputShape.reserve(OUTPUT_SHAPE.size());
        for (const std::int64_t D : OUTPUT_SHAPE) {
            outputShape.push_back(static_cast<std::size_t>(D));
            outputElements *= static_cast<std::size_t>(D);
        }

        // * Assembly
        // ? The vectors are built HERE, then the tensors wrap them. Moving a
        // ? std::vector transfers its heap block, so the pointer the tensor
        // ? holds stays valid after the move into `m`.
        M members{
            .session = std::move(session),
            .binding = nullptr,                                                         // ? needs the tensors first
            .inputTensor = nullptr,
            .outputTensor = nullptr,
            .hostInput = std::vector<float>(inputElements, 0.0f),
            .hostOutput = std::vector<float>(outputElements, 0.0f),
            .inputName = inputName,
            .outputName = outputName,
            .inputBytes = inputElements * sizeof(float),
            .outputBytes = outputElements * sizeof(float),
            .outputShape = std::move(outputShape)
        };

        // * Tensors wrapping the buffers
        // ? `CreateTensor` does NOT copy - it stores the pointer. The vectors
        // ? must outlive the tensors, which they do: both live in `m`.
        const Ort::MemoryInfo MEMORY = Ort::MemoryInfo::CreateCpu(
            OrtArenaAllocator, OrtMemTypeDefault
        );

        members.inputTensor = std::make_unique<Ort::Value>(Ort::Value::CreateTensor<float>(
            MEMORY,
            members.hostInput.data(), members.hostInput.size(),
            INPUT_SHAPE.data(), INPUT_SHAPE.size()
        ));

        members.outputTensor = std::make_unique<Ort::Value>(Ort::Value::CreateTensor<float>(
            MEMORY,
            members.hostOutput.data(), members.hostOutput.size(),
            OUTPUT_SHAPE.data(), OUTPUT_SHAPE.size()
        ));

        // * Bind once
        // ? This is what makes infer() allocation-free. The plain
        // ? `Session::Run(inputs, outputs)` overload allocates a fresh output
        // ? tensor on every call.
        members.binding = std::make_unique<Ort::IoBinding>(*members.session);
        members.binding->BindInput(inputName.c_str(), *members.inputTensor);
        members.binding->BindOutput(outputName.c_str(), *members.outputTensor);

        return OnnxEngine(std::move(members));
    }
    catch (const Ort::Exception& e) {
        cogidrone::log(LOGGING_LEVEL::ERROR, e.what());
        return std::unexpected(InitError::EngineLoadFailed);
    }
}


std::span<const float> OnnxEngine::infer() {
    try {
        // ? No-ops for the CPU provider, but correct if a GPU provider is ever
        // ? added. Cheap enough to keep.
        m.binding->SynchronizeInputs();
        m.session->Run(Ort::RunOptions{nullptr}, *m.binding);
        m.binding->SynchronizeOutputs();
    }
    catch (const Ort::Exception& e) {
        cogidrone::log(LOGGING_LEVEL::ERROR, e.what());
        return {};
    }

    return m.hostOutput;
}
