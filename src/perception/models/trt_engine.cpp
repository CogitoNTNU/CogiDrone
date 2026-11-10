#include "perception/models/trt_engine.h"

#include <fstream>

#include "logging.h"


namespace {
// * Logger
// ? TensorRT requires an ILogger. Routing it through cogidrone::log keeps the
// ? output in one place instead of a second stream nobody reads.
class TrtLogger final : public nvinfer1::ILogger {
public:
    void log(Severity severity, const char* message) noexcept override {
        // ? TensorRT is chatty at INFO. Only surface warnings and above.
        if (severity > Severity::kWARNING) return;
        cogidrone::log(LOGGING_LEVEL::WARNING, message);
    }
};

[[nodiscard]] std::vector<char> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};

    const std::streamsize SIZE = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(static_cast<std::size_t>(SIZE));
    if (!file.read(buffer.data(), SIZE)) return {};

    return buffer;
}

// ? A missing tensor name is the single most common TensorRT integration bug.
// ? Listing what the engine actually has turns a silent zero-output into a
// ? one-line fix.
void logTensorNames(const nvinfer1::ICudaEngine& engine) {
    const int COUNT = engine.getNbIOTensors();
    for (int i = 0; i < COUNT; ++i) {
        const char* NAME = engine.getIOTensorName(i);
        const bool IS_INPUT = engine.getTensorIOMode(NAME) == nvinfer1::TensorIOMode::kINPUT;
        cogidrone::log(LOGGING_LEVEL::INFO, IS_INPUT ? "  input tensor found" : "  output tensor found");
        cogidrone::log(LOGGING_LEVEL::INFO, NAME);
    }
}
} // namespace


// * Ctors & dtor
TrtEngine::~TrtEngine() {
    // ? Destroy the stream before the buffers it was used with. The unique_ptr
    // ? members are destroyed after this body runs, so the order is correct.
    if (m.stream) cudaStreamDestroy(m.stream);
}


TrtEngine::TrtEngine(TrtEngine&& other) noexcept : m(std::move(other.m)) {
    other.m.stream = nullptr;
}

TrtEngine& TrtEngine::operator=(TrtEngine&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    if (m.stream) {
        cudaStreamDestroy(m.stream);
    }

    m = std::move(other.m);
    other.m.stream = nullptr;

    return *this;
}

std::expected<TrtEngine, TrtEngine::InitError> TrtEngine::create(
    const std::string& enginePath,
    const std::string& inputName,
    const std::string& outputName
) {
    // ! Fallible work FIRST: read the plan
    const std::vector<char> PLAN = readFile(enginePath);
    if (PLAN.empty()) {
        cogidrone::log(LOGGING_LEVEL::ERROR, "TensorRT engine file not found or empty");
        return std::unexpected(InitError::EngineLoadFailed);
    }

    static TrtLogger logger;

    auto runtime = std::unique_ptr<nvinfer1::IRuntime, TrtDeleter>(
        nvinfer1::createInferRuntime(logger)
    );
    if (!runtime) return std::unexpected(InitError::EngineLoadFailed);

    auto engine = std::unique_ptr<nvinfer1::ICudaEngine, TrtDeleter>(
        runtime->deserializeCudaEngine(PLAN.data(), PLAN.size())
    );
    if (!engine) return std::unexpected(InitError::EngineLoadFailed);

    auto context = std::unique_ptr<nvinfer1::IExecutionContext, TrtDeleter>(
        engine->createExecutionContext()
    );
    if (!context) return std::unexpected(InitError::ContextCreateFailed);

    // * Tensor names
    // ? TRT 10: getTensorShape replaces getBindingDimensions. A name that does
    // ? not exist returns a Dims with nbDims == -1.
    const nvinfer1::Dims INPUT_DIMS  = engine->getTensorShape(inputName.c_str());
    const nvinfer1::Dims OUTPUT_DIMS = engine->getTensorShape(outputName.c_str());

    if (INPUT_DIMS.nbDims < 0 || OUTPUT_DIMS.nbDims < 0) {
        cogidrone::log(LOGGING_LEVEL::ERROR, "TensorRT tensor name not found in engine");
        logTensorNames(*engine);
        return std::unexpected(InitError::TensorNameNotFound);
    }

    std::size_t inputElements = 1;
    for (int i = 0; i < INPUT_DIMS.nbDims; ++i) {
        inputElements *= static_cast<std::size_t>(INPUT_DIMS.d[i]);
    }

    std::vector<std::size_t> outputShape;
    std::size_t outputElements = 1;
    for (int i = 0; i < OUTPUT_DIMS.nbDims; ++i) {
        outputShape.push_back(static_cast<std::size_t>(OUTPUT_DIMS.d[i]));
        outputElements *= static_cast<std::size_t>(OUTPUT_DIMS.d[i]);
    }

    TrtEngine trt(M{
        .runtime = std::move(runtime),
        .engine = std::move(engine),
        .context = std::move(context),
        .stream = nullptr,                                                              // ? will be wired in wire()
        .deviceInput = nullptr,                                                         // ? will be wired in wire()
        .deviceOutput = nullptr,                                                        // ? will be wired in wire()
        .hostInput = {},                                                                // ? sized in wire(), not here
        .hostOutput = {},                                                               // ? sized in wire(), not here                
        .inputName = inputName,
        .outputName = outputName,
        .inputBytes = inputElements * sizeof(float),
        .outputBytes = outputElements * sizeof(float),
        .outputShape = std::move(outputShape)
    });

    if (auto wired = trt.wire(); !wired) {
        return std::unexpected(wired.error());
    }

    return trt;
}

