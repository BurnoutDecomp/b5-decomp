// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.cpp
// ============================================================================
// BrnGameState::ChallengeManager -- the top-level CHALLENGE-mode manager (FILE/CLASS TU,
// 57 functions in the X360 ARTIST build). This is a faithful FOUNDATION: the type is homed
// as OPAQUE attested-offset storage (see BrnChallengeManager.h for why), and the methods that
// the X360 asm reaches purely through `this`-offset arithmetic (plus already-recovered free
// functions / homed accessors) are bodied. The remainder -- the network-reaching methods, the
// per-frame skill / arbitration / action VMX pipelines, and methods that raw-index the unhomed
// nested aggregates -- are DECLARATION-ONLY and resolve in later slices.
//
// BODIED (this TU):
//   Construct, UpdateResultsTimer, ReceivedSuccessUpdatesFromAllPlayers,
//   GetNumPlayerSucceeding, GetNumPlayersContributing,
//   GetFreeburnChallengeList, GetProgressionManager, GetLocalChallengeCompletionData,
//   GetChallengeStyle, CountCompletedChallenges, UpdateStuntScores.
//
// BLOCKED (NOT bodied): ResetActionData @0x823246F0 -- its per-action reset dispatch indexes an
//   un-recovered 41-entry rodata lookup table (dword_82021288: EChallengeActionType -> skill index,
//   with the 38==skip / 17==special cases) that is not present in the dossier or the committed tree.
//   Reconstructing it would require fabricating the table's 41 mapping values, so it stays for the
//   slice that recovers that rodata.
//
// DECLARATION-ONLY (FLAGGED, own slices): the other 52 functions in the postmortem (Update*,
// Handle*, NetworkPlayer*, Check*, BeginChallenge/EndChallenge/Trigger/Cancel, ProcessEvent,
// WriteDataToOutput*, the timer-arm helpers that call ChallengeListEntry methods, etc.).
//
// ATTESTED OFFSETS (X360 `*(this + N)`; read/written by the bodied methods above):
//   +0x428 (1064)  maaePlayersSuccessStatus  -- s32[player][2] success-status grid; the
//                  per-player success enum (0..3). GetNumPlayerSucceeding/Contributing index it
//                  as *(this + 0x428 + 8*player + 4*action). (X360: 4*(action + 0x10A) + 8*player.)
//   +0x590 (1424)  mabReceivedSuccessUpdates -- bool[8]; one received-update flag per player.
//   +0xD00 (3328)  mLocalChallengeCompletionData -- FastBitArray<2000> completion bit store.
//   +0xE0C (3596)  mpCurrentChallenge        -- const ChallengeListEntry*; the active challenge.
//   +0xE28 (3624)  mfResultsTimer            -- f32; the post-challenge results countdown.
//   +0xE2C (3628)  mbResultsTimerRunning     -- bool; latched once the timer is armed.
//   +0xE30 (3632)  meChallengeManagerStatus  -- s32 EChallengeManagerStatus; ==1 (PENDING) picks
//                  the 5.5s results time-out, else 0.0.
//   +0xE38 (3640)  mpFreeburnChallengeList   -- const ChallengeList*.
//   +0xE44 (3652)  mpProgression             -- ProgressionManager*.
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                  // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"                      // CgsContainers::FastBitArray<2000>
#include "SharedClasses/DataLists/ChallengeListEntry.h"                            // BrnResource::ChallengeListEntry::GetNumPlayers
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // CgsDev::DebugComponent (Construct registers the debug component)

#include <cstddef>   // offsetof (layout assert)

namespace BrnResource    { class ChallengeList; }
namespace BrnProgression { class ProgressionManager; }

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// File-scope constants the bodied methods use. KF_RESULTS_TIME_OUT is the DWARF-named
// const (BrnChallengeManager.cpp:35); the X360 immediate is 5.5f (flt_8202127C).
// ----------------------------------------------------------------------------
static const f32 KF_RESULTS_TIME_OUT = 5.5f;

