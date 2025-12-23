//Crypto.h
#pragma once
#include <vector>
#include <cstdint> 

std::vector<uint8_t> Encrypt(const std::vector<uint8_t>& data);
std::vector<uint8_t> Decrypt(const std::vector<uint8_t>& data);