#pragma once
#include <vector>
#include <cstdint>
#include <string>

// Encrypts data using Windows DPAPI (CryptProtectData)
// This binds the data to the current user context.
std::vector<uint8_t> Encrypt(const std::vector<uint8_t>& data);

// Decrypts data using Windows DPAPI (CryptUnprotectData)
std::vector<uint8_t> Decrypt(const std::vector<uint8_t>& data);