// FX-RCEM4 (crash-parity 2026-09-24): the car-colour chain -- CHAIN-RECOLOUR + the G61-D6 colour leg.
// run_fxrcem4_recolour.py extracts VERBATIM from the shipped sources and replays against fixtures:
//   Attrib::Gen::burnoutcargraphicsasset::PlayerColourIndex / PlayerColourPaletteIndex (DWARF :83/:90;
//       layout words +4 / +0, inlined at 0x822EB4D8 / 0x822EB4E0)
//   ActiveRaceCar::OnResourcesLoaded @0x822EB168 -- its default-colour leg 0x822EB474..0x822EB4F0
//   RaceCarEntityModule::SetupCarColour @0x822F5170
//   RaceCarEntityModule::IsCarColourInUse @0x822D2E68
//   RaceCarEntityModule::GetRandomCarColour @0x822EA088 (Range TU; table dword_820148D8)
//   the taken-down block of ProcessRaceCarCrashCompleteEvents 0x822F40C8..0x822F4390 with its two
//       re-colour legs (0x822F4174 / 0x822F4298)
// A piece the pre-fix source lacks is replayed as an empty stub, so the RED side reports per-check
// failures instead of failing to build.
#include "types.hpp"
#include "GameSource/BurnoutConstants.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

static unsigned guAssertions = 0;
static std::vector<std::string> gaAsserts;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) {
    ++guAssertions; gaAsserts.push_back(lpcMessage ? lpcMessage : "");
    std::fprintf(stderr, "ASSERT: %s\n", lpcMessage); return 0;
}
void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

namespace BrnGameState { struct GameModeParams {
    static const u64 KU_FLAG_AI_PERSISTENT_DAMAGE  = 0x40000000ull;    // BrnGameModeParams.h:172
    static const u64 KU_FLAG_SET_OPPONENTS_TO_COPS = 0x400000000ull;   // :176 (1 << 34)
}; }

