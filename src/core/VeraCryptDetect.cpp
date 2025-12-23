#include "VeraCryptDetect.h"
#include <windows.h>
#include <cstdio>

std::map<char, std::string> GetMountedVeraCryptVolumes()
{
    std::map<char, std::string> result;

    FILE* pipe = _popen(
        "assets\\veracrypt\\VeraCrypt.exe /list", "r"
    );
    if (!pipe) return result;

    char line[512];
    while (fgets(line, sizeof(line), pipe))
    {
    
        if (strlen(line) < 5) continue;

        char letter = line[0];
        char* path = strchr(line, ':');
        if (!path) continue;

    
        path += 3;

        std::string vaultPath = path;
        vaultPath.erase(vaultPath.find_last_not_of("\r\n") + 1);

        result[letter] = vaultPath;
    }

    _pclose(pipe);
    return result;
}
