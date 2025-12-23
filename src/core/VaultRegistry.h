#pragma once
#include <string>
#include <vector>

struct Vault {
    std::string name;
    std::string path;
    char letter;
};

class VaultRegistry {
public:
    static void Load();
    static void Save();

    static void Add(const Vault& v);
    static std::vector<Vault>& All();
    static Vault* GetActive();
    static void SetActive(int index);
};
