#pragma once
#include "../patch/core.h"

np3d::SourcePtr openGameCard(uint32_t* chipId = nullptr);
bool gameCardMatches(np3d::SourcePtr card);
