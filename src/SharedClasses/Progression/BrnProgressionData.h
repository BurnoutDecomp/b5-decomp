#ifndef BRN_PROGRESSION_DATA_H
#define BRN_PROGRESSION_DATA_H

#include "types.hpp"

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

    // ---- Other X360-attested methods this header owns (bodies are separate TUs) ------------
    // FixUp/FixDown keep the int-delta contract the committed ProgressionResourceType.cpp uses
    // (X360-authoritative over the PS3 DWARF's `void FixUp(MemoryResource)`).
    int FixUp(int liDelta);
    int FixDown(int liDelta);

private:
    // 0x00 muVersionNumber + 0x04 muSize + 0x08 mpaPlayerCarIds + 0x0C muPlayerCarIdCount.
    u8 maPadHeader[0x10];          // 0x00..0x0F

    ProgressionRankData* mpaProgressionRanks;     // 0x10  (DWARF BrnProgressionData.h:255)
    u32                  muProgressionRankCount;  // 0x14  (DWARF :256)

    // 0x18 mpaEventJunctions + count + 0x20 mpaEvents + count + 0x28 mpaRivals + count.
    u8 maPadEventsRivals[0x18];    // 0x18..0x2F

    OpponentBalanceData* mpaAIBalances;           // 0x30  (DWARF :267)
    u32                  muAIBalanceCount;        // 0x34  (DWARF :268)

    // 0x38 mpaPersonalities + 0x3C muPersonalityCount.
    u8 maPadPersonalities[0x08];   // 0x38..0x3F

    TrophyUnlockData*    mpaTrophyUnlocks;        // 0x40  (DWARF :273)
    u32                  muTrophyUnlockCount;     // 0x44  (DWARF :274)

    // 0x48 mpaCarOpponentSet + 0x4C muCarOpponentsCount. Padded so sizeof == 0x50.
    u8 maPadCarOpponents[0x08];    // 0x48..0x4F
};
}

#endif // BRN_PROGRESSION_DATA_H
