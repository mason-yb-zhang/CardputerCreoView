#pragma once

#include "latest_sample.h"

#include <ProMdl.h>
#include <ProView.h>
#include <ProWindows.h>

#include <cstdint>
#include <string>

namespace ccv {

struct ViewTick {
    bool contextOk = false;
    bool writeOk = true;
    bool active = false;
    std::wstring status;
};

class ViewController {
public:
    void Arm();
    void Disarm();
    bool IsArmed() const;
    ViewTick Tick(bool hasFrame, const MotionFrame& frame,
                  std::uint64_t localMs, std::uint64_t nowMs);

private:
    struct Context {
        int window = -1;
        ProMdl model = nullptr;
        ProMdlType type = PRO_MDL_UNUSED;
        int modelId = -1;
        ProMdlFileName name = {};
        ProMatrix orientation = {};
        ProMatrix panZoom = {};
    };

    bool ReadContext(Context& context, std::wstring& status);
    bool SameContext(const Context& context) const;
    void Latch(const Context& context, const MotionFrame& frame, std::uint64_t nowMs);
    void ClearLatch();
    ViewTick Fail(const std::wstring& status, bool disarm);

    bool armed_ = false;
    bool latched_ = false;
    bool active_ = false;
    int window_ = -1;
    ProMdl model_ = nullptr;
    ProMdlType type_ = PRO_MDL_UNUSED;
    int modelId_ = -1;
    ProMdlFileName name_ = {};
    std::uint32_t sessionId_ = 0;
    std::uint32_t grabId_ = 0;
    ProMatrix orientationAnchor_ = {};
    ProMatrix panZoomAnchor_ = {};
    double panX_ = 0.0;
    double panY_ = 0.0;
    double zoomScale_ = 1.0;
    std::uint64_t lastMotionMs_ = 0;
};

}