namespace Fixture {

struct Vector3 { f32 x, y, z, w; };
struct Vector4 { f32 x, y, z, w; };
enum { E_NUM_PALETTES = 4 };
const u32 KU_RACECAR_MODULE_DIAG_MAX_LINES = 128u;
enum ERaceCarType { E_RACE_CAR_TYPE_PLAYER = 0, E_RACE_CAR_TYPE_AI = 1, E_RACE_CAR_TYPE_NETWORK = 2,
                    E_RACE_CAR_TYPE_INACTIVE = 3, E_RACE_CAR_TYPE_COUNT = 4 };

// ---- AttribSys stand-ins: a car asset key -> its graphics collection -> the 8-byte layout ----------
namespace Attrib {
struct Collection { s32 maLayout[2]; };            // word 0 = PlayerColourPaletteIndex, word 1 = PlayerColourIndex
struct CarAssetEntry { u64 muKey; Collection* mpGraphics; };
static std::vector<CarAssetEntry> gaCarAssets;
static s32 gaiDefaultDataArea[2] = { 0, 0 };        // Attrib::DefaultDataArea(8u): zeroed
static int giCarAssetConstructs = 0, giGraphicsConstructs = 0;
namespace Gen {
struct burnoutcarasset {
    struct RefSpec { Collection* mpCollection; const Collection* GetCollection() { return mpCollection; } };
    RefSpec mGraphicsRef;
    burnoutcarasset(u64 luKey, void*) {
        ++giCarAssetConstructs; mGraphicsRef.mpCollection = nullptr;
        for (const CarAssetEntry& lrEntry : gaCarAssets) if (lrEntry.muKey == luKey) mGraphicsRef.mpCollection = lrEntry.mpGraphics;
    }
    RefSpec* GetGraphicsAssetRefSpec() const { return const_cast<RefSpec*>(&mGraphicsRef); }
};
struct burnoutcargraphicsasset {
    const void* mpLayout;
    burnoutcargraphicsasset(Collection* lpCollection, void*) {
        ++giGraphicsConstructs; mpLayout = lpCollection ? static_cast<const void*>(lpCollection->maLayout) : gaiDefaultDataArea;
    }
    const void* GetLayoutPointer() const { return mpLayout; }
    const s32& PlayerColourIndex() const;
    const s32& PlayerColourPaletteIndex() const;
};
#include "fxrcem4_gfx_accessors.inc"
}
}

namespace CgsResource { struct ResourceHandle { u32 muId; }; }

struct DetachedPartQueue { int miConstructs = 0; void Construct() { ++miConstructs; } };
struct RenderParams {
    Vector4 mPaintColour = { 1.0f, 1.0f, 1.0f, 1.0f };
    DetachedPartQueue mQueue;
    DetachedPartQueue& GetDetachedPartQueue() { return mQueue; }
    const Vector4& GetPaintColour() const { return mPaintColour; }
};

class RaceCar {
public:
    s32 miColourIndex = -1, miColourPalette = -1;
    f32 mfPersistentDamage = 0.0f;
    u8  muType = E_RACE_CAR_TYPE_AI;
    s32  GetColourIndex() const { return miColourIndex; }
    s32  GetColourPalette() const { return miColourPalette; }
    void SetColourIndex(s32 li) { miColourIndex = li; }
    void SetColourPalette(s32 li) { miColourPalette = li; }
    ERaceCarType GetType() const { return static_cast<ERaceCarType>(muType); }
    f32  GetPersistentDamage() const { return mfPersistentDamage; }
    bool IncreasePersistentDamage();
};
#include "fxrcem4_increase.inc"

struct ActiveRaceCar {
    enum EState : u32 { E_STATE_INACTIVE = 0, E_STATE_ATTACHED = 1, E_STATE_WAITING = 2, E_STATE_ACTIVE = 3 };
    u32  muState = E_STATE_ATTACHED;
    bool mbAttached = true, mbTakenDown = false;
    RaceCar* mpRaceCar = nullptr;
    RenderParams mRenderParams;
    CgsResource::ResourceHandle mDeformationModelHandle = { 0u }, mGraphicsModelHandle = { 0u };
    s32  miDefaultColourIndex = -1, miDefaultColourPalette = -1;
    int  miVerletResets = 0;
    bool IsAttached() const { return mbAttached; }
    bool IsActive() const { return muState == E_STATE_ACTIVE; }
    bool IsTakenDown() const { return mbTakenDown; }
    RaceCar* GetGlobalRaceCar() { CGS_ASSERT(IsAttached(), "IsAttached()"); return mpRaceCar; }
    void ResetVerletOffsets() { ++miVerletResets; }
    RenderParams* GetRenderParams() { return &mRenderParams; }
    s32 GetDefaultColourIndex() { return miDefaultColourIndex; }
    s32 GetDefaultPaletteIndex() { return miDefaultColourPalette; }
    void OnResourcesLoaded(const CgsResource::ResourceHandle& lrDeformationModelHandle,
                           const CgsResource::ResourceHandle& lrGraphicsModelHandle,
                           const Vector3& lrInitialVelocity, u64 luCarAssetAttribKey);
};
#include "fxrcem4_on_resources_loaded.inc"

struct PlayerCarColourPalette {
    std::vector<Vector4> maPaint;
    s32 miNumColours = 0;
    const Vector4* GetPaintColours() const { return maPaint.data(); }
    s32 GetNumColours() const { return miNumColours; }
};
struct GlobalColourPalette { PlayerCarColourPalette maPalettes[E_NUM_PALETTES]; };
struct ColourResource {
    GlobalColourPalette mPalette;
    GlobalColourPalette* operator->() { return &mPalette; }
};

// The module RNG GetRandomCarColour draws from (the Range TU's GetModuleRandom): scripted draws.
struct ScriptedRandom {
    std::vector<u32> maDraws; size_t muNext = 0;
    u32 RandomUInt() { return muNext < maDraws.size() ? maDraws[muNext++] : 0u; }
};
static ScriptedRandom gRandom;
ScriptedRandom& GetModuleRandom() { return gRandom; }

struct RaceCarEntityModule {
    ActiveRaceCar       maActiveRaceCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    RaceCar             maRaceCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
    bool                mbIsInGameMode = true;
    u64                 mxGameModeFlags = 0;
    s32                 miPersistentDamageCarCount = 0;
    ColourResource      mCarColoursResource;
    RaceCarEntityModule() {
        for (int i = 0; i < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++i) { maActiveRaceCars[i].mpRaceCar = &maRaceCars[i]; maActiveRaceCars[i].mbAttached = false; }
        maRaceCars[0].muType = E_RACE_CAR_TYPE_PLAYER;
    }
    ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex leIndex) {
        CGS_ASSERT(leIndex >= 0 && leIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "active index");
        return &maActiveRaceCars[leIndex];
    }
    bool GetGameModeFlag(u64 lxMask) const { return (mxGameModeFlags & lxMask) != 0; }
    s32  GetPersistentDamageCarCount() const { return miPersistentDamageCarCount; }
    void SetupCarColour(EActiveRaceCarIndex leActiveRaceCarIndex);
    bool IsCarColourInUse(s32 liPaletteIndex, s32 liColourIndex);
    s32  GetRandomCarColour(s32 liPaletteIndex, s32 liColourIndex);
    void TakenDownBlock(ActiveRaceCar* lpActiveRaceCar, u32 luActiveRaceCarIndex);
};
#include "fxrcem4_module.inc"

