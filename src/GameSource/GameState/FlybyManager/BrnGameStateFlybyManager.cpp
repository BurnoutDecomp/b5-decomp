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

// Forward-declare the FlybyData payload (full definition lives in the GameStateModuleIO TU,
// BrnGameStateSharedIO.h). FlybyManager only needs the address of its own mFlybyData member, so a
// forward declaration is sufficient here.
namespace GameStateModuleIO { struct FlybyData; }

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

    GameStateModuleIO::FlybyData* GetFlybyData();   // X360: returns &mFlybyData (this+0x10)

private:
    u32                            mVTable;                 // +0x00
    u8                             maUnknown_04[0x0C];      // +0x04  (X360-only; not in PS3 DWARF)
    u8                             mFlybyData[0x250];       // +0x10  GameStateModuleIO::FlybyData (4-byte header + 3x196-byte FlybyRivalData)
    CgsNumeric::Random             mRandom;                 // +0x260
    Pointer32<GameStateModule>     mpGameStateModule;       // +0x290
    Pointer32<ScoringSystem>       mpScoringSystem;         // +0x294
    s32                            miRankLeader;            // +0x298
    s32                            miRankOustider;          // +0x29C
};

static_assert(sizeof(Pointer32<GameStateModule>) == 4, "Pointer32 layout drift");
static_assert(sizeof(FlybyManager::RivalRating) == 12, "RivalRating layout drift");
static_assert(sizeof(FlybyManager) == 0x2A0, "FlybyManager layout drift");

void FlybyManager::Construct(GameStateModule* lpGameStateModule, ScoringSystem* lpScoringSystem)
{
    CGS_ASSERT(lpGameStateModule, "lpGameStateModule");

    mpGameStateModule.Set(lpGameStateModule);

    CGS_ASSERT(lpScoringSystem, "lpScoringSystem");

    mpScoringSystem.Set(lpScoringSystem);
    mRandom.Construct();
}

// X360. Accessor for the owning GameStateModule; asserts it was set by Construct.
GameStateModule* FlybyManager::GetGameStateModule()
{
    CGS_ASSERT(mpGameStateModule.Get(), "mpGameStateModule");
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
            CGS_ASSERT((lpRival0->mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID) && (lpRival1->mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID),
                       "( lpRival0->mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID ) && ( lpRival1->mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID )");
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

// Minimal slice of the owning module: only the trivial getter OnlineFlybyManager::GetLocalPlayerNetworkID
// reaches through. The full GameStateModule layout/members live in the BrnGameStateModule TU; here it is
// declared-only (the real getter returns mLocalPlayerNetworkID at +232296). FlybyManager above only needs
// the forward declaration (it holds a Pointer32<GameStateModule>), so this definition is introduced here
// purely so the inlined accessor below has a complete type to call through.
class GameStateModule
{
public:
    BrnNetwork::RoadRulesRecvData::NetworkPlayerID GetLocalPlayerNetworkID();
};

// BrnGameStateOnlineFlybyManager.h:45 -- OnlineFlybyManager : public FlybyManager. Minimal slice: only
// the one method owned by this TU is declared.
class OnlineFlybyManager : public FlybyManager
{
public:
    BrnNetwork::RoadRulesRecvData::NetworkPlayerID GetLocalPlayerNetworkID();
};

// X360 @ 0x82358720. Header-inlined accessor: returns the local player's network id by reaching through
// the owning GameStateModule. The X360 null check + verbatim assert ("mpGameStateModule",
// BrnGameStateFlybyManager.h:204) is exactly FlybyManager::GetGameStateModule()'s body, which the
// compiler inlined here; *(mpGameStateModule + 232296) is GameStateModule::mLocalPlayerNetworkID.
BrnNetwork::RoadRulesRecvData::NetworkPlayerID BrnGameState::OnlineFlybyManager::GetLocalPlayerNetworkID()
{
    return GetGameStateModule()->GetLocalPlayerNetworkID();
}

// X360 (DWARF BrnGameStateFlybyManager.h:217). Trivial accessor returning the address of the
// owned FlybyData member. On X360 mFlybyData sits at this+0x10 (proven via FlybyManager::Construct
// @0x823774C8 placing mRandom at this+0x260, and OnlineFlybyManager::CalculateOnlineRivals
// @0x823861C0 calling FlybyData::Prepare(this+0x10)). reinterpret_cast bridges the opaque
// fixed-size storage of mFlybyData (kept opaque to avoid pulling the full FlybyData definition
// into this standalone TU) to the typed accessor return -- it is address-of a real member, not a
// raw offset literal. Modelled NON-virtual: this file represents the X360 vtable manually as the
// u32 mVTable member (see static_assert(sizeof(FlybyManager)==0x2A0)); a real C++ `virtual` would
// inject an 8-byte compiler vptr on the x64 gate and shift mFlybyData off this+0x10.
GameStateModuleIO::FlybyData* FlybyManager::GetFlybyData()
{
    return reinterpret_cast<GameStateModuleIO::FlybyData*>(&mFlybyData);
}

// OfflineFlybyManager : public FlybyManager (DWARF BrnGameStateOfflineFlybyManager.h:42). Minimal
// slice: only the one method reconstructed by this TU. NON-virtual, matching the manual-mVTable,
// byte-exact convention of this file (the method is still the vtable entry the X360 dispatches).
class OfflineFlybyManager : public FlybyManager
{
public:
    // X360 @ 0x82364820. const FlybyData* CalculateFlybyRivals().
    const GameStateModuleIO::FlybyData* CalculateFlybyRivals();
};

// X360 @ 0x82364820  ==  addi r3,r3,0x10 ; blr.
// IDA-truncated symbol was 'OfflineFlybyManager::Cal'; full name recovered from DWARF mangled
// symbol _ZN12BrnGameState19OfflineFlybyManager20CalculateFlybyRivalsEv and confirmed by the
// identically-truncated sibling OnlineFlybyManager::Calc @0x82391FA8.
// The Offline override emits no rival-calculation call (that work was inlined into Prepare); the
// surviving vtable entry is the bare 'return &mFlybyData' == return GetFlybyData() (this+0x10).
const GameStateModuleIO::FlybyData* OfflineFlybyManager::CalculateFlybyRivals()
{
    return GetFlybyData();
}
}
