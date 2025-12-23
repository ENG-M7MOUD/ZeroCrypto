#include "Crypto.h"
#include <windows.h>
#include <wincrypt.h>

#pragma comment(lib, "crypt32.lib")

std::vector<uint8_t> Encrypt(const std::vector<uint8_t>& data) {
    if (data.empty()) return {};

    DATA_BLOB input;
    input.pbData = const_cast<BYTE*>(data.data());
    input.cbData = (DWORD)data.size();
    
    DATA_BLOB output;
    // Encrypt validation: Uses current user credentials.
    // L"ZeroCryptoLogs" is the description.
    if (CryptProtectData(&input, L"ZeroCryptoLogs", NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        std::vector<uint8_t> result(output.pbData, output.pbData + output.cbData);
        LocalFree(output.pbData);
        return result;
    }
    return {};
}

std::vector<uint8_t> Decrypt(const std::vector<uint8_t>& data) {
    if (data.empty()) return {};

    DATA_BLOB input;
    input.pbData = const_cast<BYTE*>(data.data());
    input.cbData = (DWORD)data.size();
    
    DATA_BLOB output;
    if (CryptUnprotectData(&input, NULL, NULL, NULL, NULL, 0, &output)) {
        std::vector<uint8_t> result(output.pbData, output.pbData + output.cbData);
        LocalFree(output.pbData);
        return result;
    }
    return {};
}