void RaceCarEntityModule::TakenDownBlock(ActiveRaceCar* lpActiveRaceCar, u32 luActiveRaceCarIndex)
{
    (void)luActiveRaceCarIndex;
#include "fxrcem4_block.inc"
}

}   // namespace Fixture

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

// Palettes 0/2/3: 20 colours; palette 1: 10. Colour i paints (i*0.05, 0.5, 1 - i*0.05).
static void FillPalettes(Fixture::RaceCarEntityModule& lrModule) {
    for (int p = 0; p < Fixture::E_NUM_PALETTES; ++p) {
        Fixture::PlayerCarColourPalette& lrPalette = lrModule.mCarColoursResource.mPalette.maPalettes[p];
        lrPalette.miNumColours = (p == 1) ? 10 : 20;
        lrPalette.maPaint.clear();
        for (int c = 0; c < 20; ++c)   // storage for 20 either way: the out-of-range cases read in bounds
            lrPalette.maPaint.push_back({ c * 0.05f, 0.5f, 1.0f - c * 0.05f, 1.0f });
    }
}
static void Attach(Fixture::RaceCarEntityModule& lrModule, int liSlot, s32 liPalette, s32 liColour,
                   Fixture::Vector4 lPaint = { 9.0f, 9.0f, 9.0f, 1.0f }) {
    lrModule.maActiveRaceCars[liSlot].mbAttached = true;
    lrModule.maRaceCars[liSlot].miColourPalette = liPalette;
    lrModule.maRaceCars[liSlot].miColourIndex = liColour;
    lrModule.maActiveRaceCars[liSlot].mRenderParams.mPaintColour = lPaint;
}

