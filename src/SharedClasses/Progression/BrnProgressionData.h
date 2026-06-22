#ifndef BRN_PROGRESSION_DATA_H
#define BRN_PROGRESSION_DATA_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID

// =============================================================================
// BrnProgressionData.h  (OWNING HEADER for BrnProgression::ProgressionData)
//
// The track/career progression resource payload. This header is the real home of
// BrnProgression::ProgressionData; it supersedes the 2-method stub that formerly
// lived inline in SharedClasses/Progression/BrnProgressionResourceType.h (which now
// #includes this header and drops its local stub). The FixUp/FixDown(int) decls
// below preserve that caller contract verbatim.
//
// LAYOUT is X360-faithful and taken from the DecFIGS DWARF for this exact path
// (references/DecFIGS/dwarfdump/.../BrnProgressionData.h:249-277). The struct is a
// flat list of {pointer, count} pairs (4 bytes each on the PPC 32-bit ABI), so the
// DWARF source order lands every member at its console byte offset. This is a
// MINIMAL SLICE: only the three {pointer,count} pairs the TU's three accessors
// dereference are named (mpaProgressionRanks/muProgressionRankCount @0x10,
// mpaAIBalances/muAIBalanceCount @0x30, mpaTrophyUnlocks/muTrophyUnlockCount @0x40).
// The untouched DWARF members are represented by named explicit padding so the
// touched members keep their exact offsets WITHOUT pulling in the not-yet-committed
// element types. Grow in place: when a caller needs one of the padded slots, replace
// that pad with the real DWARF member -- do not fork a second definition.
//
// X360-vs-DWARF: the PS3 DWARF declares FixUp/FixDown as `void FixUp(MemoryResource)`.
// The committed X360 ProgressionResourceType.cpp forwards a load-base DELTA and treats
// them as `int FixUp(int)/int FixDown(int)`; the X360 binary is authoritative, so those
// signatures are kept here (declared-only; bodies are a separate TU).
// =============================================================================

namespace BrnProgression
{
// Per-rank tuning record (112-byte stride; proven by GetProgressionRankData's
// `112 * luIndex`). Real layout home: SharedClasses/Progression/BrnProgressionRankData.
// GetProgressionRankData only returns a pointer, so an incomplete type suffices here.
class ProgressionRankData;

// Per-trophy unlock record (16-byte stride; proven by GetTrophyUnlock's `16 * luIndex`).
// Real home: SharedClasses/Progression/BrnTrophyUnlockData.h. GetTrophyUnlock only returns
// a pointer, so an incomplete forward declaration is enough.
struct TrophyUnlockData;

// Per-rival AI balance graph (68-byte stride; proven by GetInterpolatedAIBalanceGraph's
// `68 * luIndex`). Full layout is required (returned BY VALUE and element-wise interpolated),
// so it lives in the sibling owning header BrnRaceBalance.h.
struct OpponentBalanceData;

// The remaining array element types. Held only by pointer in the layout below, so an
// incomplete forward declaration is enough here; the lookup/relocation TU that needs a
// complete one includes the element's owning header (BrnRival.h, BrnOpponentData.h,
// BrnRaceEventData.h). Real homes:
//   Rival                 -> SharedClasses/Progression/BrnRival.h
//   CarOpponentSet        -> SharedClasses/Progression/BrnOpponentData.h
//   EventJunction         -> SharedClasses/Progression/BrnRaceEventData.h
//   RaceEventData         -> SharedClasses/Progression/BrnRaceEventData.h
//   EventRacerPersonality -> SharedClasses/Progression/BrnRaceEventData.h
struct Rival;
struct CarOpponentSet;
struct EventJunction;
struct RaceEventData;
struct EventRacerPersonality;

struct ProgressionData
{
    // ---- Accessors reconstructed by this TU (X360 standalone symbols) ----------------------

    // X360 0x82311790. Returns &mpaProgressionRanks[luIndex] (asserts the bound).
    const ProgressionRankData* GetProgressionRankData(u32 luIndex) const;

    // X360 0x823569F0. Returns &mpaTrophyUnlocks[luIndex] (asserts the bound).
    TrophyUnlockData* GetTrophyUnlock(u32 luIndex) const;

    // X360 0x82676820. Blends the ahead/behind graphs of two AI-balance entries by lfBlend and
    // returns the result by value (the catch-up cut-off ratio is left zeroed).
    OpponentBalanceData GetInterpolatedAIBalanceGraph(s32 liIndexA, s32 liIndexB, f32 lfBlend) const;

    // X360 0x82676B18. Finds the CarOpponentSet for a given player car id whose rank is the
    // closest match <= the player's race rank (exact rank wins immediately).
    CarOpponentSet* FindCarOpponentSet(CgsID lCarModelId, s32 liPlayerRaceRank) const;

    // X360 0x82676AC8. Returns the rival whose id matches lRivalId, or null if none.
    const Rival* FindRival(CgsID lRivalId) const;

    // X360 0x82676A90. Returns the index of the rival whose id matches lRivalId, or the rival
    // count if none matches.
    s32 FindRivalIndexFromId(CgsID lRivalId) const;

    // ---- Other X360-attested methods this header owns (bodies are separate TUs) ------------
    // FixUp/FixDown keep the int-delta contract the committed ProgressionResourceType.cpp uses
    // (X360-authoritative over the PS3 DWARF's `void FixUp(MemoryResource)`).
    int FixUp(int liDelta);
    int FixDown(int liDelta);

private:
    // Full DWARF-faithful layout (BrnProgressionData.h:249-277): a flat list of {pointer, count}
    // pairs. The padding the minimal slice used has been replaced in place by the real named
    // members at their console offsets (additive growth -- offsets/sizeof preserved). NOTE: the
    // pointer members are 32-bit on the console and 64-bit on the host, so the byte offsets in the
    // comments are the X360 offsets and are NOT asserted across the pointer members on the gate.
    u32                    muVersionNumber;         // 0x00  (DWARF :249)
    u32                    muSize;                  // 0x04  (DWARF :250)
    CgsID*                 mpaPlayerCarIds;         // 0x08  (DWARF :252)
    u32                    muPlayerCarIdCount;      // 0x0C  (DWARF :253)

    ProgressionRankData*   mpaProgressionRanks;     // 0x10  (DWARF :255)
    u32                    muProgressionRankCount;  // 0x14  (DWARF :256)

    EventJunction*         mpaEventJunctions;       // 0x18  (DWARF :258)
    u32                    muEventJunctionCount;    // 0x1C  (DWARF :259)

    RaceEventData*         mpaEvents;               // 0x20  (DWARF :261)
    u32                    muEventCount;            // 0x24  (DWARF :262)

    Rival*                 mpaRivals;               // 0x28  (DWARF :264)
    s32                    miRivalCount;            // 0x2C  (DWARF :265)

    OpponentBalanceData*   mpaAIBalances;           // 0x30  (DWARF :267)
    u32                    muAIBalanceCount;        // 0x34  (DWARF :268)

    EventRacerPersonality* mpaPersonalities;        // 0x38  (DWARF :270)
    u32                    muPersonalityCount;      // 0x3C  (DWARF :271)

    TrophyUnlockData*      mpaTrophyUnlocks;        // 0x40  (DWARF :273)
    u32                    muTrophyUnlockCount;     // 0x44  (DWARF :274)

    CarOpponentSet*        mpaCarOpponentSet;       // 0x48  (DWARF :276)
    u32                    muCarOpponentsCount;     // 0x4C  (DWARF :277)
};
}

#endif // BRN_PROGRESSION_DATA_H
