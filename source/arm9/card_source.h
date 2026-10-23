#pragma once
#include "../patch/core.h"

np3d::SourcePtr openGameCard(uint32_t* chipId, std::string& error);
bool gameCardMatches(np3d::SourcePtr card);
