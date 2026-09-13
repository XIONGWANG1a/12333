#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "aimbot.hpp"
#include "esp.hpp"
#include "menu.hpp"
#include <Windows.h>
#include <chrono>
#include <algorithm>

namespace
{
    using SteadyClock = std::chrono::steady_clock;
    SteadyClock::time_point triggerTargetAcquired{};
    SteadyClock::time_point lastTriggerShot{};
    int32_t currentTriggerTargetIndex = -1;

    void resetTriggerState()
    {
        triggerTargetAcquired = {};
        currentTriggerTargetIndex = -1;
    }
}

bool aimbot::init()
{
    pID = esp::pID;
    modBase = esp::modBase;
    return pID != 0 && modBase != 0;
}

void aimbot::updateTriggerbot(const menu::RuntimeConfig& config)
{
    if (!config.triggerbotEnabled ||
        config.inputSuppressed ||
        !sdl_renderer::isInputAllowed()) {
        resetTriggerState();
        return;
    }

    if (!(GetAsyncKeyState(config.triggerbotKey) & 0x8000)) {
        resetTriggerState();
        return;
    }

    if (!esp::localPlayer.isValid) {
        resetTriggerState();
        return;
    }

    const int32_t crosshairEntityIndex =
        esp::localPlayer.crosshairEntityIndex;
    if (crosshairEntityIndex <= 0) {
        resetTriggerState();
        return;
    }

    const esp::EnemySnapshot enemies =
        esp::getEnemySnapshot();
    if (!enemies) {
        resetTriggerState();
        return;
    }

    bool liveEnemyUnderCrosshair = false;
    for (const EnemyInfo& enemy : *enemies) {
        if (static_cast<int32_t>(enemy.entityIndex) ==
            crosshairEntityIndex) {
            liveEnemyUnderCrosshair = true;
            break;
        }
    }
    if (!liveEnemyUnderCrosshair) {
        resetTriggerState();
        return;
    }

    const auto now = SteadyClock::now();
    if (currentTriggerTargetIndex != crosshairEntityIndex) {
        currentTriggerTargetIndex = crosshairEntityIndex;
        triggerTargetAcquired = now;
    }

    const auto acquisitionDelay = std::chrono::milliseconds(
        std::max(0, config.triggerbotDelay));
    if (now - triggerTargetAcquired < acquisitionDelay) {
        return;
    }

    constexpr auto MINIMUM_SHOT_INTERVAL =
        std::chrono::milliseconds(50);
    if (lastTriggerShot.time_since_epoch().count() != 0 &&
        now - lastTriggerShot < MINIMUM_SHOT_INTERVAL) {
        return;
    }

    if (!sdl_renderer::isInputAllowed()) {
        resetTriggerState();
        return;
    }

    INPUT clicks[2]{};
    clicks[0].type = INPUT_MOUSE;
    clicks[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    clicks[1].type = INPUT_MOUSE;
    clicks[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    if (SendInput(2, clicks, sizeof(INPUT)) == 2) {
        lastTriggerShot = now;
    } else {
        resetTriggerState();
    }
}
