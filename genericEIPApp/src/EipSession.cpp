#include "EipSession.h"

#include <utility>
#include <chrono>
#include <cstdio>
#include <stdexcept>

#include "SessionInfo.h"
#include "sockets/EndPoint.h"
#include "ConnectionManager.h"
#include "cip/connectionManager/NetworkConnectionParams.h"

namespace genericEIP {

class ImplicitConnection {
public:
    std::shared_ptr<eipScanner::ConnectionManager> manager;
    eipScanner::IOConnection::SPtr connection;
    std::atomic<bool> running{false};
    std::atomic<bool> closeNotified{false};
    std::thread worker;
};

static std::vector<uint8_t> assemblyPath(uint32_t config, uint32_t output, uint32_t input)
{
    std::vector<uint8_t> path{0x20, 0x04};
    if (config) { path.push_back(0x24); path.push_back(static_cast<uint8_t>(config)); }
    path.push_back(0x2c); path.push_back(static_cast<uint8_t>(output));
    path.push_back(0x2c); path.push_back(static_cast<uint8_t>(input));
    return path;
}


EipSession::EipSession(const std::string& host)
{
    // Match EIPScanner's default SessionInfo(host, port) behavior.
    const auto timeout = std::chrono::milliseconds(1000);
    session_ = std::make_shared<eipScanner::SessionInfo>(
        host, EIP_DEFAULT_EXPLICIT_PORT, timeout);
}

EipSession::~EipSession()
{
    stopImplicit();
    disconnect();
}

bool EipSession::startImplicit(std::uint32_t config, std::uint32_t output,
    std::uint32_t input, std::size_t outputSize, std::size_t inputSize,
    std::uint32_t outputRpi, std::uint32_t inputRpi, unsigned timeoutMultiplier,
    bool outputRealtime, bool inputRealtime, unsigned trigger,
    ReceiveHandler receive, SendHandler send, CloseHandler close)
{
    if (!session_ || implicit_) return false;
    using namespace eipScanner::cip::connectionManager;
    using eipScanner::cip::connectionManager::NetworkConnectionParams;
    auto router = std::make_shared<eipScanner::MessageRouter>(
        eipScanner::MessageRouter::USE_8_BIT_PATH_SEGMENTS);
    auto manager = std::make_shared<eipScanner::ConnectionManager>(router);
    ConnectionParameters p;
    p.connectionPath = assemblyPath(config, output, input);
    p.o2tNetworkConnectionParams = NetworkConnectionParams::P2P |
        NetworkConnectionParams::SCHEDULED_PRIORITY | static_cast<uint32_t>(outputSize);
    p.t2oNetworkConnectionParams = NetworkConnectionParams::P2P |
        NetworkConnectionParams::SCHEDULED_PRIORITY | static_cast<uint32_t>(inputSize);
    p.o2tRPI = outputRpi;
    p.t2oRPI = inputRpi;
    // The AZD-KREP adapter needs the more tolerant Class 1 watchdog used by
    // the working reference driver (4 << 3 times the negotiated API).
    p.connectionTimeoutMultiplier = static_cast<std::uint8_t>(timeoutMultiplier);
    p.o2tRealTimeFormat = outputRealtime;
    p.t2oRealTimeFormat = inputRealtime;
    p.transportTypeTrigger = trigger ? trigger : NetworkConnectionParams::CLASS1;
    auto weak = manager->forwardOpen(session_, p);
    auto connection = weak.lock();
    if (!connection) return false;

    auto state = std::make_unique<ImplicitConnection>();
    state->manager = manager;
    // Keep a strong reference for the lifetime of the worker.  The reference
    // driver keeps its IO connection alive while polling; relying only on a
    // weak pointer makes receive handling fragile during startup/reconnect.
    state->connection = connection;
    connection->setDataToSend(send());
    connection->setReceiveDataListener([receive](auto, auto, const auto& data) {
        receive(data);
    });
    connection->setCloseListener([statePtr = state.get(), close]() {
        if (!statePtr->closeNotified.exchange(true)) close();
        statePtr->running = false;
    });
    state->running = true;
    state->worker = std::thread([statePtr = state.get(), send, close]() {
        try {
            while (statePtr->running) {
                statePtr->connection->setDataToSend(send());
                statePtr->manager->handleConnections(std::chrono::milliseconds(10));
            }
        } catch (const std::exception& e) {
            std::fprintf(stderr, "genericEIP implicit worker: %s\n", e.what());
            if (!statePtr->closeNotified.exchange(true)) close();
            statePtr->running = false;
        }
    });
    implicit_ = std::move(state);
    return true;
}

void EipSession::stopImplicit()
{
    if (!implicit_) return;
    implicit_->running = false;
    if (implicit_->worker.joinable()) implicit_->worker.join();
    implicit_.reset();
}

void EipSession::disconnect()
{
    session_.reset();
}

}  // namespace genericEIP
