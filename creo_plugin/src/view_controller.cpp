#include "view_controller.h"

#include "MotionProtocol.h"
#include "view_math.h"

#include <algorithm>
#include <cmath>
#include <cwchar>

namespace ccv {
namespace {

constexpr std::uint64_t kStaleMs = 300;
constexpr double kPanSpeed = 0.5;
constexpr double kZoomSpeed = 1.0;

double Clamp(double value, double minimum, double maximum) {
    return std::max(minimum, std::min(maximum, value));
}

}

void ViewController::Arm() {
    ClearLatch();
    armed_ = true;
}

void ViewController::Disarm() {
    armed_ = false;
    ClearLatch();
}

bool ViewController::IsArmed() const {
    return armed_;
}

bool ViewController::ReadContext(Context& context, std::wstring& status) {
    ProMdl windowModel = nullptr;
    if (ProWindowCurrentGet(&context.window) != PRO_TK_NO_ERROR || context.window < 0 ||
        ProMdlCurrentGet(&context.model) != PRO_TK_NO_ERROR || context.model == nullptr ||
        ProWindowMdlGet(context.window, &windowModel) != PRO_TK_NO_ERROR ||
        windowModel != context.model ||
        ProMdlTypeGet(context.model, &context.type) != PRO_TK_NO_ERROR ||
        (context.type != PRO_MDL_PART && context.type != PRO_MDL_ASSEMBLY) ||
        ProMdlIdGet(context.model, &context.modelId) != PRO_TK_NO_ERROR ||
        ProMdlDisplaynameGet(context.model, PRO_B_TRUE, context.name) != PRO_TK_NO_ERROR ||
        ProViewMatrixGet(context.model, nullptr, context.orientation) != PRO_TK_NO_ERROR ||
        ProWindowCurrentMatrixGet(context.panZoom) != PRO_TK_NO_ERROR) {
        status = L"Open a 3D part or assembly in the current window";
        return false;
    }
    return true;
}

bool ViewController::SameContext(const Context& context) const {
    return context.window == window_ && context.model == model_ &&
           context.type == type_ && context.modelId == modelId_ &&
           wcscmp(context.name, name_) == 0;
}

void ViewController::Latch(const Context& context, const MotionFrame& frame,
                           std::uint64_t nowMs) {
    window_ = context.window;
    model_ = context.model;
    type_ = context.type;
    modelId_ = context.modelId;
    wcscpy_s(name_, context.name);
    sessionId_ = frame.sessionId;
    grabId_ = frame.grabId;
    std::copy(&context.orientation[0][0], &context.orientation[0][0] + 16,
              &orientationAnchor_[0][0]);
    std::copy(&context.panZoom[0][0], &context.panZoom[0][0] + 16,
              &panZoomAnchor_[0][0]);
    panX_ = 0.0;
    panY_ = 0.0;
    zoomScale_ = 1.0;
    lastMotionMs_ = nowMs;
    latched_ = true;
    active_ = false;
}

void ViewController::ClearLatch() {
    latched_ = false;
    active_ = false;
    window_ = -1;
    model_ = nullptr;
    modelId_ = -1;
    sessionId_ = 0;
    grabId_ = 0;
    lastMotionMs_ = 0;
}

ViewTick ViewController::Fail(const std::wstring& status, bool disarm) {
    if (disarm) {
        armed_ = false;
    }
    ClearLatch();
    return {true, false, false, status};
}

ViewTick ViewController::Tick(bool hasFrame, const MotionFrame& frame,
                              std::uint64_t localMs, std::uint64_t nowMs) {
    Context context;
    ViewTick tick;
    if (!ReadContext(context, tick.status)) {
        bool wasLatched = latched_;
        ClearLatch();
        if (wasLatched) {
            armed_ = false;
        }
        tick.writeOk = !wasLatched;
        return tick;
    }
    tick.contextOk = true;

    if (latched_ && !SameContext(context)) {
        return Fail(L"Model or window changed; enable again", true);
    }
    if (!armed_) {
        ClearLatch();
        tick.status = L"Disabled";
        return tick;
    }

    bool stale = !hasFrame || localMs + kStaleMs < nowMs ||
                 frame.hostMs + kStaleMs < nowMs || frame.hostMs > nowMs + 1000;
    if (stale) {
        if (latched_) {
            return Fail(L"Motion data timed out; enable again", true);
        }
        tick.status = L"Waiting for motion data";
        return tick;
    }
    if ((frame.flags & motion::kFlagEnable) == 0) {
        ClearLatch();
        tick.status = L"Bridge output is off";
        return tick;
    }
    if ((frame.flags & motion::kFlagGrab) == 0) {
        ClearLatch();
        tick.status = L"Armed; hold Space to grab";
        return tick;
    }

    if (!latched_ || sessionId_ != frame.sessionId || grabId_ != frame.grabId) {
        Latch(context, frame, nowMs);
    }

    double deltaSeconds = Clamp(static_cast<double>(nowMs - lastMotionMs_) / 1000.0,
                                0.0, 0.5);
    lastMotionMs_ = nowMs;
    bool translating = (frame.flags & motion::kFlagTranslate) != 0;
    bool orientationDirty = !translating;
    bool panZoomDirty = false;

    ProMatrix orientation = {};
    if (orientationDirty) {
        viewmath::BuildOrientation(orientationAnchor_, frame.quaternion, orientation);
        ProError error = ProViewMatrixSet(context.model, nullptr, orientation);
        if (error != PRO_TK_NO_ERROR) {
            return Fail(L"ProViewMatrixSet failed: " + std::to_wstring(error), true);
        }
    }

    if (translating) {
        panX_ += Clamp(frame.pan[0], -1.0F, 1.0F) * kPanSpeed * deltaSeconds;
        panY_ += Clamp(frame.pan[1], -1.0F, 1.0F) * kPanSpeed * deltaSeconds;
        panZoomDirty = std::abs(frame.pan[0]) > 0.0001F ||
                       std::abs(frame.pan[1]) > 0.0001F;
    }
    if (std::abs(frame.zoom) > 0.0001F) {
        zoomScale_ *= std::exp(Clamp(frame.zoom, -1.0F, 1.0F) * kZoomSpeed * deltaSeconds);
        zoomScale_ = Clamp(zoomScale_, 0.05, 20.0);
        panZoomDirty = true;
    }

    if (panZoomDirty) {
        ProMatrix panZoom = {};
        viewmath::BuildPanZoom(panZoomAnchor_, panX_, panY_, zoomScale_, panZoom);
        ProError error = ProWindowPanZoomMatrixSet(context.window, panZoom);
        if (error != PRO_TK_NO_ERROR) {
            return Fail(L"ProWindowPanZoomMatrixSet failed: " + std::to_wstring(error), true);
        }
    }

    if ((orientationDirty || panZoomDirty) &&
        ProWindowRepaint(context.window) != PRO_TK_NO_ERROR) {
        return Fail(L"ProWindowRepaint failed", true);
    }

    active_ = true;
    tick.active = true;
    tick.status = translating ? L"Active: translate" : L"Active: rotate";
    return tick;
}

}