std::expected<void, TrtEngine::InitError> TrtEngine::wire() {
    // * Stream
    if (cudaStreamCreate(&m.stream) != cudaSuccess) {
        cogidrone::log(LOGGING_LEVEL::ERROR, "CUDA stream creation failed");
        return std::unexpected(InitError::StreamCreateFailed);
    }

    // * Device buffers
    void* input = nullptr;
    if (cudaMalloc(&input, m.inputBytes) != cudaSuccess) {
        cogidrone::log(LOGGING_LEVEL::ERROR, "CUDA input buffer allocation failed");
        return std::unexpected(InitError::BufferAllocationFailed);
    }
    m.deviceInput.reset(input);

    void* output = nullptr;
    if (cudaMalloc(&output, m.outputBytes) != cudaSuccess) {
        cogidrone::log(LOGGING_LEVEL::ERROR, "CUDA output buffer allocation failed");
        return std::unexpected(InitError::BufferAllocationFailed);
    }
    m.deviceOutput.reset(output);

    // * Host staging buffers
    // ? Sized from the engine's tensor shapes, allocated once. `assign` rather
    // ? than `resize` so the memory is zero-initialised - a partially-filled
    // ? input buffer then produces zeros, not garbage.
    m.hostInput.assign(m.inputBytes / sizeof(float), 0.0f);
    m.hostOutput.assign(m.outputBytes / sizeof(float), 0.0f);

    return {};
}


// * Inference
// ? inputBuffer() is defined inline in the header - it is a member access.

std::span<const float> TrtEngine::infer() {
    // * 1. Host -> device
    // ! On the same stream as the enqueue below, so the ordering is guaranteed
    // ! without an explicit synchronize between them.
    cudaMemcpyAsync(
        m.deviceInput.get(),
        m.hostInput.data(),
        m.inputBytes,
        cudaMemcpyHostToDevice,
        m.stream
    );

    // * 2. Run
    // ? TRT 10: named tensor addresses + enqueueV3.
    // ? TRT 8:  context->setBindingDimensions(0, dims) then
    // ?         context->enqueueV2(bindings, stream, nullptr), where bindings
    // ?         is a void*[2] of {deviceInput, deviceOutput}.
    m.context->setTensorAddress(m.inputName.c_str(), m.deviceInput.get());
    m.context->setTensorAddress(m.outputName.c_str(), m.deviceOutput.get());

    if (!m.context->enqueueV3(m.stream)) {
        cogidrone::log(LOGGING_LEVEL::ERROR, "TensorRT enqueue failed");
        return {};
    }

    // * 3. Device -> host
    cudaMemcpyAsync(
        m.hostOutput.data(),
        m.deviceOutput.get(),
        m.outputBytes,
        cudaMemcpyDeviceToHost,
        m.stream
    );

    // ? The only synchronization point. Everything above is queued.
    cudaStreamSynchronize(m.stream);

    return m.hostOutput;
}