#pragma once
#include "core.h"
#include "miniz.h"
#include <map>

namespace np3d {

struct Package {
    mz_zip_archive archive = {};
    mz_file_read_func reader = nullptr;
    void* readOpaque = nullptr;
    std::map<std::string, unsigned int> entries;
    Workspace& work;
    unsigned int next = 0;
    Package(const std::string& path, Workspace& work);
    ~Package();
    bool has(const std::string& name) const;
    std::string extract(const std::string& name);
    std::vector<uint8_t> bytes(const std::string& name);
    SourcePtr replace(const std::string& name, SourcePtr source);
};

}
