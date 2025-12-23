// SecureBuffer.h
#pragma once
#include <vector>
#include <string>
#include <windows.h>

class SecureBuffer {
    std::vector<char> data;

public:
    SecureBuffer(size_t size) : data(size, 0) {}
    
    ~SecureBuffer() {
        
        SecureZeroMemory(data.data(), data.size());
    }

    char* Get() { return data.data(); }
    size_t Size() const { return data.size(); }
    
    
    std::string ToString() const { return std::string(data.data()); }
    
    void Clear() { SecureZeroMemory(data.data(), data.size()); }
};