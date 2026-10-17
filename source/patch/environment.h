#pragma once

#include "core.h"

namespace np3d {

void environmentRecord(FILE* report, const char* format, ...);
void probeFilesystem(FILE* report, const std::string& directory);

}
