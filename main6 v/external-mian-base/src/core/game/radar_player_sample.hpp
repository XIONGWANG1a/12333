#pragma once

#include <cstdint>
#include <optional>
#include "game_snapshot.hpp"

namespace game
{
    namespace radar_player_sample
    {
        inline constexpr uint64_t kLastKnownMaximumAgeMs = 5000;

        struct PositionRead
        {
            bool succeeded = false;
            WorldPosition value{};
        };

        struct PlayerSampleIdentity
        {
            uint64_t playerId = 0;
            uintptr_t controllerAddress = 0;
            uintptr_t pawnAddress = 0;
            uint32_t pawnHandle = 0;
            uint8_t team = 0;
            uint64_t steamId = 0;
            bool steamIdKnown = false;
        };

        struct PlayerLastKnownState
        {
            PlayerSampleIdentity identity{};
            uint64_t lastSampleMs = 0;
            std::optional<WorldPosition> position;
            std::optional<float> yaw;
            bool wasAlive = false;
            bool wasDormant = false;
        };

        struct RetainedPlayerSample
        {
            std::optional<WorldPosition> position;
            std::optional<float> yaw;
        };

        inline std::optional<WorldPosition> choosePosition(
            const PositionRead& absolute,
            const PositionRead& oldOrigin)
        {
            if (absolute.succeeded) {
                return absolute.value;
            }
            if (oldOrigin.succeeded) {
                return oldOrigin.value;
            }
            return std::nullopt;
        }

        inline RetainedPlayerSample retainPlayerSample(
            PlayerLastKnownState& state,
            const PlayerSampleIdentity& identity,
            uint64_t nowMs,
            bool alive,
            bool dormant,
            std::optional<WorldPosition> position,
            std::optional<float> yaw)
        {
            RetainedPlayerSample result;

            const bool identityChanged =
                state.identity.playerId != identity.playerId ||
                state.identity.pawnHandle != identity.pawnHandle ||
                state.identity.team != identity.team;

            if (identityChanged ||
                state.lastSampleMs == 0 ||
                nowMs - state.lastSampleMs > kLastKnownMaximumAgeMs) {
                state.identity = identity;
                state.lastSampleMs = nowMs;
                state.position = position;
                state.yaw = yaw;
                state.wasAlive = alive;
                state.wasDormant = dormant;
                result.position = position;
                result.yaw = yaw;
                return result;
            }

            state.lastSampleMs = nowMs;
            state.wasAlive = alive;
            state.wasDormant = dormant;

            if (position) {
                state.position = position;
                result.position = position;
            } else {
                result.position = state.position;
            }

            if (yaw) {
                state.yaw = yaw;
                result.yaw = yaw;
            } else {
                result.yaw = state.yaw;
            }

            return result;
        }
    }
}
