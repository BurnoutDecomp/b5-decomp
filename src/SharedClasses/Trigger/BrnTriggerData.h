#ifndef BRN_TRIGGER_DATA_H
#define BRN_TRIGGER_DATA_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 (rw::math::vpu::Vector3), CgsID
#include <cstdint>            // uintptr_t (relocation delta)
#include <cstddef>            // offsetof (host layout static_asserts)
#include "SharedClasses/Trigger/BrnKillzone.h"   // complete BrnTrigger::Killzone (GetKillzone 16B stride)

// Owning header for BrnTrigger::TriggerData — the track-trigger resource payload.
//
// SCOPE (minimal): this header exists so BrnGameState::OfflineGameMode::SelectRandomDestinations
// (X360 0x82321E38, GameState/.../BrnOfflineGameMode.cpp) can walk a track's landmark list by
// name instead of raw offsets. SelectRandomDestinations' body needs exactly three accessors:
//   - GetLandmarkCount() / GetNumLandmarks()  -> the loop bound (X360 offset 0x34 == 52)
//   - GetLandmark(int) const                  -> &mpLandmarks[i] (X360 offset 0x30 == 48; 52-byte stride)
// so only those (plus the X360-attested GetLandmarkFromRegionIndex, whose home this header is)
// are declared here. The remaining TriggerData accessors enumerated in the DecFIGS DWARF
// (signature stunts, killzones, blackspots, generic/VFX regions, roaming/spawn locations, the
// region table, FixUp/FixDown, etc.) are deliberately omitted to keep this minimal; add them
// only when a reconstructed caller actually uses them.
//
// LAYOUT (X360-faithful, proven against the SelectRandomDestinations pseudocode/asm):
//   lwz r11, 0x30(this)  -> mpLandmarks       (offset 48)
//   lwz r11, 0x34(this)  -> miLandmarkCount   (offset 52)
//   addi r29,r29,0x34    -> sizeof(Landmark) stride == 52
// The preceding members are taken from the DecFIGS DWARF (BrnTriggerData.h):
//   miVersionNumber @0, muSize @4, then two 16-byte SIMD Vector3 (start pos/dir) @16/@32,
//   which lands mpLandmarks at 48 and miLandmarkCount at 52 — exactly the observed offsets.
//
// GATING: the X360 ledger (progress/identity.json) attests TriggerData methods FindLandmark,
// FixUp, FixDown, GetKillzone, GetOnlineLandmark, GetRegion and GetLandmarkFromRegionIndex as
// real (own) functions; the landmark count/index accessors used by SelectRandomDestinations are
// NOT separate symbols (they were inlined into the caller), so they are correctly modeled as
// inline accessors. Only GetLandmarkFromRegionIndex is forward-declared here (its body lives in
// the BrnTriggerData TU); everything OfflineGameMode needs is inline below.
//
// FixUp/FixDown CONTRACT: the PS3 DWARF declares these as `void FixUp(MemoryResource)`. The X360
// ARTIST binary (0x8267FC08 / 0x8267F800) takes the load-base DELTA in a register and returns it,
// and TriggerResourceType::FixUp @0x826800D8 (`mr r3,r4; lwz r4,0(r5); b ...`) supplies the
// rw::Resource's segment-0 base.
//
// ⚠️ WIDENED 2026-07-29 (lane-data port). These were modelled as `int FixUp(int)`, which is only
// correct while pointers are 32 bits: on the x64 host `static_cast<int>(GetLoadBase(...))` throws
// away the top half of the allocation address, so every rebased pointer would land in the wrong
// 4 GB — bug class (a). The delta is now uintptr_t end to end, fed by GetLoadBase64. The
// serialised slots themselves are 64 bits wide too (tools/assets/bundles/lane_transcode.py).

