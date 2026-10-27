#include "motion_receiver.h"

#include "MotionProtocol.h"
#include "log.h"
#include "motion_codec.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <array>

namespace ccv {

MotionReceiver::MotionReceiver(LatestSample& sample) : sample_(sample) {
}

MotionReceiver::~MotionReceiver() {
    Stop();
}

bool MotionReceiver::Start(std::string& error) {
    if (running_) {
        return true;
    }
    if (worker_.joinable() || socketHandle_ != INVALID_SOCKET || wsaStarted_) {
        Stop();
    }

    stop_ = false;
    error.clear();
    sample_.Clear();
    hasSequence_ = false;
    WSADATA wsaData = {};
    int wsaError = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaError != 0) {
        error = "WSAStartup failed: " + std::to_string(wsaError);
        return false;
    }
    wsaStarted_ = true;

    SOCKET socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socketHandle == INVALID_SOCKET) {
        error = "UDP socket failed: " + std::to_string(WSAGetLastError());
        WSACleanup();
        wsaStarted_ = false;
        return false;
    }
    socketHandle_ = static_cast<std::uintptr_t>(socketHandle);

    DWORD timeoutMs = 200;
    setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(motion::kPort);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(socketHandle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
        error = "UDP bind failed: " + std::to_string(WSAGetLastError());
        closesocket(socketHandle);
        socketHandle_ = INVALID_SOCKET;
        WSACleanup();
        wsaStarted_ = false;
        return false;
    }

    try {
        running_ = true;
        worker_ = std::thread(&MotionReceiver::Run, this);
    } catch (const std::exception& exception) {
        running_ = false;
        error = exception.what();
        closesocket(socketHandle);
        socketHandle_ = INVALID_SOCKET;
        WSACleanup();
        wsaStarted_ = false;
        return false;
    }

    Log("UDP receiver listening on 127.0.0.1:" + std::to_string(motion::kPort));
    return true;
}

void MotionReceiver::Stop() {
    stop_ = true;
    if (worker_.joinable()) {
        worker_.join();
    }
    running_ = false;
    if (socketHandle_ != INVALID_SOCKET) {
        closesocket(static_cast<SOCKET>(socketHandle_));
        socketHandle_ = INVALID_SOCKET;
    }
    if (wsaStarted_) {
        WSACleanup();
        wsaStarted_ = false;
    }
    sample_.Clear();
}

bool MotionReceiver::IsRunning() const {
    return running_;
}

void MotionReceiver::Run() {
    std::array<std::uint8_t, 256> buffer = {};
    while (!stop_) {
        sockaddr_in source = {};
        int sourceLength = sizeof(source);
        int received = recvfrom(static_cast<SOCKET>(socketHandle_),
                                reinterpret_cast<char*>(buffer.data()),
                                static_cast<int>(buffer.size()), 0,
                                reinterpret_cast<sockaddr*>(&source), &sourceLength);
        if (received == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAETIMEDOUT || error == WSAEWOULDBLOCK) {
                continue;
            }
            if (!stop_) {
                Log("UDP receive failed: " + std::to_string(error));
                running_ = false;
            }
            break;
        }
        if (source.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
            continue;
        }

        MotionFrame frame;
        if (!DecodeMotionFrame(buffer.data(), static_cast<std::size_t>(received), frame)) {
            continue;
        }
        if (hasSequence_ && frame.sessionId == sessionId_ &&
            !IsSequenceNewer(frame.sequence, lastSequence_)) {
            continue;
        }
        if (!hasSequence_ || frame.sessionId != sessionId_) {
            sessionId_ = frame.sessionId;
            hasSequence_ = true;
        }
        lastSequence_ = frame.sequence;
        sample_.Store(frame, GetTickCount64());
    }
}

}