// The two "make all challenges N-player" debug toggles (DWARF `extern bool` statics; X360
// backs them with byte_82FAD9C0/C1). Default-cleared, exactly as Construct re-clears the globals.
bool ChallengeManager::mbChallengesAreAllOnePlayer = false;
bool ChallengeManager::mbChallengesAreAllTwoPlayer = false;

// ----------------------------------------------------------------------------
// Opaque-storage byte-offset helpers (the opaque-external-field-by-attested-offset
// allowance). NOTHING here fabricates a named member; each helper is a typed view of one
// X360-attested byte offset into maOpaque.
// ----------------------------------------------------------------------------
namespace
{
    template <typename T>
    inline T& FieldAt(u8* lpStorage, u32 luOffset)
    {
        return *reinterpret_cast<T*>(lpStorage + luOffset);
    }
    template <typename T>
    inline const T& FieldAt(const u8* lpStorage, u32 luOffset)
    {
        return *reinterpret_cast<const T*>(lpStorage + luOffset);
    }
}

// ----------------------------------------------------------------------------
// Construct -- X360 0x82332DB0. EXECUTED in the boot-trace milestone.
//
// FAITHFUL FOUNDATION NOTE: the X360 body zero/initialises a wide span of per-frame and
// per-player scratch state (the FastBitArray success masks, the per-player score arrays, the
// leaping-data pool, the timers/flags), runs ClearPlayerSuccessData + ClearFreeburnSkillsThisFrame,
// asserts the five injected back-pointers are non-null and caches them, then registers the
// embedded debug component. Of those, only the back-pointer asserts + caches and the timer/flag
// resets reduce to clean `this`-offset writes; the bulk-zeroing loops sweep the unhomed nested
// aggregates (success masks / pools) whose interior strides are not reconstructable here, and the
// two clear helpers + DebugComponent::Register reach declaration-only / live-object machinery.
// To avoid fabricating aggregate strides, Construct here reproduces the X360's exact scalar field
// initialisation over the attested offsets and FLAGS the aggregate-sweep + debug-registration as
// the parts that land with the dependent slices. The whole opaque buffer is zeroed first so no
// field is left indeterminate (a superset of the X360's per-field zeroing -- no behaviour added).
// ----------------------------------------------------------------------------
void ChallengeManager::Construct(void* lpGameStateModule,
                                 void* lpModeManager,
                                 void* lpProgression,
                                 void* lpRoadRulesManager,
                                 void* lpTriggerQueryManager)
{
    // Zero the whole object (superset of the X360 per-field `li r30,0; st... r30` clears).
    for (u32 luByte = 0; luByte < KU_CHALLENGE_MANAGER_SIZE; ++luByte)
    {
        maOpaque[luByte] = 0;
    }

    // Scalar field resets that map to single attested offsets (X360: timers 0.0, the results
    // status/run flag cleared, the "-1" sentinels). FLAG: the wide per-player / FastBitArray /
    // ObjectPool sweeps and the two clear-helper calls (ClearPlayerSuccessData,
    // ClearFreeburnSkillsThisFrame) are decl-only here; they land with their own slices.
    FieldAt<f32>(maOpaque, 0xE18) = 0.0f;   // X360 stfs f31,0xE18
    FieldAt<f32>(maOpaque, 0xE20) = 0.0f;   // X360 stfs f31,0xE20
    FieldAt<f32>(maOpaque, 0xE28) = 0.0f;   // X360 stfs f31,0xE28 (mfResultsTimer)
    FieldAt<f32>(maOpaque, 0x110) = -1.0f;  // X360 stfs flt_820037C8(-1.0),0x110
    FieldAt<s32>(maOpaque, 0x1000) = -1;    // X360 stw r7(-1),0x1000

    // Back-pointer non-null asserts + caches (X360 message strings + BrnChallengeManager.cpp lines).
    CGS_ASSERT(lpProgression != nullptr, "lpProgression");
    CGS_ASSERT(lpRoadRulesManager != nullptr, "lpRoadRulesManager");
    CGS_ASSERT(lpTriggerQueryManager != nullptr, "lpTriggerQueryManager");
    CGS_ASSERT(lpGameStateModule != nullptr, "lpGameStateModule");
    CGS_ASSERT(lpModeManager != nullptr, "lpModeManager");

    FieldAt<void*>(maOpaque, 0xE48) = lpGameStateModule;      // X360 stw r27,0xE48
    FieldAt<void*>(maOpaque, 0xE4C) = lpModeManager;          // X360 stw r26,0xE4C
    FieldAt<void*>(maOpaque, 0xE44) = lpProgression;          // X360 stw r28,0xE44 (mpProgression)
    FieldAt<void*>(maOpaque, 0xE3C) = lpRoadRulesManager;     // X360 stw r25,0xE3C
    FieldAt<void*>(maOpaque, 0xE40) = lpTriggerQueryManager;  // X360 stw r24,0xE40

    // Re-clear the two debug "make all challenges N-player" globals (X360 stb r11(0),byte_82FAD9C0/C1).
    mbChallengesAreAllOnePlayer = false;
    mbChallengesAreAllTwoPlayer = false;

    // FLAG (decl-only): X360 sets up the embedded ChallengeManagerDebugComponent at this+0x1004
    // (its back-pointer @+0x1010 = this, flags @+0x1014/+0x1018 = 0) and calls
    // CgsDev::DebugComponent::Register on it. In the opaque-storage model the embedded component is
    // not a live typed object (its vtable/base would need x64-correct placement), so the registration
    // is left to the slice that homes the component as a real member. NOT fabricated here.
}

