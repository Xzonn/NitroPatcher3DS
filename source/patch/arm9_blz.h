#pragma once
#include "rom.h"

namespace np3d {

struct Arm9Image {
    SourcePtr data;
    bool compressed;
    uint32_t postSize;
};
Arm9Image unpackArm9(Workspace& work, SourcePtr input, Header& header, bool nitrocode);
SourcePtr packArm9(Workspace& work, SourcePtr input, Header& header, bool nitrocode, uint32_t postSize);

}
