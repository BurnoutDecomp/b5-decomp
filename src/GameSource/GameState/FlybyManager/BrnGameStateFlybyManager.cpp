#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"

template <typename T>
struct Pointer32
{
    u32 muAddress;

    void Set(T* lpPointer)
    {
        muAddress = static_cast<u32>(reinterpret_cast<usize>(lpPointer));
    }

    T* Get() const
    {
        return reinterpret_cast<T*>(static_cast<usize>(muAddress));
    }
};

namespace BrnNetwork
{
struct RoadRulesRecvData
{
    typedef s32 NetworkPlayerID;
};
}

namespace CgsNetwork
{
static const BrnNetwork::RoadRulesRecvData::NetworkPlayerID K_INVALID_PLAYER_ID = -1;
}

namespace CgsNumeric
{
namespace
{
const u32 KU_FLOAT_BUFFER_SIZE = 8;
const u32 KU_IEEE_754_REPRESENTATION_FLOAT_ONE = 0x3F800000;
const u64 KU_RANDOM_DEFAULT_SEED = 0xC87CD8C91AD0891Bull;
const u64 KU_RANDOM_MULTIPLIER = 0x4C957F2D5851F42Dull;
}

class alignas(8) Random
{
public:
    void Construct()
    {
        muSeed = KU_RANDOM_DEFAULT_SEED;
        muOldestBufferIndex = 0;
        mauIntegerBuffer[0] = KU_IEEE_754_REPRESENTATION_FLOAT_ONE;

        for (u32 luIndex = 1; luIndex < KU_FLOAT_BUFFER_SIZE; ++luIndex)
            AddRandomFloatToBuffer();

        muOldestBufferIndex = (muOldestBufferIndex + 1) & (KU_FLOAT_BUFFER_SIZE - 1);
    }

private:
    union
    {
        f32 mafFloatBuffer[KU_FLOAT_BUFFER_SIZE];
        u32 mauIntegerBuffer[KU_FLOAT_BUFFER_SIZE];
    };
    u64 muSeed;
    u32 muOldestBufferIndex;

    static u32 ConvertUnsignedFixed32ToFloatRepresentation(u32 lu32Random)
    {
        return KU_IEEE_754_REPRESENTATION_FLOAT_ONE | (lu32Random >> 9);
    }

    void AddRandomFloatToBuffer()
    {
        const u32 luRandomFloatBits = ConvertUnsignedFixed32ToFloatRepresentation(static_cast<u32>(muSeed >> 32));
        muSeed = muSeed * KU_RANDOM_MULTIPLIER + 1;
        muOldestBufferIndex = (muOldestBufferIndex + 1) & (KU_FLOAT_BUFFER_SIZE - 1);
        mauIntegerBuffer[muOldestBufferIndex] = luRandomFloatBits;
    }
};

static_assert(sizeof(Random) == 0x30, "Random layout drift");
}

namespace BrnGameState
{
class GameStateModule;
class ScoringSystem;

class FlybyManager
{
public:
    void Construct(GameStateModule* lpGameStateModule, ScoringSystem* lpScoringSystem);
    GameStateModule* GetGameStateModule();   // X360 (asserts non-null, returns mpGameStateModule)

    struct RivalRating
    {
        bool mbHasValidRelationship;
        u8   mPad1[3];
        BrnNetwork::RoadRulesRecvData::NetworkPlayerID mPlayerID;
        s32  miRivalWeighting;

        static s32 SortRivalsCallback(const void* lpData0, const void* lpData1);
    };

private:
    u32                            mVTable;
    u8                             mFlybyData[0x25C];
    CgsNumeric::Random             mRandom;
    Pointer32<GameStateModule>     mpGameStateModule;
    Pointer32<ScoringSystem>       mpScoringSystem;
    s32                            miRankLeader;
    s32                            miRankOustider;
};

static_assert(sizeof(Pointer32<GameStateModule>) == 4, "Pointer32 layout drift");
static_assert(sizeof(FlybyManager::RivalRating) == 12, "RivalRating layout drift");
static_assert(sizeof(FlybyManager) == 0x2A0, "FlybyManager layout drift");

void FlybyManager::Construct(GameStateModule* lpGameStateModule, ScoringSystem* lpScoringSystem)
{
    if (!lpGameStateModule)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpGameStateModule",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/FlybyManager/BrnGameStateFlybyManager.cpp",
            123);
        CgsDev::Assert::EndAssert();
    }

    mpGameStateModule.Set(lpGameStateModule);

    if (!lpScoringSystem)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpScoringSystem",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/FlybyManager/BrnGameStateFlybyManager.cpp",
            126);
        CgsDev::Assert::EndAssert();
    }

    mpScoringSystem.Set(lpScoringSystem);
    mRandom.Construct();
}

// X360. Accessor for the owning GameStateModule; asserts it was set by Construct.
GameStateModule* FlybyManager::GetGameStateModule()
{
    if (!mpGameStateModule.Get())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "mpGameStateModule",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\flybymanager\\BrnGameStateFlybyManager.h",
            204);
        CgsDev::Assert::EndAssert();
    }
    return mpGameStateModule.Get();
}

s32 FlybyManager::RivalRating::SortRivalsCallback(const void* lpData0, const void* lpData1)
{
    const RivalRating* lpRival0 = static_cast<const RivalRating*>(lpData0);
    const RivalRating* lpRival1 = static_cast<const RivalRating*>(lpData1);

    if (lpRival0->mPlayerID == lpRival1->mPlayerID)
    {
        if (lpRival0->mPlayerID != CgsNetwork::K_INVALID_PLAYER_ID
            || lpRival1->mPlayerID != CgsNetwork::K_INVALID_PLAYER_ID)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "( lpRival0->mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID ) && ( lpRival1->mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID )",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/FlybyManager/BrnGameStateFlybyManager.cpp",
                83);
            CgsDev::Assert::EndAssert();
        }

        return 0;
    }

    if (lpRival0->mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID)
        return 1;

    if (lpRival1->mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID)
        return -1;

    if (lpRival0->miRivalWeighting < lpRival1->miRivalWeighting)
        return 1;

    if (lpRival0->miRivalWeighting > lpRival1->miRivalWeighting)
        return -1;

    return 0;
}
}
