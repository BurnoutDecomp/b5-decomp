#ifndef BRN_TRIGGER_DATA_H
#define BRN_TRIGGER_DATA_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 (rw::math::vpu::Vector3), CgsID

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

namespace BrnTrigger
{
// Forward declaration: GetLandmark returns a Landmark* and the count accessors return ints, so
// the complete Landmark layout is not needed at this declaration site. The owning header
// (SharedClasses/Trigger/BrnLandmark.h) is #included by the .cpp that dereferences the result.
struct Landmark;

struct TriggerData
{
    // --- Accessors used by OfflineGameMode::SelectRandomDestinations -------------------------

    // Number of landmarks in this track's trigger data (X360 offset 0x34). GetNumLandmarks is
    // the spelling the SelectRandomDestinations body uses for its loop bound; GetLandmarkCount
    // is the DWARF-attested name. Both alias the same member.
    int GetLandmarkCount() const { return miLandmarkCount; }
    int GetNumLandmarks()  const { return miLandmarkCount; }

    // The liIndex'th landmark (const). DWARF: `const Landmark* GetLandmark(int32_t) const`.
    // Declaration only — the body (&mpLandmarks[liIndex]) needs the complete Landmark layout and
    // is provided by the BrnTriggerData TU; a declaration is all the cl /c gate needs for callers
    // that include this header.
    const Landmark* GetLandmark(int liIndex) const;

    // --- Other X360-attested member this header owns (body in the BrnTriggerData TU) ----------
    // X360 0x8231B648. Maps a region-table index back to its owning Landmark.
    const Landmark* GetLandmarkFromRegionIndex(int liRegionIndex) const;

private:
    // Layout to the last field OfflineGameMode reads. Members beyond miLandmarkCount exist in the
    // DWARF (online landmark count, signature stunts, regions, ...) but are out of this minimal
    // header's scope; if a caller needs them, extend this struct in offset order rather than
    // forking a second definition.
    int     miVersionNumber;        // 0x00
    u32     muSize;                 // 0x04
    // 8 bytes implicit padding to the 16-byte Vector3 alignment boundary (0x08..0x0F)
    Vector3 mPlayerStartPosition;   // 0x10 (16-byte SIMD)
    Vector3 mPlayerStartDirection;  // 0x20 (16-byte SIMD)
    Landmark* mpLandmarks;          // 0x30
    int     miLandmarkCount;        // 0x34
};
}

#endif
