#ifndef BRN_PROGRESSION_DATA_H
#define BRN_PROGRESSION_DATA_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID

#include <cstdint>            // uintptr_t (the serialised 32-bit array-base slots below)

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

    // ADDITIVE GROW: the rank-table length at +0x14. The X360 reads it inline --
    // ProgressionManager::GetProgressionRank @0x823701D8 clamps the player's cached rank with
    // `*(progressionData + 20)`, with no call. Defined inline here (same precedent as
    // GetRivalCount below); no layout change.
    u32 GetProgressionRankCount() const { return muProgressionRankCount; }

    // X360 0x823569F0. Returns &mpaTrophyUnlocks[luIndex] (asserts the bound).
    TrophyUnlockData* GetTrophyUnlock(u32 luIndex) const;

    // The trophy table's length. The X360 has no standalone symbol -- ProgressionManager::
    // OnTrophyUnlock @0x82389740 inlines the read as `lwz r14, 0x44(r3)` right after the
    // operator-> -- so it is defined inline here, the same precedent as GetProgressionRankCount
    // and GetRivalCount. No layout change.
    u32 GetTrophyUnlockCount() const { return muTrophyUnlockCount; }

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

    // ADDITIVE GROW (StreetManager wave-C keystone; DWARF BrnProgressionData.h:111/:114/:117).
    // By-index rival iteration for StreetManager::FindRivalsByDistrict / SetupParRivals
    // (X360 0x82336360 / 0x8233F560 read mpaRivals/miRivalCount through these header-inlines).
    // GetRival owns the bounds assert the X360 bakes as BrnProgressionData.h:460
    // ("liIndex < miRivalCount") -- callers do NOT duplicate it. GetRival bodies are
    // declare-only here (Rival is forward-declared; bodies land with the ProgressionData TU,
    // same convention as FindRival above).
    s32 GetRivalCount() const { return miRivalCount; }
    const Rival* GetRival(s32 liIndex) const;
    Rival*       GetRival(s32 liIndex);

    // ADDITIVE GROW (declare-only; bodies in the ProgressionData TU).
    // DriveThruManager::UnlockCarChallengeForCar walks the event-junction table.
    //   GetEventJunctionCount() -> muEventJunctionCount (count word +0x1C).
    //   GetEventJunction(i)     -> &mpaEventJunctions[i]  (base +0x18, 16-byte stride).
    u32 GetEventJunctionCount() const;
    const EventJunction* GetEventJunction(u32 luIndex) const;

    // ---- Other X360-attested methods this header owns (bodies are separate TUs) ------------
    // FixUp/FixDown keep the int-delta contract the committed ProgressionResourceType.cpp uses
    // (X360-authoritative over the PS3 DWARF's `void FixUp(MemoryResource)`).
    int FixUp(int liDelta);
    int FixDown(int liDelta);

