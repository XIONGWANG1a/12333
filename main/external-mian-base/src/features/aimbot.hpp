#pragma once

#include "core/memory/memory.hpp"
#include "utils/math/vector.hpp"
#include "offsets.hpp"
#include "client_dll.hpp"
#include <cstdint>

namespace menu
{
    struct RuntimeConfig;
}

namespace aimbot
{
    inline uintptr_t pID = 0;
    inline uintptr_t modBase = 0;

    bool init();
    void updateTriggerbot(const menu::RuntimeConfig& config);
}
