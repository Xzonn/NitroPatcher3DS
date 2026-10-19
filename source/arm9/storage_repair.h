#pragma once
#include <cstdint>
#include <string>

namespace storage {
enum class Status { Clean, Repairable, Damaged, Ambiguous, Unsupported, Busy, Memory, IoError, Repaired };
enum class Stage { Directories, Allocation, Compare, Backup, Repair, Verify };
using Progress = void (*)(Stage stage, unsigned int copy, uint32_t completed, uint32_t total);
struct Copy {
    uint32_t files = 0, conflicts = 0, lost = 0, invalid = 0, free = 0;
    std::string problem;
    bool valid() const { return !conflicts && !lost && !invalid; }
};
struct Result {
    Status status = Status::Unsupported;
    Copy copies[2];
    unsigned int count = 0;
    uint32_t differentSectors = 0;
    bool fsInfo = false;
    std::string backup;
};
Result inspect(Progress progress);
Result repair(Progress progress);
bool writeBlocked();
}
