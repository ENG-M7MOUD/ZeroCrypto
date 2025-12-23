// ProcessUtils.h - Helper function
#include <windows.h>
#include <string>


bool ExecuteSilent(const std::string& appPath, const std::string& args, bool wait = true) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    SecureZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    // CreateProcess
    std::string cmdLine = "\"" + appPath + "\" " + args;
    
    if (!CreateProcessA(NULL, &cmdLine[0], NULL, NULL, FALSE, 
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return false;
    }

    if (wait) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return (exitCode == 0);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}