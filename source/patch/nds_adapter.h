#pragma once
#include "core.h"
#include "ndstool.h"
#include "overlay.h"
#include "crc.h"
#include "encryption.h"

namespace np3d {

struct NamedFile {
    std::string name;
    uint32_t id;
    uint32_t offset;
    uint32_t length;
    std::string rawName;
    uint32_t directory = 0;
};
struct NamedDirectory {
    std::string name, rawName;
    uint32_t parent;
    std::vector<uint32_t> files, children;
};
std::vector<NamedFile> enumerateFiles(SourcePtr source, const Header& header, bool canonical = false,
    std::vector<NamedDirectory>* directories = nullptr);

}