int main() {
    using namespace Fixture;

    // ---- the two DWARF accessors (layout words +4 / +0) ------------------------------------------
    {
        Attrib::Collection lGfx = { { 2, 13 } };
        Attrib::Gen::burnoutcargraphicsasset lAsset(&lGfx, nullptr);
        Check(lAsset.PlayerColourIndex() == 13, "PlayerColourIndex() == layout word +4 (lwz r10, 4(r11) @0x822EB4D8)");
        Check(lAsset.PlayerColourPaletteIndex() == 2, "PlayerColourPaletteIndex() == layout word +0 (lwz r11, 0(r11) @0x822EB4E0)");
    }

    // ---- G61-D6 colour leg: ActiveRaceCar::OnResourcesLoaded ---------------------------------------
    {
        Attrib::Collection lGfx = { { 1, 7 } };
        Attrib::gaCarAssets.push_back({ 0xABCDEF0123456789ull, &lGfx });
        Attrib::giCarAssetConstructs = Attrib::giGraphicsConstructs = 0;
        ActiveRaceCar lCar;
        const Vector3 lZero = { 0, 0, 0, 0 };
        lCar.OnResourcesLoaded({ 5u }, { 6u }, lZero, 0xABCDEF0123456789ull);
        Check(lCar.muState == ActiveRaceCar::E_STATE_WAITING && lCar.mDeformationModelHandle.muId == 5u
                  && lCar.mGraphicsModelHandle.muId == 6u && lCar.miVerletResets == 1 && lCar.mRenderParams.mQueue.miConstructs == 1,
              "OnResourcesLoaded: state WAITING, both handles, ResetVerletOffsets, queue Construct (unchanged head)");
        Check(lCar.miDefaultColourIndex == 7, "G61-D6: miDefaultColourIndex (+0x1C80) = the car asset's graphics PlayerColourIndex");
        Check(lCar.miDefaultColourPalette == 1, "G61-D6: miDefaultColourPalette (+0x1C84) = PlayerColourPaletteIndex");
        Check(Attrib::giCarAssetConstructs == 1 && Attrib::giGraphicsConstructs == 1,
              "G61-D6: one burnoutcarasset(key) and one burnoutcargraphicsasset(RefSpec @+0x170) per load");
        ActiveRaceCar lUnknown;
        lUnknown.OnResourcesLoaded({ 1u }, { 2u }, lZero, 0x1111ull);
        Check(lUnknown.miDefaultColourIndex == 0 && lUnknown.miDefaultColourPalette == 0,
              "G61-D6: an asset with no graphics collection reads the zeroed default data area -> 0 / 0");
    }

    // ---- IsCarColourInUse ---------------------------------------------------------------------------
    {
        RaceCarEntityModule lModule; FillPalettes(lModule);
        Check(!lModule.IsCarColourInUse(0, 5), "IsCarColourInUse: no attached car -> false");
        Attach(lModule, 2, 1, 5);                                  // colour 5 of ANOTHER palette
        Check(lModule.IsCarColourInUse(0, 5), "IsCarColourInUse: index match on any palette -> true (the palette is not compared)");
        RaceCarEntityModule lPaint; FillPalettes(lPaint);
        const Vector4 lCol5 = lPaint.mCarColoursResource.mPalette.maPalettes[0].maPaint[5];
        Attach(lPaint, 3, 0, 11, { lCol5.x + 0.1f, lCol5.y + 0.05f, lCol5.z + 0.06f, 1.0f });   // L1 0.21
        Check(lPaint.IsCarColourInUse(0, 5), "IsCarColourInUse: paint within L1 0.21 < 0.22 (flt_82014900) -> true");
        RaceCarEntityModule lFar; FillPalettes(lFar);
        Attach(lFar, 3, 0, 11, { lCol5.x + 0.1f, lCol5.y + 0.05f, lCol5.z + 0.08f, 1.0f });     // L1 0.23
        Check(!lFar.IsCarColourInUse(0, 5), "IsCarColourInUse: L1 0.23 >= 0.22 -> false");
        RaceCarEntityModule lDetached; FillPalettes(lDetached);
        Attach(lDetached, 4, 0, 5); lDetached.maActiveRaceCars[4].mbAttached = false;
        Check(!lDetached.IsCarColourInUse(0, 5), "IsCarColourInUse: a detached slot is skipped");
        RaceCarEntityModule lNan; FillPalettes(lNan);
        const f32 lfNan = std::numeric_limits<f32>::quiet_NaN();
        Attach(lNan, 1, 0, 11, { lfNan, lfNan, lfNan, 1.0f });
        Check(!lNan.IsCarColourInUse(0, 5), "IsCarColourInUse: an unordered distance is not < 0.22 (fcmpu ; blt)");
        const unsigned luBefore = guAssertions;
        RaceCarEntityModule lBad; FillPalettes(lBad);
        lBad.IsCarColourInUse(1, 12);
        Check(guAssertions == luBefore + 1, "IsCarColourInUse: colour >= miNumColours trips :9743 once");
        guAssertions = luBefore;
    }

    // ---- GetRandomCarColour (table {5,6,9,12,14,15,16,18} @0x820148D8) -----------------------------
    {
        RaceCarEntityModule lModule; FillPalettes(lModule);
        gRandom = ScriptedRandom(); gRandom.maDraws = { 0x00000003u };
        Check(lModule.GetRandomCarColour(0, -1) == 12 && gRandom.muNext == 1,
              "GetRandomCarColour(p, -1): ONE draw, index = draw & 7 (3 -> 12)");
        gRandom = ScriptedRandom(); gRandom.maDraws = { 0xFFFFFFF9u };                           // & 7 == 1
        Check(lModule.GetRandomCarColour(0, -1) == 6, "GetRandomCarColour: index = draw & 7 (0x..F9 -> 1 -> 6)");

        RaceCarEntityModule lBusy; FillPalettes(lBusy);
        Attach(lBusy, 1, 0, 12); Attach(lBusy, 2, 0, 14);          // entries 3 and 4 in use
        gRandom = ScriptedRandom(); gRandom.maDraws = { 3u };
        Check(lBusy.GetRandomCarColour(0, -1) == 15, "GetRandomCarColour: walks past colours in use (12, 14 -> 15)");
        Attach(lBusy, 3, 0, 18); Attach(lBusy, 4, 0, 5);
        gRandom = ScriptedRandom(); gRandom.maDraws = { 7u };
        Check(lBusy.GetRandomCarColour(0, -1) == 6, "GetRandomCarColour: wraps (index + 1) % 8 past the end (18, 5 -> 6)");
        RaceCarEntityModule lFull; FillPalettes(lFull);
        const s32 kaColours[8] = { 5, 6, 9, 12, 14, 15, 16, 18 };
        for (int i = 0; i < 8; ++i) Attach(lFull, i, 0, kaColours[i]);
        gRandom = ScriptedRandom(); gRandom.maDraws = { 2u };
        Check(lFull.GetRandomCarColour(0, -1) == 9, "GetRandomCarColour: all eight in use -> the starting entry after 8 tries");

        RaceCarEntityModule lKeep; FillPalettes(lKeep);
        gRandom = ScriptedRandom(); gRandom.maDraws = { 1u, 4u };
        Check(lKeep.GetRandomCarColour(0, 3) == 3 && gRandom.muNext == 1,
              "GetRandomCarColour(p, free c): an odd first draw keeps c (one draw)");
        gRandom = ScriptedRandom(); gRandom.maDraws = { 2u, 4u };
        Check(lKeep.GetRandomCarColour(0, 3) == 14 && gRandom.muNext == 2,
              "GetRandomCarColour(p, free c): an even first draw -> the index comes from the SECOND draw (4 -> 14)");
        Attach(lKeep, 5, 0, 3);
        gRandom = ScriptedRandom(); gRandom.maDraws = { 1u, 4u };
        Check(lKeep.GetRandomCarColour(0, 3) == 6 && gRandom.muNext == 1,
              "GetRandomCarColour(p, c in use): no coin flip -- the first draw is the index (1 -> 6)");
    }

    // ---- SetupCarColour ----------------------------------------------------------------------------
    {
        // the player: the default pair, straight
        RaceCarEntityModule lModule; FillPalettes(lModule);
        lModule.maActiveRaceCars[0].mbAttached = true;
        lModule.maActiveRaceCars[0].miDefaultColourIndex = 13; lModule.maActiveRaceCars[0].miDefaultColourPalette = 2;
        lModule.SetupCarColour(E_ACTIVE_RACE_CAR_INDEX_0);
        Check(lModule.maRaceCars[0].miColourIndex == 13 && lModule.maRaceCars[0].miColourPalette == 2,
              "SetupCarColour: the player takes its default colour + palette (+0x1C80/+0x1C84 -> +0x94/+0x98)");

        // a colour already set is kept
        RaceCarEntityModule lSet; FillPalettes(lSet);
        lSet.maActiveRaceCars[3].mbAttached = true; lSet.maRaceCars[3].miColourIndex = 4; lSet.maRaceCars[3].miColourPalette = 1;
        lSet.maActiveRaceCars[3].miDefaultColourIndex = 9; lSet.maActiveRaceCars[3].miDefaultColourPalette = 0;
        lSet.SetupCarColour(E_ACTIVE_RACE_CAR_INDEX_3);
        Check(lSet.maRaceCars[3].miColourIndex == 4 && lSet.maRaceCars[3].miColourPalette == 1,
              "SetupCarColour: miColourIndex != -1 -> nothing (bne @0x822F51C8)");

        // a rival whose default colour is free: the default
        RaceCarEntityModule lRival; FillPalettes(lRival);
        lRival.maActiveRaceCars[3].mbAttached = true;
        lRival.maActiveRaceCars[3].miDefaultColourIndex = 9; lRival.maActiveRaceCars[3].miDefaultColourPalette = 1;
        lRival.SetupCarColour(E_ACTIVE_RACE_CAR_INDEX_3);
        Check(lRival.maRaceCars[3].miColourIndex == 9 && lRival.maRaceCars[3].miColourPalette == 1,
              "SetupCarColour: a rival with a free default colour keeps it");

        // a rival whose default colour is taken, in a game mode: a random free one
        RaceCarEntityModule lTaken; FillPalettes(lTaken);
        Attach(lTaken, 1, 0, 9);
        lTaken.maActiveRaceCars[3].mbAttached = true;
        lTaken.maActiveRaceCars[3].miDefaultColourIndex = 9; lTaken.maActiveRaceCars[3].miDefaultColourPalette = 0;
        gRandom = ScriptedRandom(); gRandom.maDraws = { 2u };                                     // -> 9 in use -> 12
        lTaken.SetupCarColour(E_ACTIVE_RACE_CAR_INDEX_3);
        Check(lTaken.maRaceCars[3].miColourIndex == 12 && lTaken.maRaceCars[3].miColourPalette == 0,
              "SetupCarColour: default taken (IsCarColourInUse) in a game mode -> GetRandomCarColour(palette, -1)");

        // same, outside a game mode: the default regardless
        RaceCarEntityModule lFree; FillPalettes(lFree);
        Attach(lFree, 1, 0, 9); lFree.mbIsInGameMode = false;
        lFree.maActiveRaceCars[3].mbAttached = true;
        lFree.maActiveRaceCars[3].miDefaultColourIndex = 9; lFree.maActiveRaceCars[3].miDefaultColourPalette = 0;
        gRandom = ScriptedRandom(); gRandom.maDraws = { 2u };
        lFree.SetupCarColour(E_ACTIVE_RACE_CAR_INDEX_3);
        Check(lFree.maRaceCars[3].miColourIndex == 9 && gRandom.muNext == 0,
              "SetupCarColour: not in a game mode (+0x18344 == 0) -> the default even when taken, no draw");

        // SET_OPPONENTS_TO_COPS: black (6), in both arms
        RaceCarEntityModule lCops; FillPalettes(lCops); lCops.mxGameModeFlags = BrnGameState::GameModeParams::KU_FLAG_SET_OPPONENTS_TO_COPS;
        lCops.maActiveRaceCars[3].mbAttached = true;
        lCops.maActiveRaceCars[3].miDefaultColourIndex = 9; lCops.maActiveRaceCars[3].miDefaultColourPalette = 0;
        lCops.SetupCarColour(E_ACTIVE_RACE_CAR_INDEX_3);
        Attach(lCops, 1, 0, 9);
        lCops.maActiveRaceCars[4].mbAttached = true;
        lCops.maActiveRaceCars[4].miDefaultColourIndex = 9; lCops.maActiveRaceCars[4].miDefaultColourPalette = 0;
        lCops.SetupCarColour(E_ACTIVE_RACE_CAR_INDEX_4);
        Check(lCops.maRaceCars[3].miColourIndex == 6 && lCops.maRaceCars[4].miColourIndex == 6,
              "SetupCarColour: KU_FLAG_SET_OPPONENTS_TO_COPS (1<<34) -> KI_BLACK_CAR_COLOUR_INDEX 6, free or taken");

        const unsigned luBefore = guAssertions;
        RaceCarEntityModule lBad; FillPalettes(lBad); lBad.mbIsInGameMode = false;
        lBad.maActiveRaceCars[3].mbAttached = true;
        lBad.maActiveRaceCars[3].miDefaultColourIndex = 15; lBad.maActiveRaceCars[3].miDefaultColourPalette = 1;
        lBad.SetupCarColour(E_ACTIVE_RACE_CAR_INDEX_3);
        Check(guAssertions == luBefore + 1, "SetupCarColour: default colour >= miNumColours trips \"Invalid Colour Index: \" (:2297)");
        guAssertions = luBefore;
    }

    // ---- the taken-down re-colour legs of ProcessRaceCarCrashCompleteEvents -----------------------
    {
        RaceCarEntityModule lModule; FillPalettes(lModule);
        lModule.mxGameModeFlags = BrnGameState::GameModeParams::KU_FLAG_AI_PERSISTENT_DAMAGE;
        Attach(lModule, 2, 0, 11);
        ActiveRaceCar& lrSlot = lModule.maActiveRaceCars[2]; lrSlot.mbTakenDown = true;
        RaceCar& lrCar = lModule.maRaceCars[2];
        gRandom = ScriptedRandom(); gRandom.maDraws = { 1u };
        for (int i = 0; i < 3; ++i) lModule.TakenDownBlock(&lrSlot, 2u);
        Check(std::fabs(lrCar.mfPersistentDamage - 0.9f) < 1e-6f && lrCar.miColourIndex == 11 && gRandom.muNext == 0,
              "re-colour: three takedowns accrue 0.9 and keep the colour (no draw)");
        lModule.TakenDownBlock(&lrSlot, 2u);
        Check(lrCar.mfPersistentDamage == 0.0f && lrCar.miColourIndex == 6 && lrCar.miColourPalette == 0,
              "re-colour 0x822F4298: the wrap past 1.0 re-colours -- GetRandomCarColour(palette, -1) (draw 1 -> 6)");

        RaceCarEntityModule lCapped; FillPalettes(lCapped);
        lCapped.mxGameModeFlags = BrnGameState::GameModeParams::KU_FLAG_AI_PERSISTENT_DAMAGE;
        lCapped.miPersistentDamageCarCount = 3;
        Attach(lCapped, 4, 0, 11); lCapped.maActiveRaceCars[4].mbTakenDown = true;
        gRandom = ScriptedRandom(); gRandom.maDraws = { 5u };
        lCapped.TakenDownBlock(&lCapped.maActiveRaceCars[4], 4u);
        Check(lCapped.maRaceCars[4].mfPersistentDamage == 0.0f && lCapped.maRaceCars[4].miColourIndex == 15,
              "re-colour 0x822F4174: clean rival with three damaged rivals already -> no damage, re-coloured (draw 5 -> 15)");

        RaceCarEntityModule lCops; FillPalettes(lCops);
        lCops.mxGameModeFlags = BrnGameState::GameModeParams::KU_FLAG_AI_PERSISTENT_DAMAGE
                              | BrnGameState::GameModeParams::KU_FLAG_SET_OPPONENTS_TO_COPS;
        lCops.miPersistentDamageCarCount = 3;
        Attach(lCops, 4, 0, 11); lCops.maActiveRaceCars[4].mbTakenDown = true;
        gRandom = ScriptedRandom(); gRandom.maDraws = { 5u };
        lCops.TakenDownBlock(&lCops.maActiveRaceCars[4], 4u);
        Check(lCops.maRaceCars[4].miColourIndex == 6 && gRandom.muNext == 0,
              "re-colour: SET_OPPONENTS_TO_COPS -> 6 (r17), no draw");

        RaceCarEntityModule lPlayer; FillPalettes(lPlayer);
        lPlayer.mxGameModeFlags = BrnGameState::GameModeParams::KU_FLAG_AI_PERSISTENT_DAMAGE;
        lPlayer.miPersistentDamageCarCount = 3;
        Attach(lPlayer, 0, 0, 11); lPlayer.maActiveRaceCars[0].mbTakenDown = true;
        lPlayer.TakenDownBlock(&lPlayer.maActiveRaceCars[0], 0u);
        Check(lPlayer.maRaceCars[0].miColourIndex == 11, "re-colour: never the player (lbz 0xA4 == 1 gate)");
    }

    Check(guAssertions == 0, "valid fixtures fire no stray assertions");
    std::printf("FxRcem4Recolour: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
