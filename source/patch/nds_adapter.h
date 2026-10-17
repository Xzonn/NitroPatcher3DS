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
};
std::vector<NamedFile> enumerateFiles(SourcePtr source, const Header& header);

}
