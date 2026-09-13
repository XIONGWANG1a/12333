#include "features/esp.hpp"
#include "features/aimbot.hpp"
#include "core/renderer/sdl_renderer.h"
#include "features/menu.hpp"
#include "core/diagnostics.hpp"
#include <algorithm>
#include <thread>
#include <atomic>
#include <iostream>
#include <chrono>
#include <Windows.h>
#include "imgui.h"

uint32_t WIDTH;
uint32_t HEIGHT;
uint32_t WINDOW_W;
uint32_t WINDOW_H;
int32_t VIEWPORT_X;
int32_t VIEWPORT_Y;
uint32_t VIEWPORT_W;
uint32_t VIEWPORT_H;

void renderWaitingScreen(int dotCount)
{
    const float dpiScale = sdl_renderer::getDpiScale();
    const float margin = std::max(8.0f, 16.0f * dpiScale);
    const float windowWidth = std::min(
        480.0f * dpiScale,
        std::max(1.0f, static_cast<float>(WIDTH) - margin * 2.0f));
    const float windowHeight = std::min(
        270.0f * dpiScale,
        std::max(1.0f, static_cast<float>(HEIGHT) - margin * 2.0f));
    ImGui::SetNextWindowPos(
        ImVec2(
            WIDTH / 2.0f - windowWidth / 2.0f,
            HEIGHT / 2.0f - windowHeight / 2.0f
        ),
        ImGuiCond_Always
    );
    ImGui::SetNextWindowSize(
        ImVec2(windowWidth, windowHeight),
        ImGuiCond_Always
    );

    ImGui::Begin("Aegis // Connection", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    {
        const ImVec2 position = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        sdl_renderer::setInteractiveRect(
            position.x,
            position.y,
            size.x,
            size.y);
    }

    char dots[4] = {0};
    for (int i = 0; i < dotCount; i++) dots[i] = '.';

    ImGui::TextColored(
        ImVec4(0.330f, 0.800f, 1.000f, 1.0f),
        "AEGIS");
    ImGui::SameLine();
    ImGui::TextColored(
        ImVec4(0.500f, 0.570f, 0.670f, 1.0f),
        "CS2 TRIGGERBOT");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 12.0f * dpiScale));
    ImGui::TextColored(
        ImVec4(0.930f, 0.960f, 1.000f, 1.0f),
        "Waiting for Counter-Strike 2%s",
        dots);
    ImGui::TextColored(
        ImVec4(0.500f, 0.570f, 0.670f, 1.0f),
        "The client and its active monitor will be detected automatically.");

    ImGui::Dummy(ImVec2(0.0f, 8.0f * dpiScale));
    ImGui::TextColored(
        ImVec4(0.500f, 0.570f, 0.670f, 1.0f),
        "Checking every 3 seconds  |  Press F9 to exit");

    ImGui::End();
}

namespace
{
    class ScopedHandle
    {
    public:
        explicit ScopedHandle(HANDLE handle = nullptr)
            : handle_(handle)
        {
        }

        ~ScopedHandle()
        {
            if (handle_) {
                CloseHandle(handle_);
            }
        }

        ScopedHandle(const ScopedHandle&) = delete;
        ScopedHandle& operator=(const ScopedHandle&) = delete;

        HANDLE get() const
        {
            return handle_;
        }

    private:
        HANDLE handle_ = nullptr;
    };

    bool hasArgument(
        int argc,
        char* argv[],
        const char* expected)
    {
        for (int index = 1; index < argc; ++index) {
            if (_stricmp(argv[index], expected) == 0) {
                return true;
            }
        }
        return false;
    }

