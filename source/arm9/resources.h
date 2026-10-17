#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace np3d {
std::vector<uint8_t> readResource(const char* name);
uint32_t resourceSize(const char* name);
void writeResource(const char* name, const std::string& path);
}
