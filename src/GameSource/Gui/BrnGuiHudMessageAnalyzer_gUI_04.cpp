#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"   // HudMessageAnalyzer + the KAPC_* rodata tables
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"         // GuiHudMessage (Construct / AddParam)
#include "GameSource/Gui/BrnGuiCache.h"                 // GuiCache accessors the six handlers read
#include "GameSource/GameState/BrnGameActions.h"        // BrnWorld::EPowerParkOutcome (E_PPO_SUCCESS / _FAILURE)

#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h" // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiHudMessage.h"  // CgsGui::E_HUDMESSAGEPARAMTYPES_*

// =====================================================================================
// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (gateui wave round-2 partfile 04, owner guihand2). Six more of the wave-B fan-out's
// declaration-only arms; each was an LNK2019 standing between this tree and a mountable
// HudMessageAnalyzer:
//
//   HandleBurningHomeRunRunnerCrashes     @0x8251B440  (114 insns, cpp:1925-1928)
//   HandleBurningHomeRunCheckpointReached @0x8251B1D0  ( 95 insns, cpp:1829-1832)
//   HandleEventLeaderSplitTime            @0x8251E640  ( 84 insns, cpp:3785)
//   HandlePowerParkingResult              @0x8251F330  ( 74 insns, cpp:4989/4998/5007)
//   HandleEventDistanceToFinish           @0x8251E550  ( 59 insns, cpp:3758/3759)
//   HandleRaceCheckpointReached           @0x8251B350  ( 59 insns, cpp:1882/1883)
//
// -------------------------------------------------------------------------------------
// [gateui r3] THE TWO PREPROCESSOR GATES ARE GONE -- these six bodies are LIVE.
// -------------------------------------------------------------------------------------
// Round 2 shipped this partfile behind `BRN_GUI_GUI04_PAYLOADS` /
// `BRN_GUI_GUI04_CACHE_MEMBERS` because all six handlers read members of GUI-event payload
// structs that were still opaque placeholders in a header round 2's ownership rules put in
// another owner's lane. Neither macro was ever defined anywhere in the tree, so the TU
// compiled to an EMPTY object: gate-green, six LNK2019s still open, nothing diagnosing it
// (verify_r2_guihand2/VERDICT.md finding 2).
//
// Round 3 landed both dependencies and deleted both `#if`s in the same change:
//   * the six payload structs now carry their real DWARF field sets in
//     BrnGuiEventTypeDefs.h (each with its own GetEventType() -- the member
//     CgsGui::GuiModule::AddGuiEvent<T> bakes -- plus the house sizeof/offsetof pins), and
//     their opaque twins were DELETED from BrnGuiDemangledEventTypes.h, so there is exactly
//     one definition of each name and no ODR split with CgsGuiModule_AddGuiEvent_Inst.cpp;
//   * GuiCache carries the two members HandleRaceCheckpointReached reads
//     (+0x4B04 mePlayerGlobalRaceCarIndex / +0x5286 muNumActiveLandmarks), carved out of
//     mPad_4B04 / mPad_5284 without shifting anything, with the two accessors used below.
// Prove it the way the verdict asked: `dumpbin /SYMBOLS` this object and look for six
// SECT-External `Handle*` rows. No row means the file went dark again.
// =====================================================================================

namespace BrnGui
{


namespace
{
    // X360 rodata flt_82013F90 (0x8251E610) -- the metres-to-kilometres factor the
    // distance-to-finish line bakes in. Same constant the round-1 KM boundary handler
    // uses; kept file-local per partfile, not hoisted into a shared header.
    const f32 KF_METRES_TO_KILOMETRES = 0.001f;

