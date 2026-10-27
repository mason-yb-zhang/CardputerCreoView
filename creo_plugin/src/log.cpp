#include "log.h"

#include <windows.h>

#include <string>
#include <fstream>
#include <mutex>

namespace ccv {
namespace {

std::mutex g_logMutex;
std::string g_logPath;

}

void LogInit() {
    char base[MAX_PATH] = {};
    DWORD length = GetEnvironmentVariableA("LOCALAPPDATA", base, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        length = GetTempPathA(MAX_PATH, base);
    }
    if (length == 0 || length >= MAX_PATH) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_logMutex);
    const std::string directory = std::string(base) + "\\CardputerCreoView";
    if (!CreateDirectoryA(directory.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        g_logPath.clear();
        return;
    }
    g_logPath = directory + "\\creo_plugin.log";
}

void Log(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logPath.empty()) {
        return;
    }
    std::ofstream stream(g_logPath, std::ios::app);
    if (!stream) {
        return;
    }
    stream << '[' << GetTickCount64() << " ms][thread " << GetCurrentThreadId()
           << "] " << message << '\n';
}

}
