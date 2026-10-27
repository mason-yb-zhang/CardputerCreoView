#include "log.h"
#include "ui_timer.h"

#include <ProMenu.h>
#include <ProMenuBar.h>
#include <ProToolkit.h>
#include <ProUICmd.h>
#include <ProUtil.h>

#include <windows.h>

namespace {

wchar_t g_messageFile[] = L"cardputer_msg.txt";
uiCmdCmdId g_openCommand = 0;

uiCmdAccessState AccessAll(uiCmdAccessMode) {
    return ACCESS_AVAILABLE;
}

extern "C" int OpenController(uiCmdCmdId, uiCmdValue*, void*) {
    ccv::UiOpen();
    return 0;
}

}

extern "C" int user_initialize(int, char**, char*, char*, wchar_t[80]) {
    ccv::LogInit();
    int wcharSize = 0;
    if (ProWcharSizeVerify(sizeof(wchar_t), &wcharSize) != PRO_TK_NO_ERROR ||
        wcharSize != sizeof(wchar_t)) {
        ccv::Log("ProWcharSizeVerify failed");
        return -1;
    }

    ccv::UiInit(GetCurrentThreadId());
    ProError error = ProCmdActionAdd(const_cast<char*>("CardputerCreoOpen"),
                                     OpenController, uiProeAsynch, AccessAll,
                                     PRO_B_FALSE, PRO_B_FALSE, &g_openCommand);
    if (error != PRO_TK_NO_ERROR) {
        ccv::Log("ProCmdActionAdd failed: " + std::to_string(error));
        return -1;
    }

    ProCmdDesignate(g_openCommand, const_cast<char*>("Open_Label"),
                    const_cast<char*>("Open_Help"), const_cast<char*>("Open_Help"),
                    g_messageFile);
    ProMenubarMenuAdd(const_cast<char*>("CardputerMotion"),
                      const_cast<char*>("CardputerMotion"),
                      const_cast<char*>("Help"), PRO_B_FALSE, g_messageFile);
    ProMenubarmenuPushbuttonAdd(const_cast<char*>("CardputerMotion"),
                                const_cast<char*>("CardputerCreoOpenButton"),
                                const_cast<char*>("Open_Label"),
                                const_cast<char*>("Open_Help"),
                                const_cast<char*>(""), PRO_B_FALSE,
                                g_openCommand, g_messageFile);
    ccv::Log("Cardputer Creo View initialized; output is disabled");
    return 0;
}

extern "C" void user_terminate() {
    ccv::UiShutdown();
    ccv::Log("Cardputer Creo View terminated");
}
