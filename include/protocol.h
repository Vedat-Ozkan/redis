#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

int32_t parse_req(const uint8_t* data, size_t size, std::vector<std::string>& out);