// ----------------------------------------------------------------------------
// UpdateResultsTimer -- X360 0x82323408. Arms the results countdown on first call
// (5.5s when meChallengeManagerStatus==PENDING(1), else 0.0), then counts it down one frame and
// returns true once it has expired. The X360 uses fsel to clamp the decremented value at 0.0.
// ----------------------------------------------------------------------------
bool ChallengeManager::UpdateResultsTimer(f32 lfTimeStep)
{
    if (!FieldAt<bool>(maOpaque, 0xE2C))              // !mbResultsTimerRunning
    {
        if (FieldAt<s32>(maOpaque, 0xE30) == 1)       // meChallengeManagerStatus == PENDING
        {
            FieldAt<f32>(maOpaque, 0xE28) = KF_RESULTS_TIME_OUT;  // 5.5
        }
        else
        {
            FieldAt<f32>(maOpaque, 0xE28) = 0.0f;
        }
        FieldAt<bool>(maOpaque, 0xE2C) = true;
    }

    const bool lbWasRunning = FieldAt<bool>(maOpaque, 0xE2C);
    if (!lbWasRunning)
    {
        return true;   // X360 LABEL_11 path: v5==0 -> v5==0 returns 1
    }

    if (FieldAt<f32>(maOpaque, 0xE28) > 0.0f)
    {
        // f0 = timer - dt; fsel(-(timer-dt), 0.0, f0) clamps the result at 0.0 (X360 fsel f0,f12,f13,f0).
        const f32 lfNew = FieldAt<f32>(maOpaque, 0xE28) - lfTimeStep;
        FieldAt<f32>(maOpaque, 0xE28) = (lfNew >= 0.0f) ? lfNew : 0.0f;
    }

    // Expired iff the timer is running AND has reached <= 0.0 (X360: v8=0 unless still >0.0).
    if (FieldAt<f32>(maOpaque, 0xE28) > 0.0f)
    {
        return false;
    }
    return true;
}

