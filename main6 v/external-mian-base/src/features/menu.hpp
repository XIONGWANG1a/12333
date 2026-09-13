#pragma once

#include "imgui.h"
#include "core/renderer/sdl_renderer.h"
#include "core/memory/memory.hpp"
#include "core/diagnostics.hpp"
#include <Windows.h>
#include <shellapi.h>
#include <cstdio>
#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace menu
{
    inline std::mutex configMutex;
    inline diagnostics::StartupReport startupReport;

    inline void setStartupReport(diagnostics::StartupReport report)
    {
        startupReport = std::move(report);
    }

    struct RuntimeConfig
    {
        bool triggerbotEnabled = false;
        int triggerbotDelay = 50;
        int triggerbotKey = 0x46;
        bool inputSuppressed = false;

        // Referenced by esp.cpp (set to defaults)
        bool espEnabled = false;
        bool aimbotEnabled = false;
        bool antiFlash = false;
        bool localRadarEnabled = false;
        bool webRadarEnabled = false;
        bool publicRelayEnabled = false;
        bool radarRecordingEnabled = false;
        int radarRefreshRateHz = 20;
        bool bombTimer = false;
        bool grenadeESP = false;
        bool droppedWeaponESP = false;
        bool espWallCheck = true;
        bool espSkeleton = true;
        bool espWeapon = false;
        bool espFlashIndicator = false;
        bool espViewAngle = false;
        bool headOffsetEnabled = true;
        float headOffsetAmount = 5.0f;
        float headOffsetAngleMin = 45.0f;
        float headOffsetAngleMax = 135.0f;
        float aimbotFOV = 10.0f;
        float aimbotSmoothing = 5.0f;
        int aimbotBone = 0;
        bool aimbotVisibleOnly = true;
        int aimbotKey = VK_SHIFT;
        bool smartAimEnabled = false;
        int smartAimPriority = 0;
        float mouseSensitivity = 1.0f;
        float localRadarAnchorX = 0.02f;
        float localRadarAnchorY = 0.08f;
        float localRadarSize = 0.32f;
        float localRadarMarkerSize = 12.0f;
        bool localRadarShowNames = true;

        [[nodiscard]] bool radarSnapshotEnabled() const noexcept
        {
            return localRadarEnabled || webRadarEnabled ||
                publicRelayEnabled || radarRecordingEnabled;
        }
    };

    // Triggerbot Settings
    inline bool triggerbotEnabled = false;
    inline int triggerbotDelay = 50;
    inline int triggerbotKey = 0x46;

    // Menu Toggle Key
    inline int menuToggleKey = VK_F4;
    inline int exitKey = VK_F9;

    // 0 = auto-detect, 1 = stretch, 2 = 4:3, 3 = 16:10
    inline int viewportMode = 0;

    // Hotkey binding state
    inline bool isBindingKey = false;
    inline int* bindingKeyTarget = nullptr;
    inline const char* bindingKeyName = nullptr;
    inline bool bindingWaitingForRelease = false;
    inline std::string bindingError;
    inline bool suppressHotkeysUntilRelease = false;

    inline RuntimeConfig buildRuntimeConfig()
    {
        RuntimeConfig config{};
        config.triggerbotEnabled = triggerbotEnabled;
        config.triggerbotDelay = triggerbotDelay;
        config.triggerbotKey = triggerbotKey;
        config.inputSuppressed =
            isBindingKey || suppressHotkeysUntilRelease;
        return config;
    }

    inline RuntimeConfig runtimeConfigSnapshot = buildRuntimeConfig();

    inline RuntimeConfig getRuntimeConfig()
    {
        std::lock_guard<std::mutex> lock(configMutex);
        return runtimeConfigSnapshot;
    }

    inline void publishRuntimeConfig()
    {
        const RuntimeConfig updated = buildRuntimeConfig();
        std::lock_guard<std::mutex> lock(configMutex);
        runtimeConfigSnapshot = updated;
    }

    inline std::filesystem::path persistentSettingsPath()
    {
        std::array<wchar_t, 32768> localAppData{};
        const DWORD length = GetEnvironmentVariableW(
            L"LOCALAPPDATA",
            localAppData.data(),
            static_cast<DWORD>(localAppData.size()));
        std::filesystem::path directory =
            length > 0 && length < localAppData.size()
                ? std::filesystem::path(
                    std::wstring_view(localAppData.data(), length)) /
                    L"AegisCS2"
                : std::filesystem::temp_directory_path() / L"AegisCS2";
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        return directory / L"settings-v1.ini";
    }

    inline int readPersistentInt(
        const wchar_t* key,
        const int fallback,
        const std::filesystem::path& path)
    {
        return static_cast<int>(GetPrivateProfileIntW(
            L"settings",
            key,
            fallback,
            path.c_str()));
    }

    inline void loadPersistentSettings()
    {
        const std::filesystem::path path = persistentSettingsPath();
        if (readPersistentInt(L"schema", 0, path) != 1) {
            publishRuntimeConfig();
            return;
        }
        triggerbotEnabled = readPersistentInt(
            L"triggerbot_enabled", triggerbotEnabled ? 1 : 0, path) != 0;
        triggerbotDelay = std::clamp(
            readPersistentInt(L"triggerbot_delay", triggerbotDelay, path),
            0, 500);
        triggerbotKey = readPersistentInt(
            L"triggerbot_key", triggerbotKey, path);
        publishRuntimeConfig();
    }

    inline void writePersistentValue(
        const wchar_t* key,
        const std::wstring_view value,
        const std::filesystem::path& path)
    {
        const std::wstring owned(value);
        WritePrivateProfileStringW(
            L"settings",
            key,
            owned.c_str(),
            path.c_str());
    }

    inline void savePersistentSettings()
    {
        const std::filesystem::path path = persistentSettingsPath();
        writePersistentValue(L"schema", L"1", path);
        writePersistentValue(
            L"triggerbot_enabled",
            triggerbotEnabled ? L"1" : L"0", path);
        writePersistentValue(
            L"triggerbot_delay",
            std::to_wstring(std::clamp(triggerbotDelay, 0, 500)), path);
        writePersistentValue(
            L"triggerbot_key",
            std::to_wstring(triggerbotKey), path);
    }

    inline const char* GetKeyName(int vkCode)
    {
        static char keyName[32];

        switch (vkCode)
        {
        case VK_LBUTTON: return "Mouse1";
        case VK_RBUTTON: return "Mouse2";
        case VK_MBUTTON: return "Mouse3";
        case VK_XBUTTON1: return "Mouse4";
        case VK_XBUTTON2: return "Mouse5";
        case VK_BACK: return "Backspace";
        case VK_TAB: return "Tab";
        case VK_RETURN: return "Enter";
        case VK_SHIFT: return "Shift";
        case VK_CONTROL: return "Ctrl";
        case VK_MENU: return "Alt";
        case VK_PAUSE: return "Pause";
        case VK_CAPITAL: return "CapsLock";
        case VK_ESCAPE: return "Escape";
        case VK_SPACE: return "Space";
        case VK_PRIOR: return "PageUp";
        case VK_NEXT: return "PageDown";
        case VK_END: return "End";
        case VK_HOME: return "Home";
        case VK_LEFT: return "Left";
        case VK_UP: return "Up";
        case VK_RIGHT: return "Right";
        case VK_DOWN: return "Down";
        case VK_INSERT: return "Insert";
        case VK_DELETE: return "Delete";
        case VK_LSHIFT: return "LShift";
        case VK_RSHIFT: return "RShift";
        case VK_LCONTROL: return "LCtrl";
        case VK_RCONTROL: return "RCtrl";
        case VK_LMENU: return "LAlt";
        case VK_RMENU: return "RAlt";

        case VK_F1: return "F1";
        case VK_F2: return "F2";
        case VK_F3: return "F3";
        case VK_F4: return "F4";
        case VK_F5: return "F5";
        case VK_F6: return "F6";
        case VK_F7: return "F7";
        case VK_F8: return "F8";
        case VK_F9: return "F9";
        case VK_F10: return "F10";
        case VK_F11: return "F11";
        case VK_F12: return "F12";

        case VK_NUMPAD0: return "Num0";
        case VK_NUMPAD1: return "Num1";
        case VK_NUMPAD2: return "Num2";
        case VK_NUMPAD3: return "Num3";
        case VK_NUMPAD4: return "Num4";
        case VK_NUMPAD5: return "Num5";
        case VK_NUMPAD6: return "Num6";
        case VK_NUMPAD7: return "Num7";
        case VK_NUMPAD8: return "Num8";
        case VK_NUMPAD9: return "Num9";
        case VK_MULTIPLY: return "Num*";
        case VK_ADD: return "Num+";
        case VK_SUBTRACT: return "Num-";
        case VK_DECIMAL: return "Num.";
        case VK_DIVIDE: return "Num/";

        case 0x41: return "A";
        case 0x42: return "B";
        case 0x43: return "C";
        case 0x44: return "D";
        case 0x45: return "E";
        case 0x46: return "F";
        case 0x47: return "G";
        case 0x48: return "H";
        case 0x49: return "I";
        case 0x4A: return "J";
        case 0x4B: return "K";
        case 0x4C: return "L";
        case 0x4D: return "M";
        case 0x4E: return "N";
        case 0x4F: return "O";
        case 0x50: return "P";
        case 0x51: return "Q";
        case 0x52: return "R";
        case 0x53: return "S";
        case 0x54: return "T";
        case 0x55: return "U";
        case 0x56: return "V";
        case 0x57: return "W";
        case 0x58: return "X";
        case 0x59: return "Y";
        case 0x5A: return "Z";

        case 0x30: return "0";
        case 0x31: return "1";
        case 0x32: return "2";
        case 0x33: return "3";
        case 0x34: return "4";
        case 0x35: return "5";
        case 0x36: return "6";
        case 0x37: return "7";
        case 0x38: return "8";
        case 0x39: return "9";

        default:
            sprintf_s(keyName, "Key(0x%02X)", vkCode);
            return keyName;
        }
    }

    inline int GetPressedKey()
    {
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) return VK_LBUTTON;
        if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) return VK_RBUTTON;
        if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) return VK_MBUTTON;
        if (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) return VK_XBUTTON1;
        if (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) return VK_XBUTTON2;

        for (int i = 0x08; i <= 0xFE; i++)
        {
            if (i == VK_ESCAPE) continue;
            if (GetAsyncKeyState(i) & 0x8000)
                return i;
        }

        return 0;
    }

    inline bool AnyBindableKeyDown()
    {
        for (int key = 0x01; key <= 0xFE; ++key) {
            if (GetAsyncKeyState(key) & 0x8000) {
                return true;
            }
        }
        return false;
    }

    inline bool ConfiguredHotkeysReleased()
    {
        const int keys[] = {
            menuToggleKey,
            exitKey,
            triggerbotKey
        };
        for (int key : keys) {
            if (key > 0 && key <= 0xFF &&
                (GetAsyncKeyState(key) & 0x8000)) {
                return false;
            }
        }
        return true;
    }

    inline const char* FindHotkeyConflict(
        const int* target,
        int candidate)
    {
        struct Binding
        {
            const char* name;
            const int* key;
        };
        const Binding bindings[] = {
            { "Menu Toggle", &menuToggleKey },
            { "Exit Program", &exitKey },
            { "Triggerbot Key", &triggerbotKey }
        };
        for (const Binding& binding : bindings) {
            if (binding.key != target && *binding.key == candidate) {
                return binding.name;
            }
        }
        return nullptr;
    }

    inline void RenderHotkeyButton(const char* label, int* keyCode, const char* tooltip = nullptr)
    {
        const float dpiScale = sdl_renderer::getDpiScale();
        ImGui::Text("%s:", label);
        ImGui::SameLine(150.0f * dpiScale);

        char buttonLabel[64];
        if (isBindingKey && bindingKeyTarget == keyCode)
        {
            sprintf_s(buttonLabel, "[Press Key...]##%s", label);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.0f, 1.0f));
        }
        else
        {
            sprintf_s(buttonLabel, "%s##%s", GetKeyName(*keyCode), label);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.6f, 1.0f));
        }

        if (ImGui::Button(buttonLabel, ImVec2(100.0f * dpiScale, 0.0f)))
        {
            isBindingKey = true;
            bindingKeyTarget = keyCode;
            bindingKeyName = label;
            bindingWaitingForRelease = true;
            bindingError.clear();
            suppressHotkeysUntilRelease = true;
        }

        ImGui::PopStyleColor();

        if (tooltip && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tooltip);
    }

    inline void UpdateKeyBinding()
    {
        if (!isBindingKey || bindingKeyTarget == nullptr)
            return;

        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
        {
            isBindingKey = false;
            bindingKeyTarget = nullptr;
            bindingKeyName = nullptr;
            bindingWaitingForRelease = false;
            suppressHotkeysUntilRelease = true;
            return;
        }

        if (bindingWaitingForRelease) {
            if (!AnyBindableKeyDown()) {
                bindingWaitingForRelease = false;
            }
            return;
        }

        int pressedKey = GetPressedKey();
        if (pressedKey != 0)
        {
            if (const char* conflict =
                    FindHotkeyConflict(bindingKeyTarget, pressedKey)) {
                bindingError =
                    std::string("Already assigned to ") + conflict;
                bindingWaitingForRelease = true;
                return;
            }

            *bindingKeyTarget = pressedKey;
            isBindingKey = false;
            bindingKeyTarget = nullptr;
            bindingKeyName = nullptr;
            bindingWaitingForRelease = false;
            suppressHotkeysUntilRelease = true;
        }
    }

    inline void BeginCard(
        const char* id,
        const char* title,
        const char* subtitle,
        float height,
        float width = 0.0f)
    {
        const float dpiScale = sdl_renderer::getDpiScale();
        ImGui::PushStyleColor(
            ImGuiCol_ChildBg,
            ImVec4(0.070f, 0.090f, 0.125f, 1.0f));
        ImGui::PushStyleColor(
            ImGuiCol_Border,
            ImVec4(0.145f, 0.185f, 0.240f, 1.0f));
        ImGui::BeginChild(
            id,
            ImVec2(
                width > 0.0f ? width * dpiScale : 0.0f,
                height * dpiScale),
            true);
        ImGui::TextColored(
            ImVec4(0.330f, 0.800f, 1.000f, 1.0f),
            "%s",
            title);
        if (subtitle && subtitle[0] != '\0') {
            ImGui::TextColored(
                ImVec4(0.500f, 0.570f, 0.670f, 1.0f),
                "%s",
                subtitle);
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    inline void EndCard()
    {
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
    }

    inline void StatusValue(
        const char* label,
        bool enabled,
        const char* enabledText = "ON",
        const char* disabledText = "OFF")
    {
        ImGui::TextColored(
            ImVec4(0.610f, 0.665f, 0.750f, 1.0f),
            "%s",
            label);
        ImGui::SameLine();
        ImGui::TextColored(
            enabled
                ? ImVec4(0.250f, 0.900f, 0.600f, 1.0f)
                : ImVec4(0.930f, 0.420f, 0.430f, 1.0f),
            "%s",
            enabled ? enabledText : disabledText);
    }

    inline void RenderTriggerbotTab()
    {
        ImGui::Checkbox("Enable Triggerbot", &triggerbotEnabled);

        if (triggerbotEnabled)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::SliderInt("Delay (ms)", &triggerbotDelay, 0, 500, "%d ms");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Time before shooting after target enters crosshair");

            ImGui::Spacing();
            RenderHotkeyButton("Triggerbot Key", &triggerbotKey, "Hold to activate triggerbot");
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Hold %s to activate", GetKeyName(triggerbotKey));
        }
    }

    inline void RenderOverview()
    {
        const float dpiScale = sdl_renderer::getDpiScale();

        BeginCard(
            "##TriggerCard",
            "Triggerbot",
            "Auto-fire when crosshair is on an enemy.",
            200.0f);
        RenderTriggerbotTab();
        EndCard();

        ImGui::Spacing();

        BeginCard(
            "##SessionStatus",
            "Session status",
            "",
            150.0f);
        StatusValue(
            "Renderer",
            sdl_renderer::isAcceleratedRenderer(),
            "HARDWARE",
            "SOFTWARE");
        StatusValue(
            "Game focus",
            sdl_renderer::isGameForeground(),
            "ACTIVE",
            "PAUSED");
        StatusValue(
            "Memory writes",
            memory::WritesAllowed(),
            "UNLOCKED",
            "LOCKED");
        ImGui::Spacing();
        ImGui::Text(
            "%u x %u  |  %d Hz target",
            VIEWPORT_W,
            VIEWPORT_H,
            sdl_renderer::getTargetRefreshRate());
        EndCard();

        ImGui::Spacing();

        BeginCard(
            "##Hotkeys",
            "Hotkeys",
            "",
            120.0f);
        RenderHotkeyButton("Menu Toggle", &menuToggleKey);
        RenderHotkeyButton("Exit Program", &exitKey);
        EndCard();
    }

    inline void render()
    {
        UpdateKeyBinding();

        if (isBindingKey) {
            if (ConfiguredHotkeysReleased()) {
                suppressHotkeysUntilRelease = false;
            }
            return;
        }

        if (suppressHotkeysUntilRelease) {
            if (ConfiguredHotkeysReleased()) {
                suppressHotkeysUntilRelease = false;
                publishRuntimeConfig();
            }
        } else {
            publishRuntimeConfig();
        }

        if (!sdl_renderer::menuVisible) {
            return;
        }

        const float dpiScale = sdl_renderer::getDpiScale();
        const float menuWidth = 380.0f * dpiScale;
        const float menuHeight = 560.0f * dpiScale;
        const float margin = 12.0f * dpiScale;

        ImGui::SetNextWindowPos(
            ImVec2(margin, margin),
            ImGuiCond_Always);
        ImGui::SetNextWindowSize(
            ImVec2(menuWidth, menuHeight),
            ImGuiCond_Always);

        ImGui::Begin(
            "CS2 Triggerbot",
            nullptr,
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar);
        {
            const ImVec2 position = ImGui::GetWindowPos();
            const ImVec2 size = ImGui::GetWindowSize();
            sdl_renderer::setInteractiveRect(
                position.x,
                position.y,
                size.x,
                size.y);
        }

        ImGui::TextColored(
            ImVec4(0.330f, 0.800f, 1.000f, 1.0f),
            "CS2 TRIGGERBOT");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        RenderOverview();

        if (!bindingError.empty() && isBindingKey) {
            ImGui::TextColored(
                ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                "%s", bindingError.c_str());
        }

        ImGui::End();
    }
}
