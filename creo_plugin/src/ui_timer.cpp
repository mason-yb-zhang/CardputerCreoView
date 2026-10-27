#include "ui_timer.h"

#include "latest_sample.h"
#include "log.h"
#include "motion_receiver.h"
#include "view_controller.h"

#include <ProUIDialog.h>
#include <ProUILabel.h>
#include <ProUIPushbutton.h>
#include <ProUtil.h>

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <string>

namespace ccv {
namespace {

char g_dialog[] = "cardputer_creo";
char g_resource[] = "cardputer_creo";
char g_enableButton[] = "EnableButton";
char g_disableButton[] = "DisableButton";
char g_rate2Button[] = "Rate2Button";
char g_rate5Button[] = "Rate5Button";
char g_rate10Button[] = "Rate10Button";
char g_closeButton[] = "CloseButton";
char g_statusLabel[] = "StatusLabel";

unsigned long g_mainThreadId = 0;
std::atomic<bool> g_unsafeThread{false};
bool g_created = false;
bool g_visible = false;
bool g_activated = false;
bool g_timerCreated = false;
bool g_probeDone = false;
bool g_readOnlyOk = false;
int g_intervalMs = 500;
ProUITimerID g_timer = nullptr;
LatestSample g_sample;
MotionReceiver g_receiver(g_sample);
ViewController g_view;
std::uint64_t g_lastCallbackMs = 0;
std::uint64_t g_statsStartMs = 0;
std::uint64_t g_maxIntervalMs = 0;
int g_callbackCount = 0;
double g_actualHz = 0.0;

void SetStatus(std::wstring text) {
    if (!g_created || !g_visible) {
        return;
    }
    wchar_t suffix[96] = {};
    swprintf_s(suffix, L" | %d Hz target | %.1f Hz actual",
               1000 / g_intervalMs, g_actualHz);
    text += suffix;
    ProUILabelTextSet(g_dialog, g_statusLabel, text.data());
}

void TimerCallback(char* dialog, ProUITimerID timerId, ProAppData appdata);

void StartTimer() {
    if (!g_visible || g_unsafeThread) {
        return;
    }
    if (!g_timerCreated) {
        ProName timerName;
        ProStringToWstring(timerName, const_cast<char*>("CardputerCreoTimer"));
        ProError error = ProUITimerCreate(TimerCallback, nullptr, timerName, &g_timer);
        if (error != PRO_TK_NO_ERROR) {
            Log("ProUITimerCreate failed: " + std::to_string(error));
            SetStatus(L"Timer creation failed");
            return;
        }
        g_timerCreated = true;
    }
    ProError error = ProUIDialogTimerStart(g_dialog, g_timer, g_intervalMs, PRO_B_FALSE);
    if (error != PRO_TK_NO_ERROR) {
        Log("ProUIDialogTimerStart failed: " + std::to_string(error));
        g_view.Disarm();
        SetStatus(L"Timer start failed");
    }
}

void UpdateStats(std::uint64_t nowMs) {
    if (g_lastCallbackMs != 0) {
        g_maxIntervalMs = std::max(g_maxIntervalMs, nowMs - g_lastCallbackMs);
    }
    g_lastCallbackMs = nowMs;
    if (g_statsStartMs == 0) {
        g_statsStartMs = nowMs;
    }
    ++g_callbackCount;
    std::uint64_t elapsed = nowMs - g_statsStartMs;
    if (elapsed >= 2000) {
        g_actualHz = g_callbackCount * 1000.0 / static_cast<double>(elapsed);
        Log("Timer stats: actualHz=" + std::to_string(g_actualHz) +
            " maxIntervalMs=" + std::to_string(g_maxIntervalMs));
        g_statsStartMs = nowMs;
        g_callbackCount = 0;
        g_maxIntervalMs = 0;
    }
}

void TimerCallback(char*, ProUITimerID, ProAppData) {
    if (GetCurrentThreadId() != g_mainThreadId) {
        g_unsafeThread = true;
        Log("Unsafe timer thread; view control permanently blocked for this session");
        return;
    }
    if (g_unsafeThread || !g_visible) {
        return;
    }

    std::uint64_t nowMs = GetTickCount64();
    UpdateStats(nowMs);
    if (!g_probeDone) {
        g_probeDone = true;
        Log("Empty timer probe completed on the dialog command thread");
        SetStatus(L"Timer thread verified; running read-only checks");
        StartTimer();
        return;
    }

    if (!g_receiver.IsRunning() && g_view.IsArmed()) {
        g_view.Disarm();
    }

    MotionFrame frame;
    std::uint64_t localMs = 0;
    bool hasFrame = g_sample.Read(frame, localMs);
    ViewTick tick = g_view.Tick(hasFrame, frame, localMs, nowMs);
    g_readOnlyOk = tick.contextOk && tick.writeOk;
    if (!g_receiver.IsRunning()) {
        tick.status = L"UDP receiver stopped; view control disabled";
        tick.writeOk = false;
    }
    SetStatus(tick.status);
    StartTimer();
}

void CloseDialog(char*, char*, ProAppData) {
    g_visible = false;
    if (g_timerCreated) {
        ProUIDialogTimerStop(g_timer);
    }
    g_view.Disarm();
    g_receiver.Stop();
    ProUIDialogHide(g_dialog);
}

void EnableMotion(char*, char*, ProAppData) {
    if (g_unsafeThread || !g_probeDone || !g_readOnlyOk) {
        SetStatus(L"Read-only checks have not passed");
        return;
    }
    if (!g_receiver.IsRunning()) {
        SetStatus(L"UDP receiver is not running");
        return;
    }
    g_view.Arm();
    SetStatus(L"Armed; waiting for bridge");
}

void DisableMotion(char*, char*, ProAppData) {
    g_view.Disarm();
    SetStatus(L"Disabled");
}

void SetRate(int intervalMs) {
    if (g_intervalMs == intervalMs) {
        return;
    }
    g_intervalMs = intervalMs;
    g_view.Disarm();
    if (g_timerCreated && g_visible) {
        ProUIDialogTimerStop(g_timer);
        StartTimer();
    }
    SetStatus(L"Rate changed; enable motion again");
}

void Rate2(char*, char*, ProAppData) {
    SetRate(500);
}

void Rate5(char*, char*, ProAppData) {
    SetRate(200);
}

void Rate10(char*, char*, ProAppData) {
    SetRate(100);
}

}

void UiInit(unsigned long mainThreadId) {
    g_mainThreadId = mainThreadId;
}

void UiOpen() {
    if (g_visible || g_unsafeThread) {
        return;
    }
    unsigned long commandThreadId = GetCurrentThreadId();
    if (commandThreadId != g_mainThreadId) {
        Log("Dialog command thread differs from initialization; using the command thread as the timer baseline");
        g_mainThreadId = commandThreadId;
    }

    if (!g_created) {
        if (ProUIDialogCreate(g_dialog, g_resource) != PRO_TK_NO_ERROR ||
            ProUIDialogDialogstyleSet(g_dialog, PROUIDIALOGSTYLE_MODELESS) != PRO_TK_NO_ERROR ||
            ProUIDialogCloseActionSet(g_dialog, CloseDialog, nullptr) != PRO_TK_NO_ERROR ||
            ProUIPushbuttonActivateActionSet(g_dialog, g_enableButton, EnableMotion, nullptr) != PRO_TK_NO_ERROR ||
            ProUIPushbuttonActivateActionSet(g_dialog, g_disableButton, DisableMotion, nullptr) != PRO_TK_NO_ERROR ||
            ProUIPushbuttonActivateActionSet(g_dialog, g_rate2Button, Rate2, nullptr) != PRO_TK_NO_ERROR ||
            ProUIPushbuttonActivateActionSet(g_dialog, g_rate5Button, Rate5, nullptr) != PRO_TK_NO_ERROR ||
            ProUIPushbuttonActivateActionSet(g_dialog, g_rate10Button, Rate10, nullptr) != PRO_TK_NO_ERROR ||
            ProUIPushbuttonActivateActionSet(g_dialog, g_closeButton, CloseDialog, nullptr) != PRO_TK_NO_ERROR) {
            Log("Dialog initialization failed");
            ProUIDialogDestroy(g_dialog);
            return;
        }
        g_created = true;
    }

    g_view.Disarm();
    g_probeDone = false;
    g_readOnlyOk = false;
    std::string receiverError;
    if (!g_receiver.Start(receiverError)) {
        Log(receiverError);
    }

    g_visible = true;
    ProError error;
    int status = 0;
    if (!g_activated) {
        error = ProUIDialogActivate(g_dialog, &status);
        if (error == PRO_TK_NO_ERROR) {
            g_activated = true;
        }
    } else {
        error = ProUIDialogShow(g_dialog);
    }
    if (error != PRO_TK_NO_ERROR) {
        Log("Dialog display failed: " + std::to_string(error));
        g_visible = false;
        g_receiver.Stop();
        return;
    }

    g_statsStartMs = 0;
    g_lastCallbackMs = 0;
    g_callbackCount = 0;
    g_actualHz = 0.0;
    SetStatus(receiverError.empty() ? L"Starting read-only checks"
                                    : std::wstring(receiverError.begin(), receiverError.end()));
    StartTimer();
}

void UiShutdown() {
    g_visible = false;
    if (g_timerCreated) {
        ProUIDialogTimerStop(g_timer);
    }
    g_view.Disarm();
    g_receiver.Stop();
    if (g_timerCreated) {
        ProUITimerDestroy(g_timer);
        g_timer = nullptr;
        g_timerCreated = false;
    }
    if (g_created) {
        ProUIDialogDestroy(g_dialog);
        g_created = false;
    }
    g_activated = false;
}

}
