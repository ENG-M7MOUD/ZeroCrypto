//SecurityCore.hpp
#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <wincrypt.h>
#include <fstream>
#include <memory>

#pragma comment(lib, "crypt32.lib")

// ================== 1. Secure Memory (RAII) ==================

class SecureBuffer {
    std::vector<char> data;
public:
    SecureBuffer(size_t size) : data(size, 0) {}
    

    SecureBuffer(const SecureBuffer&) = delete;
    SecureBuffer& operator=(const SecureBuffer&) = delete;

    ~SecureBuffer() { Wipe(); }

    char* Data() { return data.data(); }
    size_t Size() const { return data.size(); }
    
    void Wipe() {
        RtlSecureZeroMemory(data.data(), data.size());
    }
};

// ================== 2. Secure File Wipe ==================

inline void SecureWipeFile(const std::string& path) {
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER size;
        GetFileSizeEx(hFile, &size);
        std::vector<BYTE> zeros(4096, 0);
        long long total = 0;
        DWORD written;
        while (total < size.QuadPart) {
            WriteFile(hFile, zeros.data(), (DWORD)zeros.size(), &written, NULL);
            total += written;
        }
        FlushFileBuffers(hFile); 
        CloseHandle(hFile);
    }
    DeleteFileA(path.c_str()); 
}

// ================== 3. DPAPI Encryption ==================

inline std::vector<uint8_t> EncryptDPAPI(const std::string& plaintext) {
    DATA_BLOB input;
    input.pbData = (BYTE*)plaintext.data();
    input.cbData = (DWORD)plaintext.size();
    
    DATA_BLOB output;
    if (CryptProtectData(&input, L"ZeroCryptoLogs", NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        std::vector<uint8_t> result(output.pbData, output.pbData + output.cbData);
        LocalFree(output.pbData);
        return result;
    }
    return {};
}

// ================== 4. Safe Process Execution ==================
inline std::string RunProcess(const std::string& cmd, const std::string& args) {
    std::string fullCmd = "\"" + cmd + "\" " + args;
    
    
    int len = MultiByteToWideChar(CP_ACP, 0, fullCmd.c_str(), -1, NULL, 0);
    std::vector<wchar_t> wCmd(len);
    MultiByteToWideChar(CP_ACP, 0, fullCmd.c_str(), -1, wCmd.data(), len);

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; 
    PROCESS_INFORMATION pi = { 0 };

    if (CreateProcessW(NULL, wCmd.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return (exitCode == 0) ? "SUCCESS" : "FAIL";
    }
    return "FAIL";
}