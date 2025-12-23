//Crypto.cpp
#include "Crypto.h"
#include <windows.h>
#include <wincrypt.h>
#include <vector>


static std::vector<uint8_t> sessionKey;

void InitCrypto() {
    if (!sessionKey.empty()) return;
    
    HCRYPTPROV hProv = 0;
    
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        sessionKey.resize(32);
        
        CryptGenRandom(hProv, 32, sessionKey.data());
        CryptReleaseContext(hProv, 0);
    } else {
        // Fallback 
        sessionKey = { /* ... static values as last resort ... */ };
    }
}

std::vector<uint8_t> Encrypt(const std::vector<uint8_t>& data) {
    if (sessionKey.empty()) InitCrypto();
    
    std::vector<uint8_t> out = data;

    
    for (size_t i = 0; i < out.size(); i++)
        out[i] ^= sessionKey[i % sessionKey.size()];
    
    return out;
}

std::vector<uint8_t> Decrypt(const std::vector<uint8_t>& data) {
    return Encrypt(data); 
}