// ----------------------------------------------------------------------------
// ReceivedSuccessUpdatesFromAllPlayers -- X360 0x82316DE8. True once the count of per-player
// received-update flags (mabReceivedSuccessUpdates[0..7]) equals the current challenge's player
// count. Asserts the current challenge is set and that the count never exceeds the player count.
// ----------------------------------------------------------------------------
bool ChallengeManager::ReceivedSuccessUpdatesFromAllPlayers()
{
    const BrnResource::ChallengeListEntry* lpCurrentChallenge =
        FieldAt<const BrnResource::ChallengeListEntry*>(maOpaque, 0xE0C);
    CGS_ASSERT(lpCurrentChallenge != nullptr, "mpCurrentChallenge");

    s32 liNumUpdatesReceived = (FieldAt<bool>(maOpaque, 0x590) != 0) ? 1 : 0;
    for (u32 luPlayer = 1; luPlayer < 8; ++luPlayer)
    {
        if (FieldAt<bool>(maOpaque, 0x590 + luPlayer))
        {
            ++liNumUpdatesReceived;
        }
    }

    const s32 liNumPlayers = lpCurrentChallenge->GetNumPlayers();
    CGS_ASSERT(liNumUpdatesReceived <= liNumPlayers,
               "liNumUpdatesReceived <= mpCurrentChallenge->GetNumPlayers()");

    return liNumPlayers == liNumUpdatesReceived;
}

// ----------------------------------------------------------------------------
// GetNumPlayerSucceeding -- X360 0x82316F00. Tally the players whose per-action success status
// is SUCCEEDING (==3) for the given action slot. The X360 indexes the success-status grid as
// *(this + 4*(action + 0x10A) + 8*player) i.e. base 0x428, stride 8 per player, 4 per action.
// ----------------------------------------------------------------------------
s32 ChallengeManager::GetNumPlayerSucceeding(s32 liActionIndex)
{
    s32 liNumPlayersSucceeding = 0;
    for (u32 luPlayer = 0; luPlayer < 8; ++luPlayer)
    {
        const u32 luOffset = 0x428u + 8u * luPlayer + 4u * static_cast<u32>(liActionIndex);
        if (FieldAt<s32>(maOpaque, luOffset) == 3)
        {
            ++liNumPlayersSucceeding;
        }
    }

    const BrnResource::ChallengeListEntry* lpCurrentChallenge =
        FieldAt<const BrnResource::ChallengeListEntry*>(maOpaque, 0xE0C);
    CGS_ASSERT(liNumPlayersSucceeding <= lpCurrentChallenge->GetNumPlayers(),
               "liNumPlayersSucceeding <= mpCurrentChallenge->GetNumPlayers()");

    return liNumPlayersSucceeding;
}

// ----------------------------------------------------------------------------
// GetNumPlayersContributing -- X360 0x82317020. As above but counts CONTRIBUTING (==2).
// ----------------------------------------------------------------------------
s32 ChallengeManager::GetNumPlayersContributing(s32 liActionIndex)
{
    s32 liNumPlayersContributing = 0;
    for (u32 luPlayer = 0; luPlayer < 8; ++luPlayer)
    {
        const u32 luOffset = 0x428u + 8u * luPlayer + 4u * static_cast<u32>(liActionIndex);
        if (FieldAt<s32>(maOpaque, luOffset) == 2)
        {
            ++liNumPlayersContributing;
        }
    }

    const BrnResource::ChallengeListEntry* lpCurrentChallenge =
        FieldAt<const BrnResource::ChallengeListEntry*>(maOpaque, 0xE0C);
    CGS_ASSERT(liNumPlayersContributing <= lpCurrentChallenge->GetNumPlayers(),
               "liNumPlayersContributing <= mpCurrentChallenge->GetNumPlayers()");

    return liNumPlayersContributing;
}

// ----------------------------------------------------------------------------
// Named accessors (X360-attested offsets; the committed ChallengeManagerDebugComponent uses them).
// ----------------------------------------------------------------------------
const BrnResource::ChallengeList* ChallengeManager::GetFreeburnChallengeList() const
{
    return FieldAt<const BrnResource::ChallengeList*>(maOpaque, 0xE38);  // *(this+0xE38)
}

BrnProgression::ProgressionManager* ChallengeManager::GetProgressionManager()
{
    return FieldAt<BrnProgression::ProgressionManager*>(maOpaque, 0xE44); // *(this+0xE44)
}

CgsContainers::FastBitArray<2000>& ChallengeManager::GetLocalChallengeCompletionData()
{
    return FieldAt<CgsContainers::FastBitArray<2000> >(maOpaque, 0xD00);  // this+0xD00
}

