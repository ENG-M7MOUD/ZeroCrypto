#include "VaultRegistry.h"
#include <fstream>

static std::vector<Vault> vaults;
static int activeIndex = -1;

void VaultRegistry::Load() {
    vaults.clear();
    std::ifstream f("zerocrypto.cfg");
    if (!f) return;

    int count;
    f >> count >> activeIndex;

    for (int i = 0; i < count; i++) {
        Vault v;
        f >> v.name >> v.path >> v.letter;
        vaults.push_back(v);
    }
}

void VaultRegistry::Save() {
    std::ofstream f("zerocrypto.cfg");
    f << vaults.size() << " " << activeIndex << "\n";
    for (auto& v : vaults)
        f << v.name << " " << v.path << " " << v.letter << "\n";
}

void VaultRegistry::Add(const Vault& v) {
    vaults.push_back(v);
    if (activeIndex == -1)
        activeIndex = 0;
}

std::vector<Vault>& VaultRegistry::All() {
    return vaults;
}

Vault* VaultRegistry::GetActive() {
    if (activeIndex < 0 || activeIndex >= (int)vaults.size())
        return nullptr;
    return &vaults[activeIndex];
}

void VaultRegistry::SetActive(int index) {
    if (index >= 0 && index < (int)vaults.size())
        activeIndex = index;
}