    // Power-parking rating bands: the console divides the 0..99 overall rating by a
    // literal 20 (`li r10,0x14; divw` @0x8251F380) and asserts the resulting index is
    // below 6 ("luRatingIndex<6", cpp:4998).
    const s32 KI_POWER_PARK_RATING_BAND_SIZE = 20;
    const u32 KU_POWER_PARK_RATING_COUNT     = 6;
}

// -------------------------------------------------------------------------------------
// @ 0x8251B1D0
// Burning-Home-Run "landmark reached" chatter. Two message families: "your" lines when
// the event's car is the local player's, "their" lines otherwise; within each family the
// "...Lmk1" variant is the singular ("1 landmark to go") and "...LdMk" the plural.
//
// The remote branch resolves the crasher's online name FIRST and appends it as STRING
// parameter of display string 0; if the name did NOT resolve the console abandons the
// whole message (`lbz var_380; beq` @0x8251B318 skips both the count parameter and
// TriggerMessage). The local branch has no name to resolve and drops straight into the
// shared tail -- which is why the compiler shares one epilogue between them.
//
// PITFALL (round-1's, honoured here): GetOnlineName is overloaded. The console calls the
// EActiveRaceCarIndex overload @0x824ECE10 (`lwz r4,0(r30)` = meActiveRaceCarIndex), so
// the argument must stay enum-typed -- an s32 would silently bind the player-id overload.
// -------------------------------------------------------------------------------------
void HudMessageAnalyzer::HandleBurningHomeRunCheckpointReached(
        const GuiBHRCheckpointReachedEvent* lpEvent)
{
    // Non-gating tripwires (cpp:1829-1832, in the console's order).
    CGS_ASSERT(lpEvent != NULL, "lpEvent");
    CGS_ASSERT(lpEvent->meActiveRaceCarIndex >= 0, "lpEvent->meActiveRaceCarIndex >= 0");
    CGS_ASSERT(lpEvent->meActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lpEvent->meActiveRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    GuiHudMessage lMessage;

    if (static_cast<s32>(lpEvent->meActiveRaceCarIndex) == mpGuiCache->GetPlayerActiveRaceCarIndex())
    {
        lMessage.Construct(lpEvent->miNumCheckpointsToGo == 1 ? "OnlBHRYrLmk1" : "OnlBHRYrLdMk");
    }
    else
    {
        lMessage.Construct(lpEvent->miNumCheckpointsToGo == 1 ? "OnlBHRTrLmk1" : "OnlBHRTrLdMk");

        bool lbNameValid = true;   // the X360 seeds the out-flag true before the call
        const char* lpcOnlineName = GetOnlineName(lpEvent->meActiveRaceCarIndex, &lbNameValid);
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, lpcOnlineName);

        if (!lbNameValid)
            return;
    }

    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0, lpEvent->miNumCheckpointsToGo);
    TriggerMessage(&lMessage);
}

// -------------------------------------------------------------------------------------
// @ 0x8251B440
// The Burning-Home-Run runner's crash budget ("N crashes left"). Same local/remote and
// singular/plural split as the landmark handler above, but the plural line carries the
// count as an INT parameter of display string 1 while the singular line carries none.
//
// FLAG (console defect, reproduced verbatim): NEITHER branch has an arm for
// miNumCrashesToGo <= 0. The compiler proves it -- @0x8251B550 `bne cr6, loc_8251B5F4`
// jumps a zero/negative count straight to TriggerMessage, and @0x8251B5A8 `bne cr6,
// loc_8251B5BC` does the same on the remote side -- so the message is triggered with an
// UNCONSTRUCTED GuiHudMessage (a stack-garbage id hash and garbage parameter counts).
// This is the shipped shape of the source (an if/else-if with no else, followed by an
// unconditional TriggerMessage); do not "fix" it by adding a guard. On the host that
// means the same uninitialised read, so if the BHR mode ever posts a zero count the
// director will look up a garbage hash -- worth a probe when BHR first runs, not a
// silent divergence here.
// -------------------------------------------------------------------------------------
void HudMessageAnalyzer::HandleBurningHomeRunRunnerCrashes(
        const GuiHUDMessageBHRRunnerCrashed* lpEvent)
{
    // Non-gating tripwires (cpp:1925-1928).
    CGS_ASSERT(lpEvent != NULL, "lpEvent");
    CGS_ASSERT(lpEvent->meActiveRaceCarIndex >= 0, "lpEvent->meActiveRaceCarIndex >= 0");
    CGS_ASSERT(lpEvent->meActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lpEvent->meActiveRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    GuiHudMessage lMessage;   // FLAG: deliberately left unconstructed on the <= 0 arms

    if (static_cast<s32>(lpEvent->meActiveRaceCarIndex) == mpGuiCache->GetPlayerActiveRaceCarIndex())
    {
        if (lpEvent->miNumCrashesToGo > 1)
        {
            lMessage.Construct("OnlBHRCrLp2");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 1, lpEvent->miNumCrashesToGo);
        }
        else if (lpEvent->miNumCrashesToGo == 1)
        {
            lMessage.Construct("OnlBHRCrLp1");
        }

        TriggerMessage(&lMessage);
    }
    else
    {
        if (lpEvent->miNumCrashesToGo > 1)
        {
            lMessage.Construct("OnlBHRCrRp2");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 1, lpEvent->miNumCrashesToGo);
        }
        else if (lpEvent->miNumCrashesToGo == 1)
        {
            lMessage.Construct("OnlBHRCrRp1");
        }

        bool lbNameValid = true;
        const char* lpcOnlineName = GetOnlineName(lpEvent->meActiveRaceCarIndex, &lbNameValid);
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1, lpcOnlineName);

        if (lbNameValid)
            TriggerMessage(&lMessage);
    }
}

