#include "SharedClasses/Progression/BrnRaceEventData.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (RaceEventData bounds checks)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnProgression::EventRacerPersonality::Construct   @ 0x826767B8
//   BrnProgression::RaceEventData::GetCheckpointData   @ 0x8230F808
//   BrnProgression::RaceEventData::GetRankScore        @ 0x823543D0
//   BrnProgression::RaceEventData::GetRankTime         @ 0x8230F748

namespace BrnProgression
{

// X360 0x826767B8. Resets the four AI tuning values to zero. The X360 build loads a single
// 0.0f constant (flt_82001CC0) and stores it into all four float slots.
void EventRacerPersonality::Construct()
{
    mfMinAggression = 0.0f;
    mfMaxAggression = 0.0f;
    mfSkill         = 0.0f;
    mfSpeed         = 0.0f;
}

// X360 0x8230F808. Bounds-checked accessor into the per-event checkpoint table (40-byte stride).
// Asserts 0 <= liCheckpointIndex < miCheckpointCount (BrnRaceEventData.h:953), then returns
// &mpaCheckpoints[liCheckpointIndex] (X360: `return 40 * index + *(this + 0x18)`).
const CheckpointData* RaceEventData::GetCheckpointData(s32 liCheckpointIndex) const
{
    CGS_ASSERT(liCheckpointIndex >= 0 && liCheckpointIndex < miCheckpointCount,
               "liCheckpointIndex >= 0 && liCheckpointIndex < miCheckpointCount");
    // CheckpointData is a complete 40-byte type (static_assert in the header), so the console's
    // `40 * index + base` is plain element indexing once the SERIALISED 32-bit table slot has
    // been turned into a host address (see the slot banner in BrnRaceEventData.h).
    const CheckpointData* lpaCheckpoints =
        reinterpret_cast<const CheckpointData*>(static_cast<uintptr_t>(muaCheckpointsOffset));
    return lpaCheckpoints + liCheckpointIndex;
}

// X360 0x823543D0. Returns the target score for rank luRank. The X360 build asserts the rank is
// in range (the signed `index < 0 || index >= 6` test collapses to the unsigned `luRank < 6`
// here; BrnRaceEventData.h:883), then returns maiRankScores[luRank] (X360: `*(this + 4*(rank+10))`
// == this + 0x28 + rank*4).
s32 RaceEventData::GetRankScore(u32 luRank) const
{
    CGS_ASSERT(luRank < KU_NUM_RANKS, "Invalid rank");
    return maiRankScores[luRank];
}

// X360 0x8230F748. Returns the target time (seconds) for rank luRank as an f32. The X360 build
// asserts the rank is in range (BrnRaceEventData.h:898), then loads the float at
// this + 4*(rank+0x10) == this + 0x40 + rank*4 (the `lfsx f1` proves a 32-bit float load/return;
// Hex-Rays mis-rendered the return as a _DWORD* through the assert-only path).
f32 RaceEventData::GetRankTime(u32 luRank) const
{
    CGS_ASSERT(luRank < KU_NUM_RANKS, "Invalid rank");
    return mafRankTimes[luRank];
}


// [H3b] The three record reads the sat-nav renderer needs (X360 inlines all three at the call
// sites; byte +0xEC / byte +0xED / doubleword +0x10 -- offsets proven by the renderer's
// GetIconInformation asm). The backing fields are the named pad carves in the header.
//
// The first two are the event's MODE and ONLINE MODE, not sat-nav presentation fields -- see the
// name-correction banner in BrnRaceEventData.h (DWARF mu8Mode/mu8OnlineMode + GetMode/GetOnlineMode,
// corroborated by tools/assets/bundles/progression_transcode.py's EVENT_MODE = 0xEC and its
// "EModeType tops out at 5" bundle check). The renderer only indexes an icon table with the mode.
u8 RaceEventData::GetMode() const
{
    return mu8Mode;
}

u8 RaceEventData::GetOnlineMode() const
{
    return mu8OnlineMode;
}

// Legacy-name aliases so the mounted sat-nav renderer keeps linking; new code calls GetMode /
// GetOnlineMode.
u8 RaceEventData::GetEventTypeByte() const
{
    return GetMode();
}

u8 RaceEventData::GetIconFrameBase() const
{
    return GetOnlineMode();
}

// ⛔ [stuntrace wave D, D3] MEMBER RENAMED, ACCESSOR KEPT. The doubleword at +0x10 is the DWARF's
// `CgsID mSpecialEventCarId` (:592), not an "event instance id" -- see the correction banner on the
// member in BrnRaceEventData.h and StartModeAtLights @0x82396F64's 8-byte `cmpld` of it against
// GameStateModule::GetOriginalCarId. This accessor keeps its name and its u64 return so the three
// committed callers (BrnSatNavRenderer, BrnPreRaceFlyBy_wJ_06, BrnProgressionManager_EventFinish)
// are untouched; new code calls GetSpecialEventCarId().
u64 RaceEventData::GetEventInstanceId() const
{
    return mSpecialEventCarId;
}

// [stuntrace wave D, D3] GetUnlockCarId -- was DECLARE-ONLY with a live caller
// (BrnDriveThruManager.cpp:827 `lpRaceEventData->GetUnlockCarId() == lRepairedCarID`) and no body
// anywhere, which BrnProgressionManager_EventFinish.cpp:352 had to work around by calling
// GetEventInstanceId() and casting. Both names mean the same DWARF member; bodied here as the
// straight read so the workaround can retire.
CgsID RaceEventData::GetUnlockCarId() const
{
    return mSpecialEventCarId;
}

// ---------------------------------------------------------------------------------------------
// [stuntrace waveB MOUNT-CLOSURE round, 2026-08-26] The two start-grid rival counts. Both were
// declare-only ("their backing fields are not in this minimal slice"); the fields are carved now,
// so these are plain named reads of bytes +0xEE / +0xEF -- see the accessor banner in the header
// for the DWARF lines, the transcoder run and, decisively, RaceMode::Start @0x82330018's own
// `lbz r11, 0xEE(r23)` / `lbz r11, 0xEF(r23)`.
//
// Bodied here rather than inline for the same reason GetMode / GetOnlineMode above are: the X360
// inlines them at every call site (no standalone symbol in the ledger), and this file is where
// this record's inlined-on-console reads already live. No assert on either -- the console has
// none, and neither byte is indexed.
// ---------------------------------------------------------------------------------------------

// The rivals placed on the grid at the lights. RaceMode::Start clamps this against the per-rank
// ProgressionRankData::GetRaceRivalsNumber (rank+0x5C) before writing the grid.
u8 RaceEventData::GetStartRivalCount() const
{
    return mu8StartRivalCount;
}

// The rivals joined mid-event. RaceMode::Start adds it to the start count to get the mode's total
// opponent count (`add r11,r11,r10` @0x82330420, stored at params+0xBC).
u8 RaceEventData::GetAddRivalCount() const
{
    return mu8AddRivalCount;
}

// ---------------------------------------------------------------------------------------------
// ⛔ EventJunction::GetEventId / EventJunction::GetId -- DELETED 2026-08-27.
//
// The RETIRE-WHEN below is discharged: DriveThruManager::UnlockCarChallengeForCar, their only
// caller, now reads GetID() at both sites and posts the junction id into JunctionInfoAction::
// muEventJunctionID as the 4-byte word the console stores. Nothing declares or references the
// two aliases any more, so the definitions go with the declarations rather than sitting here as
// an unreferenced widening someone can pick up again. The analysis is kept verbatim because it
// is the evidence for the caller's fix:
//
// ⚠️⚠️ BOTH READ THE SAME WORD, AND THE ONE WORD IS 32 BITS. The X360 has no standalone symbol
// for either -- the two names come from the reconstruction of DriveThruManager::
// UnlockCarChallengeForCar, which reads the junction record TWICE in one arm and spelled the two
// reads as two accessors. The asm (@0x82386988 for the search key, @0x82386A54 for the payload)
// is `lwz r11, 0(r30)` both times: offset ZERO, load-word, i.e. muID -- the same u32 the
// DWARF-attested GetID() already returns. There is no second id and no 64-bit field in the
// record: EventJunction is 16 bytes of {muID, muOfflineEventOffset, muOnlineEventOffset,
// miShotGroup}, a stride ProgressionData::FixDown @0x8267F220 walks and the transcoder ports.
//
// ⛔ SO THE DECLARED CgsID RETURN IS A WIDENING THE BINARY DOES NOT HAVE. Neither accessor may
// be used to serialise a junction id -- the record's id IS four bytes.
//
// ✅ FIXED 2026-08-27 in the caller (BrnDriveThruManager.cpp): the console stores that u32 into
// its 40-byte action-201 record at record+0x04 as a WORD (`stw r11, 0x100+var_9C(r1)`, record
// base var_A0), while the committed reconstruction memcpy'd an 8-byte CgsID to record+0x14.
// Both the offset and the width were wrong; the widened write also clobbered rec+0x14..0x1B
// (JunctionInfoAction::maPad0C tail + mSpecialEventCarId) and left muEventJunctionID at zero,
// so every junction this arm announced identified itself to the GUI as event 0. The caller now
// builds a real JunctionInfoAction and fills the six fields the console fills.
// ---------------------------------------------------------------------------------------------

}
