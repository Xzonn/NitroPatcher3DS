#pragma once
#include "../patch/core.h"

void writePatchedLaunch(const np3d::Prepared& prepared, np3d::SourcePtr card, uint32_t chipId, int consoleModel, bool apPatch);
void startPatchedGame();