private:
    // ========================================================================================
    // SERIALISED 32-BIT ARRAY-BASE SLOTS (corrected 2026-08-11).
    //
    // ProgressionData is the raw payload of PROGRESSION.DAT's single 0x1000E resource. Its nine
    // array bases are FILE-RELATIVE OFFSETS stored in FOUR-byte slots, which FixUp rebases by
    // adding the resource's 32-bit load base (CgsResource::GetLoadBase) -- so the record is
    // 0x50 bytes on the x64 host exactly as it is on the console. This is the same rule already
    // proven for VehicleListResource::muEntriesOffset / PlayerCarColours / the AttribSys vault.
    //
    // THE PREVIOUS DECLARATION WIDENED THEM TO HOST POINTERS AND WAS A LIVE CORRUPTION: it made
    // the record 0x98 bytes and moved every count word (muProgressionRankCount 0x14 -> 0x1C,
    // miRivalCount 0x2C -> 0x44, ...), so every accessor would have read a different field than
    // the one the shipped data holds.
    //
    // TWO INDEPENDENT PROOFS OF THE CONSOLE WIDTHS:
    //   * ProgressionData::FixDown @0x8267F220 reads the whole root as u32 WORDS -- `result[2]`,
    //     `result[4]`, `result[6]` ... `result[18]` are the nine bases and the odd words between
    //     them are the counts (20 words == 0x50).
    //   * tools/assets/bundles/progression_transcode.py ports the retail X360 bundle keeping every
    //     one of these widths, and its --verify contest reproduces EA's own little-endian port
    //     byte-for-byte over ~19.4 KB.
    //
    // Access is BY NAME through the private helpers below -- no raw offset arithmetic.
    // ========================================================================================
    u32 muVersionNumber;         // 0x00  (DWARF :249)  word 0
    u32 muSize;                  // 0x04  (DWARF :250)  word 1
    u32 muaPlayerCarIds;         // 0x08  (DWARF :252)  word 2   CgsID[]
    u32 muPlayerCarIdCount;      // 0x0C  (DWARF :253)  word 3

    u32 muaProgressionRanks;     // 0x10  (DWARF :255)  word 4   ProgressionRankData[]
    u32 muProgressionRankCount;  // 0x14  (DWARF :256)  word 5

    u32 muaEventJunctions;       // 0x18  (DWARF :258)  word 6   EventJunction[]
    u32 muEventJunctionCount;    // 0x1C  (DWARF :259)  word 7

    u32 muaEvents;               // 0x20  (DWARF :261)  word 8   RaceEventData[]
    u32 muEventCount;            // 0x24  (DWARF :262)  word 9

    u32 muaRivals;               // 0x28  (DWARF :264)  word 10  Rival[]
    s32 miRivalCount;            // 0x2C  (DWARF :265)  word 11

    u32 muaAIBalances;           // 0x30  (DWARF :267)  word 12  OpponentBalanceData[]
    u32 muAIBalanceCount;        // 0x34  (DWARF :268)  word 13

    u32 muaPersonalities;        // 0x38  (DWARF :270)  word 14  EventRacerPersonality[]
    u32 muPersonalityCount;      // 0x3C  (DWARF :271)  word 15

    u32 muaTrophyUnlocks;        // 0x40  (DWARF :273)  word 16  TrophyUnlockData[]
    u32 muTrophyUnlockCount;     // 0x44  (DWARF :274)  word 17

    u32 muaCarOpponentSet;       // 0x48  (DWARF :276)  word 18  CarOpponentSet[]
    u32 muCarOpponentsCount;     // 0x4C  (DWARF :277)  word 19

    // Serialised slot -> host address. Post-FixUp each slot holds the absolute address of its
    // table; the GameData heap is carved below 4 GB, which is the same guarantee
    // VehicleListResource::GetEntry and the ICE take dictionary already rely on.
    template <typename T>
    static T* TableFromSlot(u32 luSlot)
    {
        return reinterpret_cast<T*>(static_cast<uintptr_t>(luSlot));
    }

    ProgressionRankData*   GetProgressionRanks() const { return TableFromSlot<ProgressionRankData>(muaProgressionRanks); }
    EventJunction*         GetEventJunctions() const   { return TableFromSlot<EventJunction>(muaEventJunctions); }
    RaceEventData*         GetEvents() const           { return TableFromSlot<RaceEventData>(muaEvents); }
    Rival*                 GetRivals() const           { return TableFromSlot<Rival>(muaRivals); }
    OpponentBalanceData*   GetAIBalances() const       { return TableFromSlot<OpponentBalanceData>(muaAIBalances); }
    TrophyUnlockData*      GetTrophyUnlocks() const    { return TableFromSlot<TrophyUnlockData>(muaTrophyUnlocks); }
    CarOpponentSet*        GetCarOpponentSets() const  { return TableFromSlot<CarOpponentSet>(muaCarOpponentSet); }
};

// The 0x50 (80-byte) root ProgressionData::FixDown @0x8267F220 reads as 20 u32 words, and the
// root progression_transcode.py ports (SIZEOF['ROOT'] == 0x50). A host-pointer widening of any
// base slot breaks this.
static_assert(sizeof(ProgressionData) == 0x50, "BrnProgression::ProgressionData is an 0x50 serialised record");
}

#endif // BRN_PROGRESSION_DATA_H
