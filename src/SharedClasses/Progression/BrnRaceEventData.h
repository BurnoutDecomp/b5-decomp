#ifndef BRN_RACE_EVENT_DATA_H
#define BRN_RACE_EVENT_DATA_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID (EventJunction / RaceEventData id accessors)
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT (GetStartGridSlot's two guards)
#include "GameSource/World/BrnWorldSharedConstants.h"     // BrnWorld::KI_MAX_RIVALS_IN_MODE

#include <cstdint>            // uintptr_t (the serialised 32-bit pointer slots below)

// =============================================================================
// BrnRaceEventData.h  (OWNING HEADER for the BrnProgression race-event leaf types)
//
// DWARF home: SharedClasses/Progression/BrnRaceEventData.h. The canonical file is the
// home of several sibling types (EventRacerPersonality, EventStartGridSlot, CheckpointData,
// RaceEventData, EventJunction). This is a MINIMAL OWNING SLICE: a sibling type is carved
// here only once a caller needs it, with its real layout + the accessors the X360 attests.
// Complete so far: EventJunction, EventRacerPersonality, CheckpointData (full DWARF layout),
// RaceEventData (partial -- named members + explicit padding). EventStartGridSlot landed
// 2026-09-02 (rival-spawn wave R) with the start-grid table it sizes. When another type is
// needed it grows this single-owner header (do not fork).
//
// LAYOUT is X360-faithful and taken from the DecFIGS DWARF for this exact path. The
// EventRacerPersonality record is four contiguous f32 tuning values (proven by
// Construct, which stores 0.0f into the four float slots at byte offsets 0/4/8/0xC).
// =============================================================================

namespace BrnProgression
{

// Forward decl for the event-junction event pointers (complete RaceEventData is defined below;
// the junction only stores it by pointer).
struct RaceEventData;

// ============================================================================================
// SERIALISED 32-BIT POINTER SLOTS (2026-08-11) -- WHY THE TWO EVENT MEMBERS BELOW ARE u32.
//
// EventJunction and RaceEventData are *serialised resource records*: they are read straight out
// of PROGRESSION.DAT's single 0x1000E resource, and their "pointers" are FILE-RELATIVE OFFSETS
// that ProgressionData::FixUp rebases by adding the resource's 32-bit load base
// (CgsResource::GetLoadBase). The slots are therefore FOUR bytes wide on the x64 host, exactly as
// they are on the console -- the same rule already proven for VehicleListResource::muEntriesOffset
// and the AttribSys vault entries.
//
// THIS WAS A LIVE LAYOUT CORRUPTION, not a style point. The earlier declaration spelled them as
// host `const RaceEventData*`, which on x64 makes EventJunction 24 bytes (aligned to 32) against
// the 16-byte record the data actually is, and pushes RaceEventData::miCheckpointCount from +0x1C
// to +0x20 and the record from 248 to 256 bytes. The X360 proves the console widths directly --
// ProgressionData::FixDown @0x8267F220 strides the junction array by 16 (`v3 += 16`) touching
// `+4` and `+8`, and strides the event array by 248 (`v8 += 248`) touching `+24` -- and
// tools/assets/bundles/progression_transcode.py keeps every one of those widths when it ports the
// retail X360 bundle to platform 4 (its --verify contest is byte-for-byte against EA's own
// little-endian port).
// ============================================================================================

// Per-junction record that links an offline event to an online event. DWARF BrnRaceEventData.h:
// EventJunction. 16 bytes. ProgressionData's relocation walks the junction array and rebases the
// two event slots, so they are named here.
struct EventJunction
{
    u32 GetID() const { return muID; }
    const RaceEventData* GetOfflineEvent() const
    {
        // Serialised slot -> host address (post-FixUp the slot holds the absolute address; the
        // GameData heap is carved below 4 GB, the same guarantee VehicleListResource relies on).
        return reinterpret_cast<const RaceEventData*>(static_cast<uintptr_t>(muOfflineEventOffset));
    }
    const RaceEventData* GetOnlineEvent() const
    {
        return reinterpret_cast<const RaceEventData*>(static_cast<uintptr_t>(muOnlineEventOffset));
    }
    s32 GetShotGroup() const { return miShotGroup; }

    // ---- Remaining attested API (bodies in their own TUs; declaration-only) ----
    void Construct(u32 luID, const RaceEventData* lpOfflineEvent, const RaceEventData* lpOnlineEvent, s32 liShotGroup);

    // ⛔ RETIRED 2026-08-27 (drive-thru link-closure wave): `CgsID GetEventId() const` and
    // `CgsID GetId() const`. They were two CgsID-returning aliases minted by the
    // DriveThruManager::UnlockCarChallengeForCar reconstruction for the two places that arm
    // reads the junction record -- but @0x82386988 and @0x82386A54 are BOTH `lwz r11, 0(r30)`:
    // offset zero, load-WORD, i.e. the muID below, which the DWARF-attested GetID() already
    // returns. The widening was never in the binary, and it is what let the action-201 payload
    // be written as an 8-byte field. Sole caller now uses GetID(); do not re-mint these.

    u32 muID;                    // 0x00 (DWARF :80)
    u32 muOfflineEventOffset;    // 0x04 (DWARF :82)  serialised 32-bit slot (FixUp-rebased)
    u32 muOnlineEventOffset;     // 0x08 (DWARF :83)  serialised 32-bit slot (FixUp-rebased)
    s32 miShotGroup;             // 0x0C (DWARF :85)
};

// The 16-byte stride ProgressionData::FixDown @0x8267F220 walks the junction array with
// (`v3 += 16`), and the stride progression_transcode.py ports the retail table at.
static_assert(sizeof(EventJunction) == 16, "BrnProgression::EventJunction is a 16-byte record");

// Per-racer AI tuning record carried by the start-grid setup. DWARF BrnRaceEventData.h:99.
// Four f32 fields: an aggression range, a skill scalar and a speed scalar.
struct EventRacerPersonality
{
    // X360 0x826767B8. Zeroes all four tuning values.
    void Construct();

    // ---- Remaining X360-attested API (bodies in their own TUs; declaration-only here) ----
    void FixDown();
    void FixUp();
    f32  GetMinAggression() const;
    f32  GetMaxAggression() const;
    void SetAggression(f32 lfMin, f32 lfMax);
    f32  GetSkill() const;
    void SetSkill(f32 lfSkill);
    f32  GetSpeed() const;
    void SetSpeed(f32 lfSpeed);

private:
    f32 mfMinAggression;   // 0x00 (DWARF BrnRaceEventData.h:137)
    f32 mfMaxAggression;   // 0x04 (DWARF :138)
    f32 mfSkill;           // 0x08 (DWARF :139)
    f32 mfSpeed;           // 0x0C (DWARF :140)
};

// ----------------------------------------------------------------------------
// BrnProgression::EventStartGridSlot -- one authored start-grid slot of a race event
// (DWARF BrnRaceEventData.h:152; members :228-:233; the EFlags enum :155).
//
// [rival-spawn wave R, 2026-09-02] Landed because ModeManager::SetupOpponentData @0x82329348
// walks the event's slot table (`addi r28, r29, 0x5C` then `addi r28, r28, 0x14` per pass ==
// base +0x5C, stride 20) and reads three of its words, and RaceCarEntityModule::SetUpAIForMode
// @0x82301620 reads the flags byte of the copy carried inside OpponentData
// (`lbz r11, 0x19(r28); clrlwi r28, r11, 31` == OpponentData+0x08+0x11, bit 0).
//
// LAYOUT: 18 bytes of scalars padded to the 20-byte stride the console walks. All scalar, no
// pointers, so the host layout matches the console byte-for-byte (sizeof pinned below).
//
// WHICH WORD IS WHICH (asm-pinned, SetupOpponentData):
//   +0x00 muOpponentIndex           `lwz r10, 0(r28)`   -> taken modulo the opponent-set count
//   +0x08 miFastAIBalanceGraphIndex `lwz r30, 8(r28)`   -> r6 == GetInterpolatedAIBalanceGraph's liIndexB
//   +0x0C miSlowAIBalanceGraphIndex `lwz r31, 0xC(r28)` -> r5 == its liIndexA
//   +0x11 muFlags                   bit 0 == E_FLAG_CAN_DEVIATE_FROM_ROUTE (SetUpAIForMode)
// The accessors are the DWARF's (:174-:225). The X360 emits no standalone symbol for any of
// them -- both consumers open-code the loads -- so they are header-inline, the same precedent
// as GetCheckpointCount below.
// ----------------------------------------------------------------------------
struct EventStartGridSlot
{
    // DWARF BrnRaceEventData.h:155
    enum EFlags
    {
        E_FLAG_CAN_DEVIATE_FROM_ROUTE = 1,
        E_FLAG_CAN_TAKE_SHORTCUTS     = 2,
    };

    // ---- Remaining X360-attested API (bodies in their own TUs; declaration-only here) ----
    void Construct();
    void FixDown();
    void FixUp();

    u32  GetOpponentIndex() const                { return muOpponentIndex; }
    void SetOpponentIndex(u32 luIndex)           { muOpponentIndex = luIndex; }
    u32  GetPersonalityIndex() const             { return muPersonalityIndex; }
    void SetPersonalityIndex(u32 luIndex)        { muPersonalityIndex = luIndex; }
    s32  GetFastAIBalanceGraphIndex() const      { return miFastAIBalanceGraphIndex; }
    s32  GetSlowAIBalanceGraphIndex() const      { return miSlowAIBalanceGraphIndex; }
    void SetSlowAIBalanceGraphIndex(s32 liIndex) { miSlowAIBalanceGraphIndex = liIndex; }
    void SetFastAIBalanceGraphIndex(s32 liIndex) { miFastAIBalanceGraphIndex = liIndex; }
    u8   GetColourIndex() const                  { return muColourIndex; }
    void SetColourIndex(u8 luColourIndex)        { muColourIndex = luColourIndex; }
    bool GetFlag(EFlags leFlag) const            { return (muFlags & static_cast<u8>(leFlag)) != 0; }
    u8   GetFlags() const                        { return muFlags; }
    void SetFlag(EFlags leFlag)                  { muFlags = static_cast<u8>(muFlags | static_cast<u8>(leFlag)); }
    void SetFlags(u8 luFlags)                    { muFlags = luFlags; }
    void ClearFlag(EFlags leFlag)                { muFlags = static_cast<u8>(muFlags & ~static_cast<u8>(leFlag)); }

private:
    u32 muOpponentIndex;            // 0x00 (DWARF :228)
    u32 muPersonalityIndex;         // 0x04 (DWARF :229)
    s32 miFastAIBalanceGraphIndex;  // 0x08 (DWARF :230)
    s32 miSlowAIBalanceGraphIndex;  // 0x0C (DWARF :231)
    u8  muColourIndex;              // 0x10 (DWARF :232)
    u8  muFlags;                    // 0x11 (DWARF :233)
    // 0x12..0x13: alignment to the 20-byte stride SetupOpponentData walks (`addi r28, r28, 0x14`).
};

// The 20-byte stride SetupOpponentData @0x82329348 walks the slot table with, and the stride
// the 5-dword copy into OpponentData (`li r9, 5; mtctr` @0x82329688) preserves.
static_assert(sizeof(EventStartGridSlot) == 20, "BrnProgression::EventStartGridSlot is a 20-byte record");

// ----------------------------------------------------------------------------
// BrnProgression::CheckpointData -- one entry of a race event's checkpoint table.
//
// SCOPE NOTE: the DWARF gives this as a NAMESPACE-SCOPE sibling of RaceEventData
// (`struct BrnProgression::CheckpointData`, DWARF BrnRaceEventData.h:246), exactly like
// EventJunction / EventRacerPersonality / EventStartGridSlot -- and exactly as this file's
// own banner already describes it. An earlier slice of this header declared it nested inside
// RaceEventData (`RaceEventData::CheckpointData`); that nesting was not DWARF-backed and is
// corrected here. RaceEventData's mpaCheckpoints / GetCheckpointData / SetCheckpointData all
// name it unqualified in the DWARF, which resolves to this namespace-scope type.
//
// Do not confuse with BrnGameState::CheckpointData (GameSource/GameState/BrnCheckpointData.h),
// which is an unrelated 44-byte record.
//
// LAYOUT: 40 bytes (0x28), all scalar, no pointers -- so the host layout matches the console
// byte-for-byte. The 0x28 stride is independently PROVEN by the X360:
// RaceEventData::GetCheckpointData @0x8230F858 computes `((i + i*4) << 3) + *(this+0x18)`
// == i*40 + base.
// ----------------------------------------------------------------------------
struct CheckpointData
{
    // DWARF BrnRaceEventData.h:248 (emitted by the dumper both as this in-class constant and
    // as its namespace-scope out-of-line definition). Sizes mauBlockSectionIds.
    static const s32 KI_MAX_BLOCK_SECTION_COUNT = 8;

    // INLINE ON PURPOSE (not a stub): no standalone X360 symbol exists for this accessor --
    // callers in *other* TUs inline the load directly, which is only possible if the original
    // header defined it inline. Proof: BrnGui::CrashNavMap::CalculateEventZoomFactor
    // @0x824BF5E4 does `lwzx r4, r30, r11` (r11 == mpaCheckpoints, r30 stepping 0x28 per pass)
    // and feeds the loaded word straight to GuiCache::GetLandmarkInfoFromID -- no call. Same
    // shape in BrnGui::PreRaceFlyByState::CalculateZoomFactor @0x824BE8F0: `lwz r11,0x18(r29)`
    // then `lwzx r4, r30, r11` @0x824BEA30 with `addi r30, r30, 0x28` @0x824BEA44 as the stride.
    // DWARF BrnRaceEventData.h:265.
    u32 GetLandmarkId() const { return muLandmarkId; }

    // DWARF BrnRaceEventData.h:268. Inline for the same reason (no standalone X360 symbol).
    s32 GetBlockSectionCount() const { return miBlockSectionCount; }

    // ---- Remaining DWARF-attested API (declaration-only) ----
    // FLAG: none of these has a standalone X360 symbol either (the whole record is
    // header-inline on the console) and none has a caller in the tree yet, so they are left
    // DECLARED ONLY rather than given fabricated bodies. Whichever TU first needs one bodies
    // it -- in this header if the asm proves it inline, in BrnRaceEventData.cpp otherwise.
    void Construct(u32 luLandmarkId);              // DWARF :252
    void FixDown();                                // DWARF :255
    void FixUp();                                  // DWARF :258
    void AddBlockSection(u32 luBlockSectionId);    // DWARF :262
    u32  GetBlockSectionId(s32 liIndex) const;     // DWARF :272

private:
    u32 muLandmarkId;                                     // 0x00 (DWARF :275)
    s32 miBlockSectionCount;                              // 0x04 (DWARF :276)
    u32 mauBlockSectionIds[KI_MAX_BLOCK_SECTION_COUNT];   // 0x08..0x27 (DWARF :277)
};

// The 0x28 element stride RaceEventData::GetCheckpointData hard-codes. Pointer-free scalar
// run, so this is a host-safe check of the console layout rather than a baked console offset.
static_assert(sizeof(CheckpointData) == 40, "BrnProgression::CheckpointData is a 40-byte (0x28) record");

// ----------------------------------------------------------------------------
// BrnProgression::RaceEventData -- one serialised offline/online race event record. DWARF home
// SharedClasses/Progression/BrnRaceEventData.h. The X360 proves a 248-byte (0xF8) record (see
// ProgressionData::FixDown @0x8267F220, which strides the event table by 248 and rebases the
// checkpoint pointer at +0x18). This is a MINIMAL OWNING SLICE: only the members the accessors
// bodied in this batch touch are named at their X360-proven offsets; the remaining bytes are
// explicit named padding so the touched members keep their exact offsets WITHOUT fabricating
// member names for the not-yet-recovered fields. Grow in place (replace a pad with the real
// DWARF member) when a future caller needs it -- do not fork a second definition.
// ----------------------------------------------------------------------------
struct RaceEventData
{
    // The per-checkpoint record this event's table is made of is BrnProgression::CheckpointData,
    // defined above at namespace scope (DWARF :246) -- it is NOT a nested type. It is now a
    // COMPLETE type, so GetCheckpointData's result can be dereferenced.

    // Number of rank thresholds (the rank-score / rank-time tables are sized to this; the X360
    // bounds-checks the rank arg against 6 in GetRankScore @0x823543D0 / GetRankTime @0x8230F748).
    static const u32 KU_NUM_RANKS = 6;

    // Width of the authored start-grid table (DWARF :256 `EventStartGridSlot[7] maStartGridSlots`),
    // == BrnWorld::KI_MAX_RIVALS_IN_MODE, the bound GetStartGridSlot's :1166 assert names.
    static const u32 KU_MAX_START_GRID_SLOTS = 7;

    // Progression race-event mode classification. DWARF-attested
    // (references/DecFIGS/dwarfdump/SharedClasses/Progression/BrnRaceEventData.h).
    // BrnGui::EventPanel::ConvertLocalEventDefToProgressionEventDef maps its local EEventType
    // onto this; E_MODE_PURSUIT=5 is retained even though panel-side mapping collapses
    // E_EVENT_TYPE_ALL to E_MODE_COUNT.
    enum EModeType
    {
        E_MODE_INVALID       = -1,
        E_MODE_RACE          = 0,
        E_MODE_ROAD_RAGE     = 1,
        E_MODE_STUNT_ATTACK  = 2,
        E_MODE_SURVIVOR      = 3,
        E_MODE_BURNING_ROUTE = 4,
        E_MODE_PURSUIT       = 5,
        E_MODE_COUNT         = 6,
    };

    // The event's ONLINE mode classification -- a SEPARATE, much shorter enum from EModeType, and
    // the type of the byte at +0xED. DWARF-attested
    // (references/DecFIGS/dwarfdump/SharedClasses/Progression/BrnRaceEventData.h:18, source line
    // 306): `enum EOnlineModeType`.
    enum EOnlineModeType
    {
        E_ONLINE_MODE_RACE             = 0,
        E_ONLINE_MODE_ROAD_RAGE        = 1,
        E_ONLINE_MODE_BURNING_HOME_RUN = 2,
        E_ONLINE_MODE_COUNT            = 3,
    };

    // X360 0x8230F808. Returns &mpaCheckpoints[liCheckpointIndex] (asserts 0 <= index <
    // miCheckpointCount, BrnRaceEventData.h:953). 40-byte stride.
    const CheckpointData* GetCheckpointData(s32 liCheckpointIndex) const;

    // The live checkpoint count -- the bound GetCheckpointData asserts against. DWARF
    // BrnRaceEventData.h:368 (`int32_t GetCheckpointCount() const;`).
    // INLINE ON PURPOSE (not a stub): there is no standalone X360 symbol for it, yet callers in
    // OTHER TUs load the word directly -- CrashNavMap::CalculateEventZoomFactor @0x824BF590
    // (`lwz r11, 0x1C(r29)`) and PreRaceFlyByState::CalculateZoomFactor @0x824BE9DC/0x824BEA78 both
    // re-read +0x1C every loop pass with no call. That is only possible if the original header
    // defined the accessor inline, so it is reconstructed inline here.
    s32 GetCheckpointCount() const { return miCheckpointCount; }

    // X360 0x823543D0. Returns the target score for rank luRank (asserts luRank < 6,
    // BrnRaceEventData.h:883). maiRankScores lives at +0x28.
    s32 GetRankScore(u32 luRank) const;

    // X360 0x8230F748. Returns the target time (seconds, f32) for rank luRank (asserts luRank < 6,
    // BrnRaceEventData.h:898). mafRankTimes lives at +0x40.
    f32 GetRankTime(u32 luRank) const;

    // ---- The start-grid rival counts (BODIED 2026-08-26, stuntrace waveB MOUNT-CLOSURE round) --
    // The number of rivals on the start grid / added during the event. RaceMode::Start reads both.
    // They were declare-only because "their backing fields are not in this minimal slice"; the two
    // fields are now carved at their proven offsets below and the bodies live in
    // BrnRaceEventData.cpp next to GetMode/GetOnlineMode (their exact siblings in the +0xEC run).
    //
    // OFFSETS, PROVEN THREE WAYS AND AGREEING:
    //   * DWARF BrnRaceEventData.h:268/:271 declares `uint8_t mu8StartRivalCount;` immediately
    //     followed by `uint8_t mu8AddRivalCount;`, the third and fourth members of the eleven-byte
    //     mu8Mode..mi8UnlockRank run that starts at +0xEC -- so +0xEE and +0xEF.
    //   * progression_transcode.py models that run as `(0xEC, 1, 11)` with the record 0xF8 bytes
    //     and +0xF7 as pad, which fixes the run's base and therefore both bytes.
    //   * THE CONSUMER'S ASM. RaceMode::Start @0x82330018 reads exactly those two bytes off the
    //     event record (r23) and nothing else in the run:
    //         0x82330324  lbz r11, 0xEE(r23)   ; the start-grid count, then
    //         0x82330328  lbz r10, 0x5C(r24)   ; clamped against the RANK's muRaceRivalsNumber
    //         0x82330334  cmplw cr6, r9, r8    ; `min(event start count, rank rival number)`
    //         0x82330414  lbz r11, 0xEF(r23)   ; the add count, then
    //         0x82330418  lbz r10, 0xEE(r23)   ; summed (`add r11,r11,r10` @0x82330420) and
    //         0x8233042C  stw r11, 0xBC(r22)   ; stored as the mode's total opponent count.
    //     The pairing of +0xEE with the rank record's +0x5C is what makes the START/ADD split
    //     unambiguous: only the START count is the one clamped by the per-rank rival number.
    // Both are `lbz` with no `extsb`, hence the u8 returns the DWARF also declares.
    u8 GetStartRivalCount() const;
    u8 GetAddRivalCount() const;

    // ADDITIVE GROW (declare-only; body in the RaceEventData TU).
    // The car id this event unlocks (X360 word +0x14 -- the value DriveThruManager::
    // UnlockCarChallengeForCar matches each junction against the repaired car id). Declare-only.
    CgsID GetUnlockCarId() const;

    // ADDITIVE GROW (declare-only; bodies in the RaceEventData TU) -- BrnSatNavRenderer TU.
    //
    // NAME CORRECTION (2026-08-26): bytes +0xEC/+0xED are NOT sat-nav presentation fields. They are
    // the event's game MODE and ONLINE mode, and the DWARF names them so directly --
    // references/DecFIGS/dwarfdump/SharedClasses/Progression/BrnRaceEventData.h lines 262/265
    // declare `uint8_t mu8Mode;` then `uint8_t mu8OnlineMode;` as adjacent members, with
    // `EModeType GetMode() const;` / `EOnlineModeType GetOnlineMode() const;` (lines 308/311). The
    // repo's own retail-bundle transcoder already agrees: tools/assets/bundles/
    // progression_transcode.py sets EVENT_MODE = 0xEC, labels the +0xEC..+0xF7 run
    // "mu8Mode .. mi8UnlockRank", and rejects any event whose +0xEC byte exceeds 5 because
    // "RaceEventData::EModeType tops out at 5".
    //
    // The sat-nav renderer is a CONSUMER, not the owner of the meaning: it merely indexes its own
    // icon table with the mode. BrnSatNavRenderer.cpp's KAU_EVENTTYPE_TO_ICONROW has exactly six
    // authored entries {3,1,5,4,2,4} -- one per E_MODE_RACE..E_MODE_PURSUIT -- and the online
    // path adds 6 to the online mode to reach the online icon-frame block, which is what once
    // made +0xED look like an "icon frame base". Icon rows are renderer policy; the mode is data.
    //
    //   * GetMode            -- byte +0xEC, RaceEventData::EModeType.
    //   * GetOnlineMode      -- byte +0xED, RaceEventData::EOnlineModeType.
    //   * GetEventInstanceId -- X360 doubleword +0x10; the event-instance id the renderer
    //     compares against the cache's "current" event to special-case the active event.
    // Returned by accessor so consumers stay off raw +0xEC/+0xED/+0x10 casts.
    // [H3b] bodies in BrnRaceEventData.cpp now (the sat-nav renderer links against the
    // mode pair); the backing bytes are carved at their proven offsets below.
    u8    GetMode() const;
    u8    GetOnlineMode() const;
    u64   GetEventInstanceId() const;

    // THIN ALIASES, kept only so the already-mounted sat-nav renderer keeps compiling and linking
    // against the names it was written with (BrnSatNavRenderer.cpp:486, :521, :719). They return
    // the same two bytes as GetMode / GetOnlineMode. New code must call GetMode / GetOnlineMode --
    // "event type" and "icon frame base" were both wrong readings of these bytes.
    u8    GetEventTypeByte() const;
    u8    GetIconFrameBase() const;

    // ⭐ [stuntrace wave D, D3] The three leading scalars GameStateModule::StartModeAtLights
    // @0x82396CF8 reads (the asm citations are on the members). GetSpecialEventCarId is the
    // correctly-named twin of GetEventInstanceId above -- SAME doubleword, DWARF name.
    // The X360 emits no standalone symbol for any of the three (all inlined at the reader), so
    // they are header-inlines, exactly like ProgressionData::GetProgressionRankCount.
    // ⭐ [main-menu wave G1] The two per-event time limits (DWARF :461/:467
    // `float32_t GetTimeLimitFast() const;` / `GetTimeLimitSlow() const;`).
    // INLINE ON PURPOSE, for the same reason GetCheckpointCount above is: the X360 emits no
    // standalone symbol for either, and a caller in ANOTHER TU loads the word directly --
    // BrnGui::EventPanel::SetEventData @0x824312D8 does `lfs f1, 0x24(r24)` with no call on the
    // burning-route arm (the event's own target time, used when no challenged score overrides
    // it). That is only possible if the original header defined the accessor inline.
    f32   GetTimeLimitFast() const    { return mfTimeLimitFast; }
    f32   GetTimeLimitSlow() const    { return mfTimeLimitSlow; }

    // ⭐ [rival-spawn wave R, 2026-09-02] THE START-GRID PAIR (DWARF :524 `const EventStartGridSlot*
    // GetStartGridSlot(uint32_t) const;` / :532 `uint32_t GetStartGridCount() const;`).
    // INLINE ON PURPOSE, same reason as GetCheckpointCount: the X360 emits no standalone symbol
    // for either. ModeManager::SetupOpponentData @0x82329348 carries BOTH of GetStartGridSlot's
    // asserts at the call site with their baked header lines -- `li r5, 0x48E` (:1166,
    // "muStartGridCount <= (uint32_t)KI_MAX_RIVALS_IN_MODE") and `li r5, 0x48F` (:1167,
    // "luIndex < muStartGridCount") -- and then indexes the table itself. Those two asserts are
    // therefore THIS body's, and callers must not restate them.
    u32 GetStartGridCount() const { return muStartGridCount; }
    const EventStartGridSlot* GetStartGridSlot(u32 luIndex) const
    {
        CGS_ASSERT(muStartGridCount <= static_cast<u32>(BrnWorld::KI_MAX_RIVALS_IN_MODE),
                   "muStartGridCount <= (uint32_t)KI_MAX_RIVALS_IN_MODE");    // BrnRaceEventData.h:1166
        CGS_ASSERT(luIndex < muStartGridCount, "luIndex < muStartGridCount");  // BrnRaceEventData.h:1167
        return &maStartGridSlots[luIndex];
    }

    f32   GetTrafficDensity() const   { return mfTrafficDensity; }
    f32   GetBoostEarning() const     { return mfBoostEarning; }
    CgsID GetSpecialEventCarId() const { return mSpecialEventCarId; }

private:
    // The checkpoint-table base is a SERIALISED 32-BIT SLOT -- see the banner above EventJunction.
    // ProgressionData::FixUp/FixDown rebase it in place (`*(event + 24) -= delta` @0x8267F220), and
    // the whole record is 248 bytes with miCheckpointCount at +0x1C; a host pointer here would move
    // the count to +0x20 and grow the record to 256, desynchronising it from the shipped data.
    // ⭐⭐ [stuntrace wave D, D3] THE FIRST FIVE MEMBERS ARE CARVED, AND THE NAMES ARE THE
    // DWARF's OWN. references/DecFIGS/dwarfdump/SharedClasses/Progression/BrnRaceEventData.h
    // lists `struct BrnProgression::RaceEventData` opening with, in declaration order:
    //     :586 uint32_t  muId
    //     :587 uint32_t  muFlags
    //     :589 float32_t mfTrafficDensity
    //     :590 float32_t mfBoostEarning
    //     :592 CgsID     mSpecialEventCarId
    //     :594 CheckpointData* mpaCheckpoints          <- the +0x18 slot already carved below
    // which lands mfTrafficDensity at +0x08, mfBoostEarning at +0x0C and mSpecialEventCarId at
    // +0x10 -- and all three are independently attested by GameStateModule::StartModeAtLights
    // @0x82396CF8, which reads exactly those three bytes off the event record (r31):
    //     0x82396FF0  lfs f0, 8(r31)    -> StartGameModeParams+792  (mfTrafficDensity)
    //     0x82397000  lfs f0, 0xC(r31)  -> StartGameModeParams+796  (mfBoostEarning)
    //     0x82396F64  ld  r11, 0x10(r31); cmpld against GameStateModule::GetOriginalCarId's
    //                 CgsID return -- an 8-BYTE id compare, which is what makes +0x10 a CgsID.
    // ⛔ NAME CORRECTION, AND IT IS A REAL DEFECT THIS WAVE FOUND: this member was committed as
    // `u64 muEventInstanceId` with the sat-nav renderer named as its owner. The sat-nav is a
    // CONSUMER of the id; the DWARF and the StartModeAtLights compare both say it is the
    // SPECIAL-EVENT CAR. Two already-committed call sites had independently worked that out and
    // said so in their own comments -- BrnPreRaceFlyBy_wJ_06.cpp:170 asserts
    // `lpEventData->GetEventInstanceId() != 0` with the message "lpEventData->IsSpecialEvent()"
    // and then assigns the result to `const CgsID lPlayersCarId`, and
    // BrnProgressionManager_EventFinish.cpp:352-358 casts it to CgsID with a NAME NOTE saying the
    // semantically-right accessor is GetUnlockCarId(). BrnGameActions.h:1287's JunctionInfoAction
    // is the third witness (it records "RaceEventData+0x10 -> GuiEventJunctionInfo::
    // mSpecialEventCarId"). GetEventInstanceId() and GetUnlockCarId() are KEPT as aliases so no
    // committed caller has to change; new code calls GetSpecialEventCarId().
    u32   muId;                         // 0x00  (DWARF :586)
    u32   muFlags;                      // 0x04  (DWARF :587)
    f32   mfTrafficDensity;             // 0x08  (DWARF :589)  GetTrafficDensity
    f32   mfBoostEarning;               // 0x0C  (DWARF :590)  GetBoostEarning
    CgsID mSpecialEventCarId;           // 0x10  (DWARF :592)  GetSpecialEventCarId
    u32 muaCheckpointsOffset;           // 0x18  checkpoint table base (FixUp-rebased 32-bit slot)
    s32 miCheckpointCount;              // 0x1C  live checkpoint count
    // ⭐ [main-menu wave G1, 2026-08-29] CARVED OUT OF maPad_20. The DWARF
    // (references/DecFIGS/dwarfdump/SharedClasses/Progression/BrnRaceEventData.h:241/:244)
    // lists `float32_t mfTimeLimitFast;` then `float32_t mfTimeLimitSlow;` as the two members
    // immediately after miCheckpointCount and immediately before miRankScore[6] -- so they are
    // exactly the eight bytes this pad covered, at +0x20 and +0x24. X360-attested by
    // BrnGui::EventPanel::SetEventData @0x824312D8 (`lfs f1, 0x24(event)`), which reads the
    // burning-route target time straight off the record with no call -- see the accessor note.
    f32 mfTimeLimitFast;                // 0x20  (DWARF :241)
    f32 mfTimeLimitSlow;                // 0x24  (DWARF :244)
    s32 maiRankScores[KU_NUM_RANKS];    // 0x28  rank target scores (6 * 4 == 0x18 -> 0x40)
    f32 mafRankTimes[KU_NUM_RANKS];     // 0x40  rank target times  (6 * 4 == 0x18 -> 0x58)
    // ⭐ [rival-spawn wave R, 2026-09-02] CARVED OUT OF maPad_58 (which covered 0x58..0xEB). The
    // DWARF lists, in order, `float32_t mfExtensionTime;` (:253), `EventStartGridSlot[7]
    // maStartGridSlots;` (:256) and `uint32_t muStartGridCount;` (:259) between mfRankTime[6]
    // and mu8Mode -- 4 + 7*20 + 4 == 0x94, exactly the bytes the pad covered. Both X360
    // consumers pin the two grid members without a call: ModeManager::SetupOpponentData
    // @0x82329348 reads the count as `lwz r14, 0xE8(r29)` (and re-reads it for the :1166/:1167
    // asserts) and walks the slots from `addi r28, r29, 0x5C` in 0x14 steps.
    f32                mfExtensionTime;                            // 0x58  (DWARF :253)
    EventStartGridSlot maStartGridSlots[KU_MAX_START_GRID_SLOTS];  // 0x5C  (DWARF :256)  7 * 20 -> 0xE8
    u32                muStartGridCount;                           // 0xE8  (DWARF :259)  GetStartGridCount
    // DWARF BrnRaceEventData.h:262/265 (source lines 608/609): mu8Mode then mu8OnlineMode, the
    // first two of the eleven-byte run mu8Mode..mi8UnlockRank (+0xEC..+0xF6) that closes the 0xF8
    // record over one alignment byte at +0xF7 -- the same run progression_transcode.py models as
    // `(0xEC, 1, 11)  # mu8Mode .. mi8UnlockRank (0xF7 pad)`. GROWN IN PLACE 2026-08-26: the next
    // two of that run, mu8StartRivalCount (DWARF :268) and mu8AddRivalCount (DWARF :271), are now
    // carved because RaceMode::Start links against their accessors -- see the accessor banner
    // above for the asm that pins both bytes. The SEVEN still inside maPad_F0 are, in DWARF order,
    // mu8TakeDownBronze (+0xF0), mu8TakeDownSilver (+0xF1), mu8TakeDownGold (+0xF2),
    // mu8DamageLimit (+0xF3), mu8ExtensionTimeCount (+0xF4), mu8AStarType (+0xF5) and
    // mi8UnlockRank (+0xF6), with +0xF7 the closing alignment byte; carve one here when a caller
    // needs it. Do not fork this owner.
    u8  mu8Mode;                        // 0xEC  RaceEventData::EModeType       (GetMode)
    u8  mu8OnlineMode;                  // 0xED  RaceEventData::EOnlineModeType (GetOnlineMode)
    u8  mu8StartRivalCount;             // 0xEE  (DWARF :268)  GetStartRivalCount
    u8  mu8AddRivalCount;               // 0xEF  (DWARF :271)  GetAddRivalCount
    u8  maPad_F0[0x08];                 // 0xF0..0xF7 (sizeof == 0xF8)

    // ProgressionData's relocation walks the event table and rebases muaCheckpointsOffset.
    friend struct ProgressionData;
};

// The 248-byte (0xF8) stride ProgressionData::FixDown @0x8267F220 walks the event table with
// (`v8 += 248`), and the stride progression_transcode.py ports the retail table at.
static_assert(sizeof(RaceEventData) == 248, "BrnProgression::RaceEventData is a 248-byte (0xF8) record");

}

#endif // BRN_RACE_EVENT_DATA_H
