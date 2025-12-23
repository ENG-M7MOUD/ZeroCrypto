#include "SystemUtils.h"
#include <fstream>
#include <algorithm>
#include <vector>

namespace SystemUtils {

    std::string GetBaseDir() {
        char buffer[MAX_PATH];
        if (GetModuleFileNameA(NULL, buffer, MAX_PATH) == 0) return "";
        std::string::size_type pos = std::string(buffer).find_last_of("\\/");
        return std::string(buffer).substr(0, pos);
    }

    std::string GetAbsolutePath(const std::string& relativePath) {
        char fullPath[MAX_PATH];
        if (GetFullPathNameA(relativePath.c_str(), MAX_PATH, fullPath, NULL) != 0) return std::string(fullPath);
        return relativePath;
    }

    bool FileExists(const std::string& name) {
        std::ifstream f(name.c_str());
        return f.good();
    }

    void SecureWipeFile(const std::string& path) {
        HANDLE hFile = CreateFileA(path.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD size = GetFileSize(hFile, NULL);
            if (size != INVALID_FILE_SIZE) {
                std::vector<char> zeros(4096, 0);
                DWORD written;
                DWORD bytesLeft = size;
                SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
                while (bytesLeft > 0) {
                    DWORD chunk = (bytesLeft < 4096) ? bytesLeft : 4096;
                    // Check if write fails to avoid infinite loop
                    if (!WriteFile(hFile, zeros.data(), chunk, &written, NULL) || written == 0) {
                        break;
                    }
                    bytesLeft -= written;
                }
            }
            FlushFileBuffers(hFile);
            CloseHandle(hFile);
        }
        DeleteFileA(path.c_str());
    }

    void ExtractFileName(const char* fullPath, char* dest, size_t size) {
        if (!fullPath || !dest || size == 0) return;
        std::string pathStr = fullPath;
        
        size_t lastSlash = pathStr.find_last_of("\\/");
        if (lastSlash != std::string::npos) pathStr = pathStr.substr(lastSlash + 1);
        
        size_t lastDot = pathStr.find_last_of(".");
        if (lastDot != std::string::npos) pathStr = pathStr.substr(0, lastDot);
        
        // Use safer copy ensuring null termination
        memset(dest, 0, size);
        size_t copyLen = (pathStr.length() < size) ? pathStr.length() : (size - 1);
        memcpy(dest, pathStr.c_str(), copyLen);
    }

    DWORD ExecuteProcess(const std::string& appPath, const std::string& args, bool showWindow, bool wait) {
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        SecureZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = showWindow ? SW_SHOWNORMAL : SW_HIDE;

        // Construct command line in a mutable buffer
        std::string cmdStr = "\"" + appPath + "\" " + args;
        std::vector<char> cmdLine(cmdStr.begin(), cmdStr.end());
        cmdLine.push_back('\0'); 

        if (!CreateProcessA(NULL, cmdLine.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            SecureZeroMemory(cmdLine.data(), cmdLine.size());
            return (DWORD)-1;
        }

        // Wipe command line buffer immediately
        SecureZeroMemory(cmdLine.data(), cmdLine.size());

        if (wait) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD exitCode = 0;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return exitCode;
        } else {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return 0; // Success but we don't know the exit code
        }
    }

    bool IsDriveMounted(char letter) {
        DWORD mask = GetLogicalDrives();
        return (mask & (1 << (toupper(letter) - 'A')));
    }
}