namespace BrnTrigger
{
// Forward declaration: GetLandmark returns a Landmark* and the count accessors return ints, so
// the complete Landmark layout is not needed at this declaration site. The owning header
// (SharedClasses/Trigger/BrnLandmark.h) is #included by the .cpp that dereferences the result.
struct Landmark;          // committed: SharedClasses/Trigger/BrnLandmark.h
struct TriggerRegion;     // committed: SharedClasses/Trigger/BrnTriggerBase.h
// Remaining region-table element types are not yet committed; forward-declared so the extended
// struct's declared-only accessors compile (their bodies live in their own future TUs).
struct SignatureStunt;
struct GenericRegion;
struct Blackspot;
struct VFXBoxRegion;
struct RoamingLocation;
struct SpawnLocation;

struct TriggerData
{
    static const int32_t KI_VERSION_NUMBER = 34;   // DWARF BrnTriggerData.h:60
    static const int32_t KI_ALIGNMENT      = 16;   // DWARF BrnTriggerData.h:233

    // --- Landmark accessors used by OfflineGameMode::SelectRandomDestinations ----------------

    // Number of landmarks in this track's trigger data (X360 offset 0x34). GetNumLandmarks is
    // the spelling the SelectRandomDestinations body uses for its loop bound; GetLandmarkCount
    // is the DWARF-attested name. Both alias the same member.
    int GetLandmarkCount() const { return miLandmarkCount; }
    int GetNumLandmarks()  const { return miLandmarkCount; }
    int GetOnlineLandmarkCount() const { return miOnlineLandmarkCount; }

    // The liIndex'th landmark (const). Declaration only -- the body (&mpLandmarks[liIndex]) needs
    // the complete Landmark layout and is provided by a sibling TU.
    const Landmark* GetLandmark(int liIndex) const;
          Landmark* GetLandmark(int liIndex);

    // X360 0x824EAA00. Returns the liLandmarkIndex'th ONLINE landmark. Body in the BrnTriggerData TU.
    const Landmark* GetOnlineLandmark(int liLandmarkIndex) const;

    // --- Region table accessors owned by the BrnTriggerData TU -------------------------------
    int GetRegionCount() const { return miRegionCount; }

    // X360 0x8230E490. The liRegionIndex'th entry of the region table. Body in the BrnTriggerData TU.
    const TriggerRegion* GetRegion(int liRegionIndex) const;

    // X360 0x8231B648. Maps a region-table index back to its owning Landmark (asserts the type).
    const Landmark* GetLandmarkFromRegionIndex(int liRegionIndex) const;

    // DWARF BrnTriggerData.h:93/96 (`Vector3 GetPlayerStartPosition() const`, ...Direction).
    // The track's authored start pose. Inlined on the X360: GameStateModule::
    // SendSetupPlayerCarEvent @0x8239A918 opens with two GetMemory() calls and
    // `lvx128 v127, r11, 0x10` / `lvx128 v0, r3, 0x20` -- these two members -- and hands the
    // POSITION straight to FindNearestJunkyardID. De-inlined here to the DWARF's own names.
    Vector3 GetPlayerStartPosition()  const { return mPlayerStartPosition; }
    Vector3 GetPlayerStartDirection() const { return mPlayerStartDirection; }

    // --- Killzone accessor owned by the BrnTriggerData TU ------------------------------------
    int GetKillzoneCount() const { return miKillzoneCount; }

    // X360 0x82354820. &mpKillzones[liKillzoneIndex]. Body in the BrnTriggerData TU.
    const Killzone* GetKillzone(int liKillzoneIndex) const;

    // Remaining DWARF-attested accessors (declared-only; bodies in their own TUs / not yet
    // reconstructed). Kept so the one coherent struct matches the DWARF interface.
    const SignatureStunt*  GetSignatureStunt(int liIndex) const;
    int  GetSignatureStuntCount() const { return miSignatureStuntCount; }
    // BODIED (BrnTriggerData.cpp): &mpGenericRegions[liIndex]. Like every other &mp<Table>[i]
    // accessor on this struct it has no standalone X360 symbol (inlined at the call site --
    // GameStateModule::FindNearestJunkyardID @0x8236BAC8 walks it with the stride 0x38 that
    // sizeof(GenericRegion) gives, and carries the range assert inline).
    const GenericRegion*   GetGenericRegion(int liIndex) const;
    int  GetGenericRegionCount() const { return miGenericRegionCount; }
    const Blackspot*       GetBlackspot(int liIndex) const;
    int  GetBlackspotCount() const { return miBlackspotCount; }
    const RoamingLocation* GetRoamingLocation(int liIndex) const;
    int  GetRoamingLocationCount() const { return miRoamingLocationCount; }
    const SpawnLocation*   GetSpawnLocation(int liIndex) const;
    int  GetSpawnLocationCount() const { return miSpawnLocationCount; }

