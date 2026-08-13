#ifndef GENERIC_EIP_SESSION_H
#define GENERIC_EIP_SESSION_H

#include <chrono>
#include <memory>
#include <string>
#include <atomic>
#include <functional>
#include <thread>
#include <vector>

namespace eipScanner { class SessionInfo; }

namespace genericEIP {

class EipSession {
public:
    using ReceiveHandler = std::function<void(const std::vector<std::uint8_t>&)>;
    using SendHandler = std::function<std::vector<std::uint8_t>()>;
    using CloseHandler = std::function<void()>;
    explicit EipSession(const std::string& host);
    ~EipSession();

    EipSession(const EipSession&) = delete;
    EipSession& operator=(const EipSession&) = delete;

    bool isConnected() const { return static_cast<bool>(session_) && static_cast<bool>(implicit_); }
    bool startImplicit(std::uint32_t configAssembly, std::uint32_t outputAssembly,
                       std::uint32_t inputAssembly, std::size_t outputSize,
                       std::size_t inputSize, std::uint32_t outputRpi,
                       std::uint32_t inputRpi, unsigned timeoutMultiplier,
                       bool outputRealTimeFormat, bool inputRealTimeFormat,
                       unsigned transportTypeTrigger, ReceiveHandler receive,
                       SendHandler send, CloseHandler close);
    void stopImplicit();
    void disconnect();
    std::shared_ptr<eipScanner::SessionInfo> handle() const { return session_; }

private:
    std::shared_ptr<eipScanner::SessionInfo> session_;
    std::unique_ptr<class ImplicitConnection> implicit_;
};

}  // namespace genericEIP

#endif