// -------------------------------------------------------------------------------------
// @ 0x8251E640
// The in-event split-time / checkpoint line. Three shapes off one gate:
//   offline  -> "EventCheckPt" + (checkpoints reached, checkpoints in event)
//   online, local player leading -> "EventSpltYou" + the lead time
//   online, a rival leading      -> "EventSpltRvl" + the leader's gamer tag
//
// FLAG (console oddity, reproduced verbatim): the RIVAL arm never fires its message. The
// X360 constructs a SECOND, separate GuiHudMessage (stack slot var_360, distinct from the
// var_6B0 the other two arms share), hands it to AddGamerTagToMessage, and then branches
// straight to the epilogue -- `bl AddGamerTagToMessage` @0x8251E6C4 is followed by
// `b loc_8251E778` @0x8251E6C8, with no TriggerMessage anywhere on that path. Two distinct
// stack objects is the tell that the source really did declare a second local here rather
// than the decompiler aliasing one; the missing trigger is a shipped bug, not a transcription
// slip. Kept as-is, with the second local kept distinct so the shape stays legible.
//
// The lead time rides as an INT: `lfs f0,8(lpEvent); fctiwz; stfiwx` @0x8251E6E0 truncates
// toward zero before the INT AddParam -- it is not a FLOAT parameter.
//
// FLAG (naming, out of this lane -- see the report): the online gate is the byte at
// GuiCache +0x4B4C, which this tree currently names mbOnlineStartInProgress /
// IsOnlineStartInProgress(). Both this handler and HandleRaceCheckpointReached read it as
// a plain "this is an online event" discriminator, which that name does not describe.
// Coded against the existing accessor; the rename is a request, not an edit.
// -------------------------------------------------------------------------------------
void HudMessageAnalyzer::HandleEventLeaderSplitTime(const GuiInEventLeaderSplit* lpEvent) const
{
    CGS_ASSERT(lpEvent != NULL, "lpEvent");   // cpp:3785

    GuiHudMessage lMessage;
    s32 liSecondParam;

    if (mpGuiCache->IsOnlineStartInProgress())
    {
        if (!lpEvent->mbLocalPlayerIsLeading)
        {
            GuiHudMessage lRivalMessage;
            lRivalMessage.Construct("EventSpltRvl");
            AddGamerTagToMessage(&lRivalMessage, "TEMP_ONLINE_NAME",
                                 lpEvent->meLeaderActiveRaceCarIndex);
            return;   // FLAG: the console never triggers this one -- see above.
        }

        lMessage.Construct("EventSpltYou");
        liSecondParam = static_cast<s32>(lpEvent->mfLeadTime);
    }
    else
    {
        lMessage.Construct("EventCheckPt");
        // The console inlines GetCheckpointReached here (its "0 <= miCheckpointReached"
        // assert, BrnGuiCache.h:2927, is folded into this body at 0x8251E714); the
        // out-of-line accessor is the same function, so call it.
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0,
                          mpGuiCache->GetCheckpointReached());
        liSecondParam = mpGuiCache->GetCheckpointsInEvent();   // u8 return, zero-extended
    }

    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0, liSecondParam);
    TriggerMessage(&lMessage);
}

// -------------------------------------------------------------------------------------
// @ 0x8251F330
// The power-parking (stunt-park) result popup. On success the overall rating picks one of
// six rating-name string ids by NUMERIC SUFFIX -- the console formats the id at run time
// with SPrintf rather than indexing a table, so there is no rodata table to recover here.
// Both the numeric rating and the formatted rating-name string id ride display string 1.
// On failure a single line, no parameters.
//
// The rating index is a SIGNED divide (`divw`) compared UNSIGNED (`cmplwi`) -- a negative
// rating would wrap past the assert rather than trip it. Reproduced by dividing as s32 and
// holding the result in a u32, which is what the original expression did.
// -------------------------------------------------------------------------------------
void HudMessageAnalyzer::HandlePowerParkingResult(const GuiPowerParkResult* lpEvent)
{
    CGS_ASSERT(lpEvent != NULL, "lpEvent");   // cpp:4989

    GuiHudMessage lMessage;

    if (lpEvent->meOutcome == BrnWorld::E_PPO_SUCCESS)
    {
        const u32 luRatingIndex = lpEvent->miOverallRating / KI_POWER_PARK_RATING_BAND_SIZE;
        CGS_ASSERT(luRatingIndex < KU_POWER_PARK_RATING_COUNT, "luRatingIndex<6");  // cpp:4998

        char lacRatingStringID[32];
        CgsCore::SPrintf(lacRatingStringID, sizeof(lacRatingStringID),
                         "HUDMESSAGE_POWERPARK_RATING%d", luRatingIndex);

        lMessage.Construct("PowPkWin");
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 1, lpEvent->miOverallRating);
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1, lacRatingStringID);
        TriggerMessage(&lMessage);
    }
    else
    {
        CGS_ASSERT(lpEvent->meOutcome == BrnWorld::E_PPO_FAILURE,
                   "lpEvent->meOutcome == BrnWorld::E_PPO_FAILURE");   // cpp:5007

        lMessage.Construct("PowPkLose");
        TriggerMessage(&lMessage);
    }
}

