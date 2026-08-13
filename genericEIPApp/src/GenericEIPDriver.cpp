#include "GenericEIPDriver.h"

#include <algorithm>
#include <map>
#include <memory>
#include <cmath>
#include <cstring>
#include <utility>

#include <epicsExport.h>
#include <iocsh.h>

#include "EipSession.h"

static std::map<std::string, genericEIP::GenericEIPDriver*> drivers;

namespace genericEIP {

bool validateConnectionConfig(const ConnectionConfig& c, std::string& error)
{
    if (c.portName.empty()) { error = "port name is empty"; return false; }
    if (c.ipAddress.empty()) { error = "IP address is empty"; return false; }
    if (c.pollPeriod <= 0.0) { error = "poll period must be positive"; return false; }
    if (c.outputSize == 0 || c.inputSize == 0) {
        error = "input and output assembly sizes must be positive"; return false;
    }
    if (c.outputAssembly == 0 || c.inputAssembly == 0) {
        error = "input and output assembly IDs must be non-zero"; return false;
    }
    if (c.outputRpi == 0 || c.inputRpi == 0) {
        error = "input and output RPI values must be non-zero"; return false;
    }
    if (c.timeoutMultiplier == 0) {
        error = "connection timeout multiplier must be non-zero"; return false;
    }
    return true;
}

GenericEIPDriver::GenericEIPDriver(const ConnectionConfig& c)
    : asynPortDriver(c.portName.c_str(), 1,
                     asynOctetMask | asynDrvUserMask,
                     asynOctetMask, ASYN_CANBLOCK, 1, 0, 0),
      config_(c), inputBuffer_(c.inputSize, 0), outputBuffer_(c.outputSize, 0)
{
    scanIoInit(&ioScanPvt_);
    createParam("RAW_INPUT", asynParamOctet, &rawInputParam_);
    createParam("RAW_OUTPUT", asynParamOctet, &rawOutputParam_);
    createParam("CONNECTION_STATE", asynParamOctet, &connectionStateParam_);
    setStringParam(rawInputParam_, "");
    setStringParam(rawOutputParam_, "");
    setConnectionState("DISCONNECTED");
}

static std::uint64_t loadUnsigned(const std::vector<std::uint8_t>& b, size_t o, size_t n)
{
    std::uint64_t v = 0;
    for (size_t i = 0; i < n; ++i) v |= std::uint64_t(b[o + i]) << (8 * i);
    return v;
}

static std::int64_t signExtend(std::uint64_t v, size_t n)
{
    if (n < 8 && (v & (std::uint64_t(1) << (n * 8 - 1))))
        v |= ~((std::uint64_t(1) << (n * 8)) - 1);
    return static_cast<std::int64_t>(v);
}

bool GenericEIPDriver::readField(size_t offset, size_t length, const std::string& type,
                                 int bit, double& value) const
{
    std::lock_guard<std::mutex> lock(bufferMutex_);
    if (offset + length > inputBuffer_.size() || length == 0 || length > 8) return false;
    const auto u = loadUnsigned(inputBuffer_, offset, length);
    if (type == "BIT") { if (bit < 0 || bit > 15) return false; value = (u >> bit) & 1; }
    else if (type == "FLOAT32") { std::uint32_t bits = static_cast<std::uint32_t>(u); float f; std::memcpy(&f, &bits, sizeof f); value = f; }
    else if (type == "FLOAT64") { std::uint64_t bits = u; double d; std::memcpy(&d, &bits, sizeof d); value = d; }
    else if (type == "INT8" || type == "INT16" || type == "INT32" || type == "INT64") value = static_cast<double>(signExtend(u, length));
    else value = static_cast<double>(u);
    return true;
}

bool GenericEIPDriver::writeField(size_t offset, size_t length, const std::string& type,
                                  int bit, double value)
{
    std::lock_guard<std::mutex> lock(bufferMutex_);
    if (offset + length > outputBuffer_.size() || length == 0 || length > 8) return false;
    if (type == "BIT") {
        if (bit < 0 || bit > 15 || length != (bit > 7 ? 2u : 1u)) return false;
        const std::uint16_t mask = static_cast<std::uint16_t>(1u << bit);
        std::uint16_t word = static_cast<std::uint16_t>(outputBuffer_[offset]) |
                             (static_cast<std::uint16_t>(outputBuffer_[offset + 1]) << 8);
        if (value != 0.0) word |= mask;
        else word &= static_cast<std::uint16_t>(~mask);
        outputBuffer_[offset] = static_cast<std::uint8_t>(word);
        outputBuffer_[offset + 1] = static_cast<std::uint8_t>(word >> 8);
    } else {
        std::uint64_t u = 0;
        if (type == "FLOAT32") { float f = static_cast<float>(value); std::memcpy(&u, &f, sizeof f); }
        else if (type == "FLOAT64") { double d = value; std::memcpy(&u, &d, sizeof d); }
        else u = static_cast<std::uint64_t>(std::llround(value));
        for (size_t i = 0; i < length; ++i) outputBuffer_[offset + i] = std::uint8_t(u >> (8 * i));
    }
    return true;
}

GenericEIPDriver::~GenericEIPDriver()
{
    disconnectSession();
}

asynStatus GenericEIPDriver::connectSession()
{
    try {
        std::lock_guard<std::mutex> sessionLock(sessionMutex_);
        auto newSession = std::make_unique<EipSession>(config_.ipAddress);
        const bool opened = newSession->startImplicit(
            config_.configAssembly, config_.outputAssembly, config_.inputAssembly,
            config_.outputSize, config_.inputSize, config_.outputRpi, config_.inputRpi,
            config_.timeoutMultiplier, true, false, config_.trigger,
            [this](const std::vector<std::uint8_t>& data) {
                {
                    std::lock_guard<std::mutex> callbackLock(bufferMutex_);
                    const auto count = std::min(data.size(), inputBuffer_.size());
                    std::copy_n(data.begin(), count, inputBuffer_.begin());
                }
                scanIoRequest(ioScanPvt_);
            },
            [this]() {
                std::lock_guard<std::mutex> outputLock(bufferMutex_);
                return outputBuffer_;
            },
            [this]() {
                linkLost_ = true;
                setConnectionState("DISCONNECTED");
            });
        if (!opened) throw std::runtime_error("Class 1 Forward Open failed");
        session_ = std::move(newSession);
        linkLost_ = false;
        setConnectionState("CONNECTED");
        return asynSuccess;
    } catch (const std::exception& e) {
        printf("genericEipConnect(%s): %s\n", config_.portName.c_str(), e.what());
        setConnectionState(e.what());
        return asynError;
    }
}

void GenericEIPDriver::connectAsync()
{
    std::lock_guard<std::mutex> connectLock(connectMutex_);
    if (connecting_) return;
    if (connectThread_.joinable()) return;

    stopRequested_ = false;
    connecting_ = true;
    connectThread_ = std::thread([this]() {
        while (!stopRequested_) {
            setConnectionState("CONNECTING");
            if (connectSession() == asynSuccess) {
                while (!stopRequested_ && !linkLost_)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::lock_guard<std::mutex> sessionLock(sessionMutex_);
                session_.reset();
            } else {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            if (!stopRequested_) setConnectionState("DISCONNECTED");
        }
        connecting_ = false;
    });
}

void GenericEIPDriver::disconnectSession()
{
    stopRequested_ = true;
    linkLost_ = true;
    std::lock_guard<std::mutex> connectLock(connectMutex_);
    if (connectThread_.joinable()) connectThread_.join();
    std::lock_guard<std::mutex> sessionLock(sessionMutex_);
    session_.reset();
    setConnectionState("DISCONNECTED");
}

void GenericEIPDriver::setConnectionState(const char* state)
{
    setStringParam(connectionStateParam_, state ? state : "UNKNOWN");
    callParamCallbacks();
    scanIoRequest(ioScanPvt_);
}

bool GenericEIPDriver::sessionConnected() const
{
    std::lock_guard<std::mutex> sessionLock(sessionMutex_);
    return session_ && session_->isConnected();
}

bool GenericEIPDriver::readConnectionState(char* value, std::size_t maxChars)
{
    return value && maxChars > 0 &&
        getStringParam(connectionStateParam_, static_cast<int>(maxChars), value) == asynSuccess;
}

asynStatus GenericEIPDriver::readOctet(asynUser* pasynUser, char* value,
                                       size_t maxChars, size_t* nActual,
                                       int* eomReason)
{
    if (!value || !nActual || maxChars == 0) return asynError;
    std::lock_guard<std::mutex> lock(bufferMutex_);
    const std::vector<std::uint8_t>* buffer = nullptr;
    if (pasynUser->reason == rawInputParam_) buffer = &inputBuffer_;
    else if (pasynUser->reason == rawOutputParam_) buffer = &outputBuffer_;
    else return asynError;
    const size_t count = std::min(maxChars, buffer->size());
    std::memcpy(value, buffer->data(), count);
    *nActual = count;
    if (eomReason) *eomReason = ASYN_EOM_CNT;
    return asynSuccess;
}

asynStatus GenericEIPDriver::writeOctet(asynUser* pasynUser, const char* value,
                                        size_t maxChars, size_t* nActual)
{
    if (!value || !nActual) return asynError;
    if (pasynUser->reason != rawOutputParam_) return asynError;
    std::lock_guard<std::mutex> lock(bufferMutex_);
    if (maxChars != outputBuffer_.size()) return asynError;
    std::memcpy(outputBuffer_.data(), value, outputBuffer_.size());
    *nActual = outputBuffer_.size();
    return asynSuccess;
}

}  // namespace genericEIP

extern "C" int genericEipConfigure(const char* portName, const char* ipAddress,
    double pollPeriod, int vendorId, int productCode, int revision,
    int configAssembly, int outputAssembly, int inputAssembly,
    int outputSize, int inputSize, int outputRpi, int inputRpi)
{
    genericEIP::ConnectionConfig c;
    c.portName = portName ? portName : "";
    c.ipAddress = ipAddress ? ipAddress : "";
    c.pollPeriod = pollPeriod;
    c.vendorId = static_cast<std::uint16_t>(vendorId);
    c.productCode = static_cast<std::uint16_t>(productCode);
    c.revision = static_cast<std::uint8_t>(revision);
    c.configAssembly = static_cast<std::uint32_t>(configAssembly);
    c.outputAssembly = static_cast<std::uint32_t>(outputAssembly);
    c.inputAssembly = static_cast<std::uint32_t>(inputAssembly);
    c.outputSize = outputSize > 0 ? static_cast<size_t>(outputSize) : 0;
    c.inputSize = inputSize > 0 ? static_cast<size_t>(inputSize) : 0;
    c.outputRpi = outputRpi > 0 ? static_cast<std::uint32_t>(outputRpi) : 0;
    c.inputRpi = inputRpi > 0 ? static_cast<std::uint32_t>(inputRpi) : 0;

    std::string error;
    if (!genericEIP::validateConnectionConfig(c, error)) {
        printf("genericEipConfigure: %s\n", error.c_str());
        return -1;
    }
    auto* driver = new genericEIP::GenericEIPDriver(c);
    drivers[c.portName] = driver;
    return 0;
}

extern "C" int genericEipConnect(const char* portName)
{
    const auto it = drivers.find(portName ? portName : "");
    if (it == drivers.end() || !it->second) return -1;
    it->second->connectAsync();
    return 0;
}

extern "C" int genericEipDisconnect(const char* portName)
{
    const auto it = drivers.find(portName ? portName : "");
    if (it == drivers.end() || !it->second) return -1;
    it->second->disconnectSession();
    return 0;
}

static genericEIP::GenericEIPDriver* findDriver(const char* portName)
{
    const auto it = drivers.find(portName ? portName : "");
    return it == drivers.end() ? nullptr : it->second;
}

extern "C" bool genericEipReadField(const char* p, size_t o, size_t l, const char* t, int b, double* v)
{
    return v && findDriver(p) && t && findDriver(p)->readField(o, l, t, b, *v);
}

extern "C" bool genericEipWriteField(const char* p, size_t o, size_t l, const char* t, int b, double v)
{
    return findDriver(p) && t && findDriver(p)->writeField(o, l, t, b, v);
}

extern "C" IOSCANPVT genericEipGetScanPvt(const char* p)
{
    return findDriver(p) ? findDriver(p)->scanPvt() : nullptr;
}

extern "C" bool genericEipReadConnectionState(const char* p, char* value, size_t maxChars)
{
    return findDriver(p) && findDriver(p)->readConnectionState(value, maxChars);
}

static const iocshArg configureArgs[] = {
    {"portName", iocshArgString}, {"ipAddress", iocshArgString},
    {"pollPeriod", iocshArgDouble}, {"vendorId", iocshArgInt},
    {"productCode", iocshArgInt}, {"revision", iocshArgInt},
    {"configAssembly", iocshArgInt}, {"outputAssembly", iocshArgInt},
    {"inputAssembly", iocshArgInt}, {"outputSize", iocshArgInt},
    {"inputSize", iocshArgInt}, {"outputRpi", iocshArgInt},
    {"inputRpi", iocshArgInt}
};
static const iocshArg* configureArgPtrs[] = {
    &configureArgs[0], &configureArgs[1], &configureArgs[2], &configureArgs[3],
    &configureArgs[4], &configureArgs[5], &configureArgs[6], &configureArgs[7],
    &configureArgs[8], &configureArgs[9], &configureArgs[10], &configureArgs[11],
    &configureArgs[12]
};
static const iocshFuncDef configureDef = {"genericEipConfigure", 13, configureArgPtrs};

static const iocshArg connectArg = {"portName", iocshArgString};
static const iocshArg* connectArgs[] = {&connectArg};
static const iocshFuncDef connectDef = {"genericEipConnect", 1, connectArgs};
static const iocshFuncDef disconnectDef = {"genericEipDisconnect", 1, connectArgs};

static void configureCall(const iocshArgBuf* args)
{
    genericEipConfigure(args[0].sval, args[1].sval, args[2].dval,
        args[3].ival, args[4].ival, args[5].ival, args[6].ival,
        args[7].ival, args[8].ival, args[9].ival, args[10].ival,
        args[11].ival, args[12].ival);
}

static void connectCall(const iocshArgBuf* args)
{
    const int status = genericEipConnect(args[0].sval);
    printf("genericEipConnect(%s): %s\n", args[0].sval,
           status == 0 ? "started" : "failed");
}

static void disconnectCall(const iocshArgBuf* args)
{
    genericEipDisconnect(args[0].sval);
}

extern "C" void genericEIPRegister(void)
{
    iocshRegister(&configureDef, configureCall);
    iocshRegister(&connectDef, connectCall);
    iocshRegister(&disconnectDef, disconnectCall);
}

epicsExportRegistrar(genericEIPRegister);
