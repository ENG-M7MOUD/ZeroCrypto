#pragma once
#include <vector>
#include <string>
#include <windows.h>

class SecureBuffer {
    std::vector<char> data;

public:
    SecureBuffer(size_t size) : data(size, 0) {}
    
    // Prevent copying to avoid accidental leaks
    SecureBuffer(const SecureBuffer&) = delete;
    SecureBuffer& operator=(const SecureBuffer&) = delete;

    ~SecureBuffer() {
        Clear();
    }

    // Mutable access
    char* Get() { return data.data(); }
    
    // Const access
    const char* c_str() const { return data.data(); } 
    
    size_t Size() const { return data.size(); }
    
    // WARNING: This creates a copy of the password in memory that is NOT automatically wiped.
    // Use only when necessary.
    std::string ToString() const { return std::string(data.data()); }
    
    void Clear() { 
        SecureZeroMemory(data.data(), data.size()); 
    }
};
