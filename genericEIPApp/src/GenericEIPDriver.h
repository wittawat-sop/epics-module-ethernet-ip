#ifndef GENERIC_EIP_DRIVER_H
#define GENERIC_EIP_DRIVER_H

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <memory>
#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include <asynPortDriver.h>
#include <dbScan.h>

namespace genericEIP { class EipSession; }

namespace genericEIP {

struct ConnectionConfig {
    std::string portName;
    std::string ipAddress;
    double pollPeriod = 0.01;
    std::uint16_t vendorId = 0;
    std::uint16_t productCode = 0;
    std::uint8_t revision = 0;
    std::uint32_t configAssembly = 0;
    std::uint32_t outputAssembly = 0;
    std::uint32_t inputAssembly = 0;
    std::size_t outputSize = 0;
    std::size_t inputSize = 0;
    std::uint32_t outputRpi = 0;
    std::uint32_t inputRpi = 0;
    // Keep the explicit TCP session timeout separate from the Class 1
    // connection watchdog.  These match the working AZD-KREP driver.
    unsigned timeoutMultiplier = 3;
    unsigned transportType = 0;
    unsigned trigger = 0;
    unsigned realTimeFormat = 0;
    bool validateIdentity = false;
};

bool validateConnectionConfig(const ConnectionConfig& config,
                              std::string& errorMessage);

class GenericEIPDriver final : public asynPortDriver {
public:
    explicit GenericEIPDriver(const ConnectionConfig& config);
    ~GenericEIPDriver() override;

    asynStatus readOctet(asynUser* pasynUser, char* value, size_t maxChars,
                         size_t* nActual, int* eomReason) override;
    asynStatus writeOctet(asynUser* pasynUser, const char* value,
                          size_t maxChars, size_t* nActual) override;

    const ConnectionConfig& config() const { return config_; }
    asynStatus connectSession();
    void connectAsync();
    void disconnectSession();
    bool sessionConnected() const;
    bool readConnectionState(char* value, std::size_t maxChars);
    bool readField(std::size_t offset, std::size_t length, const std::string& type,
                   int bit, double& value) const;
    bool writeField(std::size_t offset, std::size_t length, const std::string& type,
                    int bit, double value);
    IOSCANPVT scanPvt() const { return ioScanPvt_; }

private:
    enum BufferReason { RawInput = 0, RawOutput = 1 };

    ConnectionConfig config_;
    std::vector<std::uint8_t> inputBuffer_;
    std::vector<std::uint8_t> outputBuffer_;
    mutable std::mutex bufferMutex_;
    mutable std::mutex sessionMutex_;
    std::mutex connectMutex_;
    std::thread connectThread_;
    std::atomic<bool> connecting_{false};
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> linkLost_{false};
    int rawInputParam_ = 0;
    int rawOutputParam_ = 0;
    int connectionStateParam_ = 0;
    std::unique_ptr<EipSession> session_;
    IOSCANPVT ioScanPvt_ = nullptr;
    void setConnectionState(const char* state);
};

}  // namespace genericEIP

extern "C" int genericEipConfigure(const char* portName, const char* ipAddress,
    double pollPeriod, int vendorId, int productCode, int revision,
    int configAssembly, int outputAssembly, int inputAssembly,
    int outputSize, int inputSize, int outputRpi, int inputRpi);
extern "C" int genericEipConnect(const char* portName);
extern "C" int genericEipDisconnect(const char* portName);
extern "C" bool genericEipReadField(const char* portName, std::size_t offset,
    std::size_t length, const char* type, int bit, double* value);
extern "C" bool genericEipWriteField(const char* portName, std::size_t offset,
    std::size_t length, const char* type, int bit, double value);
extern "C" IOSCANPVT genericEipGetScanPvt(const char* portName);
extern "C" bool genericEipReadConnectionState(const char* portName, char* value, std::size_t maxChars);

#endif