// ----------------------------------------------------------------------------
// GetChallengeStyle -- X360 0x82355FA8 (DWARF BrnChallengeManager.h:196). Returns the active
// challenge's freeburn style, or E_FREEBURN_STYLE_NONE unless the manager is RUNNING. The X360
// reads meChallengeManagerStatus @+0xE08 (the field immediately before mpCurrentChallenge @+0xE0C
// in the DWARF member order), compares it to RUNNING(2), asserts mpCurrentChallenge is set, and
// tail-calls ChallengeListEntry::GetChallengeStyle.
// ----------------------------------------------------------------------------
BrnResource::ChallengeListEntry::EFreeburnChallengeStyle ChallengeManager::GetChallengeStyle() const
{
    if (FieldAt<s32>(maOpaque, 0xE08) != E_CHALLENGE_MANAGER_STATUS_RUNNING)
    {
        return BrnResource::ChallengeListEntry::E_FREEBURN_STYLE_NONE;
    }

    const BrnResource::ChallengeListEntry* lpCurrentChallenge =
        FieldAt<const BrnResource::ChallengeListEntry*>(maOpaque, 0xE0C);
    CGS_ASSERT(lpCurrentChallenge != nullptr, "mpCurrentChallenge");

    return lpCurrentChallenge->GetChallengeStyle();
}

// ----------------------------------------------------------------------------
// CountCompletedChallenges -- X360 0x8233E530. Number of set bits in the local completion bit
// array (mLocalChallengeCompletionData @+0xD00). The X360 body is the container's set-bit iterator
// fully inlined: it walks 32 u64 fields (v1 bound 0x20) with a 2000-bit index bound (0x7D0), which
// is exactly FastBitArray<2000> (ceil(2000/64)==32 fields). The iterator lives entirely on the
// stack (the apparent `*HIDWORD(v33)=...` writes are stack var_140->var_B0 copies, not container
// mutations), so the function only reads the array. This is the value-identical
// GetFirstBitSet/GetNextBitSet scan over the same FastBitArray<2000> the accessor above exposes.
// ----------------------------------------------------------------------------
s32 ChallengeManager::CountCompletedChallenges()
{
    const CgsContainers::FastBitArray<2000>& lCompletion =
        FieldAt<CgsContainers::FastBitArray<2000> >(maOpaque, 0xD00);

    s32 liNumCompleted = 0;
    for (s32 liBit = lCompletion.GetFirstBitSet();
         liBit != CgsContainers::FastBitArray<2000>::KI_INVALID_BIT_INDEX;
         liBit = lCompletion.GetNextBitSet(liBit))
    {
        ++liNumCompleted;
    }
    return liNumCompleted;
}

