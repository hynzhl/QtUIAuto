#include <windows.h>
#include <QAccessible>
#include <QDebug>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        qInfo() << "QtUIAuto_Inject: DLL injected successfully";

        // TODO: initialize accessibility controller and named pipe client
        // in a separate thread to avoid DllMain limitations
    }
    return TRUE;
}