    // X360 0x82675738. Linear scan of the landmark table for one whose GetId() == lId. Returns the
    // matching Landmark, or the base pointer mpLandmarks (NOT null) on miss/empty. Body in the TU.
    const Landmark*        FindLandmark(CgsID lId) const;
    const Landmark*        FindLandmarkByDesignIndex(uint8_t luDesignIndex) const;
    uint32_t GetSize() const { return muSize; }

    // X360 0x8267F800 / 0x8267FC08. Load-time pointer relocation. luDelta is the load base; FixUp
    // ADDS it (offset -> absolute address), FixDown SUBTRACTS it (absolute -> offset). See the
    // widening note in the header comment above.
    uintptr_t FixDown(uintptr_t luDelta);
    uintptr_t FixUp(uintptr_t luDelta);

private:
    // Full X360 layout (DWARF order). Trailing members past miLandmarkCount were absent from the
    // former minimal slice and are added here in offset order (same struct, extended, NOT forked).
    int32_t          miVersionNumber;        // 0x00
    u32              muSize;                  // 0x04
    // 8 bytes implicit padding to the 16-byte Vector3 alignment boundary (0x08..0x0F)
    Vector3          mPlayerStartPosition;    // 0x10 (16-byte SIMD)
    Vector3          mPlayerStartDirection;   // 0x20 (16-byte SIMD)
    Landmark*        mpLandmarks;             // 0x30
    int32_t          miLandmarkCount;         // 0x34
    int32_t          miOnlineLandmarkCount;   // 0x38
    SignatureStunt*  mpSignatureStunts;       // 0x3C
    int32_t          miSignatureStuntCount;   // 0x40
    GenericRegion*   mpGenericRegions;        // 0x44
    int32_t          miGenericRegionCount;    // 0x48
    Killzone*        mpKillzones;             // 0x4C
    int32_t          miKillzoneCount;         // 0x50
    Blackspot*       mpBlackspots;            // 0x54
    int32_t          miBlackspotCount;        // 0x58
    VFXBoxRegion*    mpVFXBoxRegions;         // 0x5C
    int32_t          miVFXBoxRegionCount;     // 0x60
    RoamingLocation* mpRoamingLocations;      // 0x64
    int32_t          miRoamingLocationCount;  // 0x68
    SpawnLocation*   mpSpawnLocations;        // 0x6C
    int32_t          miSpawnLocationCount;    // 0x70
    TriggerRegion**  mppRegions;              // 0x74
    int32_t          miRegionCount;           // 0x78

    // ---- host layout contract with tools/assets/bundles/lane_transcode.py -------------
    // The offsets in the member comments above are the CONSOLE (4-byte pointer) ones; on
    // the host every pointer widens and the counts that follow shift with it. These pins
    // are the other end of the transcoder's CONSOLE_PIN table for TriggerData. Declared
    // in-class because the members are private (the project's _AssertIoLayout idiom).
    static void _AssertLayout()
    {
        static_assert(offsetof(TriggerData, mPlayerStartPosition) == 0x10, "TriggerData::mPlayerStartPosition");
        static_assert(offsetof(TriggerData, mpLandmarks)          == 0x30, "TriggerData::mpLandmarks");
        static_assert(offsetof(TriggerData, miLandmarkCount)      == 0x38, "TriggerData::miLandmarkCount");
        static_assert(offsetof(TriggerData, mpSignatureStunts)    == 0x40, "TriggerData::mpSignatureStunts");
        static_assert(offsetof(TriggerData, mpKillzones)          == 0x60, "TriggerData::mpKillzones");
        static_assert(offsetof(TriggerData, mppRegions)           == 0xB0, "TriggerData::mppRegions");
        static_assert(offsetof(TriggerData, miRegionCount)        == 0xB8, "TriggerData::miRegionCount");
        static_assert(sizeof(TriggerData) == 0xC0, "TriggerData host sizeof");
    }
};

}

#endif
