#pragma once
#include "core.h"
#include "miniz.h"
#include <map>

namespace np3d {

struct Package {
    mz_zip_archive archive = {};
#if NP3D_PREPARE_TRACE
    mz_file_read_func reader = nullptr;
    void* readOpaque = nullptr;
#endif
    std::map<std::string, unsigned int> entries;
    Workspace& work;
    Package(const std::string& path, Workspace& work);
    ~Package();
    bool has(const std::string& name) const;
    bool needsMd5() const;
    SourcePtr delta(const std::string& name, SourcePtr source, const std::string& label);
    std::vector<uint8_t> bytes(const std::string& name);
    SourcePtr replace(const std::string& name, SourcePtr source);
};

}
