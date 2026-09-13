#pragma once

#include <optional>
#include <vector>
#include <string>
#include <memory>
#include <mutex>

#include "core/memory/memory.hpp"
#include "core/game/game_snapshot.hpp"
#include "client_dll.hpp"
#include "offsets.hpp"
#include "utils/math/vector.hpp"

extern uint32_t WIDTH;
extern uint32_t HEIGHT;
extern uint32_t WINDOW_W;
extern uint32_t WINDOW_H;

struct viewMatrix
{
    float m[16];
};

namespace BoneIndex
{
    constexpr int PELVIS = 0;
    constexpr int SPINE_2 = 2;
    constexpr int NECK = 5;
    constexpr int HEAD = 6;
    constexpr int LEFT_SHOULDER = 8;
    constexpr int LEFT_ELBOW = 9;
    constexpr int LEFT_HAND = 11;
    constexpr int RIGHT_SHOULDER = 13;
    constexpr int RIGHT_ELBOW = 14;
    constexpr int RIGHT_HAND = 15;
    constexpr int LEFT_HIP = 17;
    constexpr int LEFT_KNEE = 18;
    constexpr int LEFT_FOOT = 19;
    constexpr int RIGHT_HIP = 20;
    constexpr int RIGHT_KNEE = 21;
    constexpr int RIGHT_FOOT = 22;
    constexpr int BONE_COUNT = 28;
}

struct EnemyInfo
{
    uintptr_t pawnAddress = 0;
    uint32_t entityIndex = 0;
    vec3 position{};
    vec3 headPosition{};
    int32_t health = 0;
    float distance = 0.0f;
    std::string weaponName;
    float viewYaw = 0.0f;
    float angleToPlayer = 180.0f;
    float flashDuration = 0.0f;
    bool viewAngleKnown = false;
    bool isFlashed = false;
    bool isSpotted = false;
    bool visibilityKnown = false;
    vec3 bonePositions[BoneIndex::BONE_COUNT]{};
    bool hasBones = false;
};

struct LocalPlayerCache
{
    uintptr_t pawn = 0;
    vec3 position{};
    vec3 eyePosition{};
    vec2 viewAngle{};
    int32_t crosshairEntityIndex = -1;
    uint8_t team = 0;
    bool isValid = false;
};

struct CachedPawn
{
    uintptr_t controllerAddress = 0;
    uintptr_t pawnAddress = 0;
    uint32_t pawnHandle = 0;
    uint32_t entityIndex = 0;
    uint8_t team = 0;
    uint64_t playerId = 0;
    uint64_t steamId = 0;
    bool steamIdKnown = false;
    std::string playerName;
    int competitiveColor = -1;
    int armor = 0;
    int money = 0;
    bool alive = false;
    bool isLocal = false;
    bool hasHelmet = false;
    bool hasDefuser = false;
    bool hasBomb = false;
    std::string weaponName;
    std::optional<game::WeaponSnapshot> activeWeapon;
    std::vector<game::WeaponSnapshot> inventory;
    float flashDuration = 0.0f;
    bool isFlashed = false;
};

namespace menu
{
    struct RuntimeConfig;
}

struct BombInfo
{
    bool isPlanted = false;
    bool isDefusing = false;
    bool hasExploded = false;
    bool isDefused = false;
    float blowTime = 0.0f;
    float defuseCountDown = 0.0f;
    float curtime = 0.0f;
    int bombSite = 0;
    uint64_t sampledAtMilliseconds = 0;
    vec3 position{};
    bool positionKnown = false;
};

struct WorldEntityInfo
{
    vec3 position;
    int type;
    std::string name;
    float distance;
};

namespace esp
{
    using EnemySnapshot =
        std::shared_ptr<const std::vector<EnemyInfo>>;
    using WorldEntitySnapshot =
        std::shared_ptr<const std::vector<WorldEntityInfo>>;
    using GameSnapshot = std::shared_ptr<const game::GameSnapshot>;

    inline EnemySnapshot enemies =
        std::make_shared<const std::vector<EnemyInfo>>();
    inline WorldEntitySnapshot worldEntities =
        std::make_shared<const std::vector<WorldEntityInfo>>();
    inline GameSnapshot gameSnapshot =
        std::make_shared<const game::GameSnapshot>();
    inline viewMatrix vm = {};
    inline vec3 player_position{};
    inline float player_yaw = 0.0f;
    inline uintptr_t pID;
    inline uintptr_t modBase;

    inline std::mutex dataMutex;

    inline std::vector<CachedPawn> cachedPawns;

    inline LocalPlayerCache localPlayer;

    inline BombInfo bombInfo;

    bool init();
    void updateEntities(const menu::RuntimeConfig& config);
    void refreshEntityCache(const menu::RuntimeConfig& config);
    void clearRuntimeState();
    EnemySnapshot getEnemySnapshot();
    GameSnapshot getGameSnapshot();
    bool w2s(const vec3& world, vec2& screen, float m[16]);
    double player_distance(const vec3& a, const vec3& b);
    float normalizeAngle(float angle);
    float calculateYawToTarget(const vec3& from, const vec3& to);
    float calculateAngleToPlayer(float enemyYaw, const vec3& enemyPos, const vec3& playerPos);
}