// -------------------------------------------------------------------------------------
// @ 0x8251E550
// "<Nth> -- N km from the finish". The ordinal comes from the already-recovered
// KAPC_FINISH_POSITION_MESSAGES table (X360 off_82F277E0, defined in
// BrnGuiHudMessageAnalyzer_wB_res.cpp): the console loads `lwzx r6, r10, r11` with r11
// biased by -4 (`lwz r6,-4(r11)` @0x8251E5F0), i.e. it indexes the table with
// miPlayerPosition - 1 -- a 1-based position against a 0-based table. Note the sibling
// HandleEliminatedEvent indexes the SAME table from slot 1 instead; the two conventions
// are genuinely different, do not unify them.
//
// The distance is metres, converted and TRUNCATED to an INT parameter (`fmuls` then
// `fctiwz` @0x8251E614), not passed as a FLOAT parameter.
// -------------------------------------------------------------------------------------
void HudMessageAnalyzer::HandleEventDistanceToFinish(const GuiInEventDistanceToFinish* lpEvent) const
{
    // Non-gating tripwires (cpp:3758/3759; the second is a single combined assert on the
    // console, message text verbatim including its spacing).
    CGS_ASSERT(lpEvent != NULL, "lpEvent");
    CGS_ASSERT((0 < lpEvent->miPlayerPosition) &&
               (E_ACTIVE_RACE_CAR_INDEX_COUNT >= lpEvent->miPlayerPosition),
               "(0 < lpEvent->miPlayerPosition) && ( E_ACTIVE_RACE_CAR_INDEX_COUNT >= lpEvent->miPlayerPosition)");

    GuiHudMessage lMessage;
    lMessage.Construct("EventDistFin");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 0,
                      KAPC_FINISH_POSITION_MESSAGES[lpEvent->miPlayerPosition - 1]);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0,
                      static_cast<s32>(lpEvent->mfDistanceToFinish * KF_METRES_TO_KILOMETRES));
    TriggerMessage(&lMessage);
}


// -------------------------------------------------------------------------------------
// @ 0x8251B350
// The online-race "you passed landmark N of M" line. Three gates, all of which must hold
// before anything is shown (the console has no else arm at all):
//   * the event is online                            (GuiCache +0x4B4C)
//   * the event's GLOBAL car index is the local player's (GuiCache +0x4B04 -- NOT the
//     active index the BHR handlers compare; GuiCache::Construct seeds both to -1 and
//     GuiCache::RecEvent writes them as a pair from one event record)
//   * the checkpoint index is below the active-landmark count (GuiCache +0x5286, the byte
//     GuiCache::HandleSetActiveLandmarksEvent @0x824EE7D0 stores alongside the u16
//     landmark array at +0x5288)
// The displayed index is 1-based (`addi r6, r11, 1` @0x8251B400) over a 0-based member.
//
// [gateui r3] UNBLOCKED: both members are carved into BrnGuiCache.h this round
// (mePlayerGlobalRaceCarIndex / muNumActiveLandmarks) with the two accessors this body
// already called, so it now compiles against the real header set like its five siblings.
// -------------------------------------------------------------------------------------
void HudMessageAnalyzer::HandleRaceCheckpointReached(const GuiRaceCheckpointReached* lpEvent)
{
    // Non-gating tripwires (cpp:1882/1883 -- note the cache assert comes FIRST here,
    // the reverse of every other handler in this partfile).
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");
    CGS_ASSERT(lpEvent != NULL, "lpEvent");

    if (!mpGuiCache->IsOnlineStartInProgress())
        return;

    if (lpEvent->meGlobalRaceCarIndex != mpGuiCache->GetPlayerGlobalRaceCarIndex())
        return;

    if (lpEvent->miCheckpointIndex >= mpGuiCache->GetNumActiveLandmarks())
        return;

    GuiHudMessage lMessage;
    lMessage.Construct("OnlRacTrLdMk");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0, lpEvent->miCheckpointIndex + 1);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0,
                      mpGuiCache->GetNumActiveLandmarks());
    TriggerMessage(&lMessage);
}



} // namespace BrnGui