// ----------------------------------------------------------------------------
// UpdateStuntScores -- X360 0x82334740. Push a completed stunt run's per-skill tallies into the
// current-frame freeburn skill scores: for each non-zero field of the incoming stunt-score struct,
// call SetCurrentSkillScore with the matching skill id and the field value (as f32).
//
// lpStuntScoreInfo is an un-homed external stunt-score snapshot handed in by PostWorldUpdate. It is
// not reconstructable as a named type here, so its fields are read by attested byte offset
// (external-data allowance). The X360 first block-copies [0x00..0x06] (7 bytes) then [0x08..0x13]
// (six u16s) into stack scratch before reading them back; that copy is a no-op for our purposes, so
// the fields are read directly at their source offsets: per-skill count bytes at [0x00..0x06], a
// 1-byte gap at [0x07], three u16 counts interleaved with bytes across [0x08..0x12], a s32
// air-distance value at [0x14], and a flag byte at [0x18]. Each non-zero field is pushed with
// lbScoredThisFrame==true, except the [0x14] branch which forwards the [0x18] flag.
//
// X360/PS3-DWARF DRIFT: the skill ids below (21..36) exceed the committed PS3 EFreeburnSkill enum
// (E_FREEBURN_SKILL_COUNT==19). The X360 build's EFreeburnSkill carries additional stunt-combo
// skills; the literal ids are attested verbatim in the X360 asm (li r4,0x15..0x24) and are cast to
// the enum without inventing names for the drifted enumerators.
// ----------------------------------------------------------------------------
void ChallengeManager::UpdateStuntScores(const void* lpStuntScoreInfo)
{
    CGS_ASSERT(lpStuntScoreInfo != nullptr, "lpStuntScoreInfo");

    const u8* lpInfo = static_cast<const u8*>(lpStuntScoreInfo);

    if (FieldAt<u8>(lpInfo, 0x06) != 0)
    {
        SetCurrentSkillScore(static_cast<EFreeburnSkill>(27), static_cast<f32>(FieldAt<u8>(lpInfo, 0x06)), true);
        if (FieldAt<u8>(lpInfo, 0x00) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(21), static_cast<f32>(FieldAt<u8>(lpInfo, 0x00)), true);
        if (FieldAt<u8>(lpInfo, 0x01) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(22), static_cast<f32>(FieldAt<u8>(lpInfo, 0x01)), true);
        if (FieldAt<u8>(lpInfo, 0x02) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(23), static_cast<f32>(FieldAt<u8>(lpInfo, 0x02)), true);
        if (FieldAt<u8>(lpInfo, 0x03) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(24), static_cast<f32>(FieldAt<u8>(lpInfo, 0x03)), true);
        if (FieldAt<u8>(lpInfo, 0x04) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(25), static_cast<f32>(FieldAt<u8>(lpInfo, 0x04)), true);
        if (FieldAt<u8>(lpInfo, 0x05) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(26), static_cast<f32>(FieldAt<u8>(lpInfo, 0x05)), true);
    }

    if (FieldAt<u8>(lpInfo, 0x12) != 0)
    {
        SetCurrentSkillScore(static_cast<EFreeburnSkill>(35), static_cast<f32>(FieldAt<u8>(lpInfo, 0x12)), true);
        if (FieldAt<u8>(lpInfo, 0x0E) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(28), static_cast<f32>(FieldAt<u8>(lpInfo, 0x0E)), true);
        if (FieldAt<u16>(lpInfo, 0x08) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(29), static_cast<f32>(FieldAt<u16>(lpInfo, 0x08)), true);
        if (FieldAt<u16>(lpInfo, 0x0A) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(30), static_cast<f32>(FieldAt<u16>(lpInfo, 0x0A)), true);
        if (FieldAt<u8>(lpInfo, 0x0F) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(31), static_cast<f32>(FieldAt<u8>(lpInfo, 0x0F)), true);
        if (FieldAt<u8>(lpInfo, 0x10) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(32), static_cast<f32>(FieldAt<u8>(lpInfo, 0x10)), true);
        if (FieldAt<u16>(lpInfo, 0x0C) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(33), static_cast<f32>(FieldAt<u16>(lpInfo, 0x0C)), true);
        if (FieldAt<u8>(lpInfo, 0x11) != 0)
            SetCurrentSkillScore(static_cast<EFreeburnSkill>(34), static_cast<f32>(FieldAt<u8>(lpInfo, 0x11)), true);
    }

    const s32 liAirDistance = FieldAt<s32>(lpInfo, 0x14);
    if (liAirDistance > 0)
    {
        SetCurrentSkillScore(static_cast<EFreeburnSkill>(36),
                             static_cast<f32>(liAirDistance),
                             FieldAt<u8>(lpInfo, 0x18) != 0);
    }
}

// ----------------------------------------------------------------------------
// _AssertLayout -- never called. Documents that the opaque buffer is sized above the highest
// bodied offset; on x64 the byte offsets the bodies use are PC-gate approximations of the X360
// layout (the type is opaque, so there is no member to offsetof against -- the size bound is the
// invariant the gate enforces).
// ----------------------------------------------------------------------------
void ChallengeManager::_AssertLayout()
{
    static_assert(KU_CHALLENGE_MANAGER_SIZE >= 0x101E,
                  "opaque storage must span past the highest X360-attested offset (+0x101D flag byte)");
    static_assert(offsetof(ChallengeManager, maOpaque) == 0,
                  "opaque storage is the sole layout member");
}

} // namespace BrnGameState
