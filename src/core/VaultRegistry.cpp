#include "VaultRegistry.h"
#include "SystemUtils.h"
#include <fstream>

static std::vector<Vault> vaults;
static int activeIndex = -1;

// Use SystemUtils to get the correct base directory for the config helper
static std::string GetCfgPath() { 
    std::string base = SystemUtils::GetBaseDir();
    if (base.empty()) return "zerocrypto.cfg"; 
    return base + "\\zerocrypto.cfg"; 
}

void VaultRegistry::Load() {
    vaults.clear();
    std::ifstream f(GetCfgPath());
    if (!f) return;

    int count;
    f >> count >> activeIndex;

    for (int i = 0; i < count; i++) {
        Vault v;
        // NOTE: This format assumes no spaces in name/path. 
        // Existing behavior preserved.
        f >> v.name >> v.path >> v.letter;
        vaults.push_back(v);
    }
}

void VaultRegistry::Save() {
    std::ofstream f(GetCfgPath());
    f << vaults.size() << " " << activeIndex << "\n";
    for (auto& v : vaults)
        f << v.name << " " << v.path << " " << v.letter << "\n";
}

void VaultRegistry::Sanitize() {
    if (vaults.empty()) return;
    bool changed = false;
    for (auto it = vaults.begin(); it != vaults.end(); ) {
        // Remove empty paths or dummy defaults
        if (it->path.empty() || (it->name == "A" && it->path.length() < 3)) {
            it = vaults.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    if (changed) Save();
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
