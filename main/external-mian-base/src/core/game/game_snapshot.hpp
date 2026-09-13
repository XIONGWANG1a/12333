#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace game
{
    struct WorldPosition
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    enum class Team : uint8_t
    {
        Unknown = 0,
        Spectator = 1,
        Terrorists = 2,
        CounterTerrorists = 3
    };

    enum class WeaponCategory : uint8_t
    {
        Unknown,
        Pistol,
        Rifle,
        SniperRifle,
        Smg,
        Shotgun,
        MachineGun,
        Grenade,
        Knife,
        Bomb,
        Equipment
    };

    struct WeaponSnapshot
    {
        uint16_t definitionIndex = 0;
        std::string name;
        std::string displayName;
        WeaponCategory category = WeaponCategory::Unknown;
        int clipAmmo = 0;
        int reserveAmmo = 0;
    };

    enum class MapPhase : uint8_t
    {
        Live,
        Warmup,
        FreezeTime
    };

    struct MapState
    {
        bool connected = false;
        std::string id;
        std::string displayName;
        MapPhase phase = MapPhase::Live;
        uint32_t roundNumber = 0;
    };

    struct PlayerSnapshot
    {
        uint64_t id = 0;
        uint64_t steamId = 0;
        std::string name;
        Team team = Team::Unknown;
        int competitiveColor = -1;
        bool alive = false;
        bool dormant = false;
        std::optional<WorldPosition> position;
        std::optional<float> yaw;
        int health = 0;
        int armor = 0;
        int money = 0;
        bool hasHelmet = false;
        bool hasDefuser = false;
        bool hasBomb = false;
        std::optional<WeaponSnapshot> activeWeapon;
        std::vector<WeaponSnapshot> inventory;
    };

    enum class BombState : uint8_t
    {
        None,
        Carried,
        Dropped,
        Planted,
        Exploded,
        Defused
    };

    enum class BombSite : uint8_t
    {
        A,
        B
    };

    struct BombInfo
    {
        BombState state = BombState::None;
        BombSite site = BombSite::A;
        std::optional<WorldPosition> position;
        std::optional<float> explodeInSeconds;
        std::optional<float> defuseInSeconds;
        bool beingDefused = false;
        bool defuseWillSucceed = false;
        std::optional<uint64_t> carrierPlayerId;
    };

    struct GameSnapshot
    {
        uint64_t sequence = 0;
        uint64_t capturedAtMs = 0;
        MapState map;
        std::optional<uint64_t> localPlayerId;
        std::optional<uint64_t> observedPlayerId;
        Team localTeam = Team::Unknown;
        std::vector<PlayerSnapshot> players;
        BombInfo bomb;
    };
}
