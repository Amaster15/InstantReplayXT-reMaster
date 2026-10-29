#pragma once
#include <cstdint>
#include <string>

struct ReplayPosition {
    int64_t position = 0;
    int64_t duration = 0;
    std::string clip;
};
