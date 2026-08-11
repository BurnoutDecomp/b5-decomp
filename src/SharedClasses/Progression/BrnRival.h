#ifndef BRN_RIVAL_H
#define BRN_RIVAL_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID

// =============================================================================
// BrnRival.h  (OWNING HEADER for BrnProgression::Rival)
//
// DWARF home: SharedClasses/Progression/BrnRival.h. A single rival (named opponent) record
// in the progression resource. This is a MINIMAL OWNING SLICE: the full DWARF data layout
// (so ProgressionData's rival lookups land mId at byte +0 and the 56-byte stride lines up by
// name) plus the GetId accessor those lookups read. The remaining attested API
// (Construct/SetName/GetCarId/...) is declared-only; bodies land with the Rival TU.
// Single owner -- grow here, do not fork.
//
// LAYOUT is X360-faithful (DWARF BrnRival.h:46). 56 bytes (CgsID is 8-byte aligned, so the
// two CgsID fields lead and the byte/short fields and 32-byte name follow).
// =============================================================================

// GetDistrict returns BrnWorld::EDistrict; only declared here, so an opaque forward is enough.
namespace BrnWorld { enum EDistrict : s32; }

namespace BrnProgression
{

struct Rival
{
    // DWARF BrnRival.h:110. Length of the embedded name buffer.
    static const s32 KI_NAME_LENGTH = 32;

    // X360 reads mId out of this record in the ProgressionData rival lookups.
    CgsID GetId() const { return mId; }

    // ---- ADDITIVE (SetupParRivals wave, 2026-08-11) --------------------------------------
    // BODIED (it was in the declared-only list below, and defined nowhere in the tree -- the
    // same latent-unresolved-external state Road::GetId/GetRoadLimitId0 were in). It has no
    // standalone X360 symbol; the console folds it into its callers, so a header inline IS the
    // faithful shape (same treatment as GetId above).
    //
    // ⭐ ASM ATTESTATION -- StreetManager::FindRivalsByDistrict @0x82336360, at 0x82336400:
    //     lbz   r10, 0x14(r11)      ; a ONE-BYTE load of miDistrictIndex (console +0x14)
    //     extsb r10, r10            ; SIGN-extended to 32 bits ...
    //     cmpw  cr6, r10, r26       ; ... and compared SIGNED against the wanted district
    // The `lbz`+`extsb` pair is what pins the member to the s8 miDistrictIndex rather than to
    // any wider neighbour, and the sign extension is why the cast below is from s8 (not u8):
    // a negative district index stays negative. Read by name, so the x64 layout carries it.
    BrnWorld::EDistrict GetDistrict() const
    {
        return static_cast<BrnWorld::EDistrict>(miDistrictIndex);
    }

    // ---- Remaining X360-attested API (bodies in the Rival TU; declaration-only here) ----
    void           Construct(CgsID lId, CgsID lCarId, s16 liPersonalityIndex, s16 liPursuitTarget, s8 liDistrictIndex);
    void           Construct(CgsID lId);
    void           SetName(const char* lpcName);
    CgsID          GetCarId() const;
    s16            GetPursuitTarget() const;
    const char*    GetName() const;
    s16            GetPersonalityIndex() const;
    s8             GetUnlockRank() const;
    void           SetUnlockRank(s8 liUnlockRank);
    u8             GetNumMedalsToUnlock() const;
    void           SetNumMedalsToUnlock(u8 luNumMedals);
    void           IsUsedForRankUpGiftCar(bool lbIsUsed);
    bool           GetIsUsedForRankUpGiftCar() const;
    void           FixDown();
    void           FixUp();

private:
    CgsID mId;                          // 0x00 (DWARF BrnRival.h:112)
    CgsID mCarId;                       // 0x08 (DWARF :113)
    s16   miPersonalityIndex;           // 0x10 (DWARF :114)
    s16   miPursuitTarget;              // 0x12 (DWARF :115)
    s8    miDistrictIndex;              // 0x14 (DWARF :116)
    s8    miUnlockRank;                 // 0x15 (DWARF :117)
    u8    mu8NumMedalsToUnlock;         // 0x16 (DWARF :118)
    bool  mbIsUsedForRankUpGiftCar;     // 0x17 (DWARF :119)
    char  macName[KI_NAME_LENGTH];      // 0x18 (DWARF :121)
};

}

#endif // BRN_RIVAL_H
