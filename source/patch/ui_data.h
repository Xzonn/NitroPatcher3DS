#pragma once
#include "core.h"
#include <map>

namespace np3d {

struct PatchInfo {
    std::string name, author, homepage, version, readme;
    bool beta = false;
};
PatchInfo inspectPatch(const std::string& path);

struct Settings {
    std::string language;
    std::map<std::string, std::vector<std::string>> games;
};
Settings loadSettings(const std::string& root);
void saveSettings(const std::string& root, const Settings& settings);

}