    void showFatalError(const wchar_t* message)
    {
        diagnostics::log(message);
        MessageBoxW(
            nullptr,
            message,
            L"CS2 Triggerbot",
            MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    }
}

int main(int argc, char* argv[])
{
    SetLastError(ERROR_SUCCESS);
    ScopedHandle instanceMutex(CreateMutexW(
        nullptr,
        FALSE,
        L"Local\\CS2ExternalOverlay-76F5E6C2-235A-4AA0"));
    const DWORD mutexError = GetLastError();
    if (!instanceMutex.get()) {
        showFatalError(
            L"Unable to create the single-instance guard.");
        return -1;
    }
    if (mutexError == ERROR_ALREADY_EXISTS) {
        MessageBoxW(
            nullptr,
            L"CS2 Triggerbot is already running.",
            L"CS2 Triggerbot",
            MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
        return 0;
    }

    memory::SetWritesAllowed(
        hasArgument(
            argc,
            argv,
            "--allow-memory-writes"));
    menu::loadPersistentSettings();

    FreeConsole();

    while (sdl_renderer::running)
    {
        sdl_renderer::menuVisible = true;
        if (!sdl_renderer::initWaiting()) {
            showFatalError(
                L"Unable to initialize the overlay. Windows 10/11 with "
                L"per-monitor DPI awareness and a working SDL2.dll are required.");
            memory::Close();
            return -1;
        }
        if (!sdl_renderer::initImGui()) {
            showFatalError(L"Unable to initialize ImGui.");
            sdl_renderer::destroy();
            memory::Close();
            return -1;
        }

        int dotCount = 0;
        DWORD lastCheckTime = 0;
        bool gameFound = false;
        while (sdl_renderer::running && !gameFound)
        {
            sdl_renderer::pollEvents();

            const DWORD currentTime = GetTickCount();
            if (currentTime - lastCheckTime >= 3000 ||
                lastCheckTime == 0) {
                lastCheckTime = currentTime;
                dotCount = (dotCount % 3) + 1;
                if (esp::init()) {
                    gameFound = true;
                    break;
                }
            }

            if (!sdl_renderer::beginFrame()) {
                break;
            }
            sdl_renderer::newFrameImGui();
            renderWaitingScreen(dotCount);
            sdl_renderer::renderImGui();
            sdl_renderer::endFrame();
            Sleep(16);
        }

        sdl_renderer::shutdownImGui();
        sdl_renderer::destroy();
        if (!sdl_renderer::running) {
            break;
        }
        if (!gameFound ||
            !sdl_renderer::init(
                L"Counter-Strike 2",
                static_cast<DWORD>(esp::pID)) ||
            !sdl_renderer::initImGui() ||
            !aimbot::init()) {
            diagnostics::log(
                L"Game attach failed or raced with shutdown; retrying.");
            sdl_renderer::shutdownImGui();
            sdl_renderer::destroy();
            esp::clearRuntimeState();
            memory::Close();
            std::this_thread::sleep_for(
                std::chrono::milliseconds(500));
            continue;
        }

        std::atomic<bool> dataRunning{true};
        std::thread dataThread(
            [&dataRunning]() {
            while (dataRunning.load(std::memory_order_relaxed)) {
                const menu::RuntimeConfig config =
                    menu::getRuntimeConfig();
                const bool gameForeground =
                    sdl_renderer::isGameForeground();
                if (!gameForeground) {
                    esp::clearRuntimeState();
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(25));
                    continue;
                }

                menu::RuntimeConfig samplingConfig = config;
                if (!gameForeground) {
                    samplingConfig.triggerbotEnabled = false;
                }
                esp::updateEntities(samplingConfig);
                if (gameForeground) {
                    aimbot::updateTriggerbot(config);
                }

                std::this_thread::sleep_for(
                    std::chrono::milliseconds(4));
            }
        });

        auto nextRenderTime = std::chrono::steady_clock::now();
        while (sdl_renderer::running &&
               !sdl_renderer::isGameDisconnected())
        {
            sdl_renderer::pollEvents();
            sdl_renderer::updateWindowPosition();
            if (!sdl_renderer::isGameVisible()) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(25));
                nextRenderTime = std::chrono::steady_clock::now();
                continue;
            }

            if (!sdl_renderer::beginFrame()) {
                break;
            }
            sdl_renderer::newFrameImGui();
            menu::render();
            sdl_renderer::renderImGui();
            sdl_renderer::endFrame();

            const int refreshRate = std::max(
                60,
                sdl_renderer::getTargetRefreshRate());
            const auto renderInterval =
                std::chrono::nanoseconds(
                    1000000000LL / refreshRate);
            nextRenderTime += renderInterval;
            const auto now = std::chrono::steady_clock::now();
            if (nextRenderTime > now) {
                std::this_thread::sleep_until(nextRenderTime);
            } else {
                nextRenderTime = now;
            }
        }

        dataRunning.store(false, std::memory_order_relaxed);
        dataThread.join();
        sdl_renderer::shutdownImGui();
        sdl_renderer::destroy();
        esp::clearRuntimeState();
        memory::Close();

        if (sdl_renderer::running) {
            diagnostics::log(
                L"CS2 disconnected; returning to the waiting screen.");
            std::this_thread::sleep_for(
                std::chrono::milliseconds(500));
        }
    }

    menu::savePersistentSettings();
    memory::Close();
    return 0;
}
