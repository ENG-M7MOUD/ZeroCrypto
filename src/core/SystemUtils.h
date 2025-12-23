#pragma once
#include <string>
#include <vector>
#include <windows.h>

namespace SystemUtils {
    
    // File System
    std::string GetBaseDir();
    std::string GetAbsolutePath(const std::string& relativePath);
    bool FileExists(const std::string& name);
    void SecureWipeFile(const std::string& path);
    void ExtractFileName(const char* fullPath, char* dest, size_t size);

    // Process
    // cmdLine is passed as a string, but internally converted to a mutable buffer for CreateProcess
    // SafeToLog: if false, we won't log the command line (useful for passwords)
    DWORD ExecuteProcess(const std::string& appPath, const std::string& args, bool showWindow, bool wait = true);

    // Drive Management
    bool IsDriveMounted(char letter);
}
