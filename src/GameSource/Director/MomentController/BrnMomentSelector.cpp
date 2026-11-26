// Out-of-line bodies for BrnDirector::MomentSelector.
//
// Only the functions the console itself homed in BrnMomentSelector.cpp live here -- the
// six the console build's baked assert __FILE__/__LINE__ place in BrnMomentSelector.h
// (SetRecencyFactor / SelectBestMoment / SelectNewBestMoment / GetSelectedMoment /
// CancelSelection / SetMaxActiveMoments) are header inlines and are bodied in
// BrnMomentSelector.h, NOT here.
//
// Bodied in this TU:
//   MomentSelector::Construct     (no standalone console symbol -- the
//                              console inlines it; recovered from the copy inside
//                              ArbStateRoaming::Construct)
//   MomentSelector::Prepare
//   MomentSelector::Release
//   MomentSelector::AddMoment(MomentDescription)
//   MomentSelector::AddMoment(EType,EMomentParamID,f32,bool)
//   MomentSelector::PickBestInhibitedMoment / PickWorstUninhibitedMoment (FX-DIRECTOR2 2026-09-25, CC-12)
//
// Signature authority is the declarations for this exact file, cross-checked against the
// argument slots the console build actually uses at every call site (the automatic C
// translation's argument lists for all five are wrong: it renders AddMoment's single
// by-value 16-byte record as twelve scalars, and it prints Prepare/SelectBestMoment's
// reference args as ints).
//
// Local variable names are the declaration's own (luMomentCount / luLoop / lMomentDescription /
// luMomentsToNotInhibit / lMomentHandle / lDescription).
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Director/MomentController/BrnMomentSelector.cpp"
#include "GameSource/Director/MomentController/BrnMomentSelector.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [DIAG] one-shot jump-ladder rungs only
#include "GameSource/Director/MomentController/BrnMomentSelectorSelector.h"   // Selector<u32,10> (the random arm)
#include "GameSource/Director/MomentController/BrnMoment.h"                    // Moment (state/flags + GetName)
#include "GameSource/Director/MomentController/BrnMomentController.h"          // MomentController::MomentHandle
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.h"  // DebugPrinter
#include <cstdlib>                                                             // [DIAG] getenv (BRN_CRASHCAM_DIAG)

namespace BrnDirector
{

namespace
{
    // The two pickers' constants: the seed / starting score (flt_82001CC0 == 0) and the 1.0 the recency is taken
    // from (flt_82001C98 == 0x3F800000), read at 0x8221C15C / 0x8221C178 and 0x8221C48C / 0x8221C4A8.
    const f32 KF_PICK_ZERO = 0.0f;
    const f32 KF_PICK_ONE  = 1.0f;

}

// Construct --
//
// No standalone console symbol (fully inlined). ArbStateRoaming::Construct emits it
// verbatim over the embedded selector at +0x1B8:
//     0.0f  -> +0x1C4 (mfTimeActive)       0.0f -> +0x1CC (mfRecencyFactor)
//     0     -> +0x0A0 / +0x194 / +0x1C0   (the three Array<> count words == Construct())
//     0     -> +0x1C8 (miFramesActive)    0    -> +0x1D0 (muValidMoments)
//     0     -> +0x1DC (meSelectionMode == E_MODE_LRU_BEST)
//     false -> +0x1E1 (mbPrepared)        0    -> +0x1D4 (muMaxActiveMomentLimit)
//     false -> +0x1E2 (mbHasMaxLimit)
// mbHasSelectedMoment (+0x1E0) and miSelectedMoment (+0x1D8) are deliberately NOT written --
// the console leaves both uninitialised until the first SelectBestMoment*. Faithful: do the
// same. (Release() is what clears mbHasSelectedMoment.)
void MomentSelector::Construct()
{
    mMomentDescriptionArray.Construct();
    mMomentHandleArray.Construct();
    mRecencyArray.Construct();

    mfTimeActive           = 0.0f;
    miFramesActive         = 0;
    mfRecencyFactor        = 0.0f;
    muValidMoments         = 0;
    muMaxActiveMomentLimit = 0;
    meSelectionMode        = E_MODE_LRU_BEST;
    mbPrepared             = false;
    mbHasMaxLimit          = false;
}

// Prepare --
//
// Signature attested by the console build: this, a MomentController, a
// Camera::BehaviourManager. Both arguments are proved by the forwarding call, which passes
//     the MomentController, desc[0], desc[4], &mMomentHandleArray[i], the BehaviourManager
// -- which is exactly MomentController::NewMoment(EType, EMomentParamID, MomentHandle&,
// BehaviourManager&). It is confirmed from the caller side too: ArbStateRoaming::Prepare
// takes the MomentController from ArbStateSharedInfo +0x20 (mpMomentController) and the
// BehaviourManager from +0x18 (mpBehaviourManager). The declaration agrees:
// Prepare(MomentController&, BehaviourManager&).
//
// Returns mbPrepared (the +0x1E1 byte), which is cleared by any NewMoment failure.
bool MomentSelector::Prepare(MomentController& lrMomentController,
                             Camera::BehaviourManager& lrBehaviourManager)
{
    // Read once, before the mbPrepared test -- the console runs GetLength()'s
    // "Array used before Construct/Clear was called" assert unconditionally at entry.
    const u32 luMomentCount = mMomentDescriptionArray.GetLength();
    u32       luLoop;

    if (!mbPrepared)
    {
        mbPrepared = true;

        // Give every registered description a live moment. A NewMoment failure does NOT
        // abort the loop; it only drops the prepared flag (the console keeps iterating).
        for (luLoop = 0; luLoop < luMomentCount; ++luLoop)
        {
            if (!mMomentHandleArray[luLoop].IsAllocated())
            {
                const MomentDescription lMomentDescription = mMomentDescriptionArray[luLoop];

                if (!lrMomentController.NewMoment(lMomentDescription.meMomentType,
                                                  lMomentDescription.meMomentParamID,
                                                  mMomentHandleArray[luLoop],
                                                  lrBehaviourManager))
                {
                    mbPrepared = false;
                }
            }
        }

        // Max-active policy: walk the moments in order, letting the first
        // muMaxActiveMomentLimit inhibitable ones run, and inhibiting every inhibitable one
        // after that. The console duplicates the (IsAllocated && mbCanBeInhibited) test in
        // both arms rather than hoisting it; kept as-is.
        if (mbHasMaxLimit)
        {
            u32 luMomentsToNotInhibit = muMaxActiveMomentLimit;

            // ARTIST 0x82255DB0..0x82255DC0: a failed first test re-tests the count before the
            // inhibit arm, i.e. two conditions joined by else-if.
            for (luLoop = 0; luLoop < luMomentCount; ++luLoop)
            {
                if (luMomentsToNotInhibit > 0 && mMomentHandleArray[luLoop].IsAllocated() &&
                    mMomentDescriptionArray[luLoop].mbCanBeInhibited)
                {
                    --luMomentsToNotInhibit;
                }
                else
                {
                    if (luMomentsToNotInhibit == 0 && mMomentHandleArray[luLoop].IsAllocated() &&
                        mMomentDescriptionArray[luLoop].mbCanBeInhibited)
                    {
                        // The console inlines MomentHandle::GetMoment (with its
                        // "mbIsAllocated" assert) then Moment::Inhibit (raise
                        // mbIsInhibited; vtable +0x10 Release(); meState = SEARCHING).
                        mMomentHandleArray[luLoop].GetMoment().Inhibit();
                    }
                }
            }
        }
    }

    return mbPrepared;
}

// Update --    ⭐ NEW 2026-08-01
//
// Signature attested by the console build: this plus one float timestep, which is added
// to +0x1C4. Nothing is returned. The automatic C translation's
// `(_DWORD* result, double a2)` is the usual PPC float-ABI artefact.
//
// Advance the selector's accumulators, decay every candidate's recency score, and re-classify
// every live moment into four running counters -- one of which is muValidMoments, the count
// ArbStateRoaming::Update's DRIVING arm reads every frame to decide whether to ask for an
// establishing shot. Then inhibit any moment that is VALID but cannot be switched to.
//
// LOOP SHAPE, every branch attested:
//   skip the currently-selected slot;  mRecencyArray[i] *= mfRecencyFactor;
//   skip !IsAllocated();   then five ordered tests on the moment, four of which `continue`.
// The console re-fetches the handle and re-calls GetMoment() before EVERY one of those tests
// (nine separate `mbIsAllocated` tripwires in one loop body);
// hoisting them is the same reads in the same order, so this keeps one local per test group.
// SnoopNumValidMoments --
//
// ⭐ BODIED 2026-08-29 (crash-camera wave). It was declaration-only, which was fine only while
// nothing called it; ArbStateCrashing::Prepare calls it on its straight-line path
// ("this state has been active for fewer than 2 frames AND there is no valid moment => not
// prepared yet"), so it became an unresolved external the moment the crash camera mounted.
//
// It is the RE-COUNT, and the store-back is the point: it walks the whole handle array and
// writes the answer into muValidMoments (+0x1D0), which is the cached value
// GetNumValidMoments() and ArbStateRoaming's DRIVING arm read every frame. A moment counts when
// it is live AND valid AND currently switchable to:
//     for each registered description index i:
//         if (!mMomentHandleArray[i].IsAllocated()) continue;
//         if (mMomentHandleArray[i].GetMoment().GetState() != E_STATE_VALID) continue;
//         if ( mMomentHandleArray[i].GetMoment().CanSwitchToMeNow()) ++muValidMoments;
//
// ⚠️ The loop bound is the DESCRIPTION array's length (`*(this+160)` == +0xA0), not the handle
// array's -- the console runs the description array's own "Array used before Construct/Clear
// was called" tripwire before the loop, unconditionally, exactly as Prepare
// does. The console also re-fetches the handle and re-calls GetMoment() before each of the two
// tests (two separate `mbIsAllocated` tripwires per iteration); the
// re-fetch is the same read in the same order, so it is hoisted to one local per iteration.
u32 MomentSelector::SnoopNumValidMoments()
{
    const u32 luMomentCount = mMomentDescriptionArray.GetLength();   // +0x0A0, asserted at entry

    muValidMoments = 0;                                              // +0x1D0 -- reset before the walk

    // DWARF (cpp:255): three handle operator[] calls and two GetMoment()s per pass; ARTIST
    // 0x8221BD28 re-fetches the handle before each test.
    for (u32 luLoop = 0; luLoop < luMomentCount; ++luLoop)
    {
        if (mMomentHandleArray[luLoop].IsAllocated())
        {
            if (mMomentHandleArray[luLoop].GetMoment().GetState() == Moment::E_STATE_VALID)
            {
                if (mMomentHandleArray[luLoop].GetMoment().CanSwitchToMeNow())
                {
                    ++muValidMoments;
                }
            }
        }
    }

    return muValidMoments;
}

void MomentSelector::Update(f32 lfTimestep)
{
    CGS_ASSERT_AT(mbPrepared, "mbPrepared", CGS_ARTIST_FILE, 117);

    mfTimeActive += lfTimestep;      // +0x1C4
    ++miFramesActive;                // +0x1C8

    // DWARF locals cpp:123..128.
    const s32 liMomentCount = mMomentDescriptionArray.GetLength();   // +0x0A0
    s32 liLoop;
    u32 luInvalidMoments          = 0;
    u32 luPotentiallyValidMoments = 0;
    u32 luUninhibitedMoments      = 0;

    muValidMoments = 0;              // +0x1D0 -- recounted from scratch every frame

    // ARTIST 0x82239FC0 re-fetches the handle and its moment before every test (one
    // "mbIsAllocated" tripwire each); the classification is an else-if chain.
    for (liLoop = 0; liLoop < liMomentCount; ++liLoop)
    {
        if (!mbHasSelectedMoment || liLoop != miSelectedMoment)
        {
            mRecencyArray[liLoop] *= mfRecencyFactor;   // +0x198[i] *= +0x1CC

            if (mMomentHandleArray[liLoop].IsAllocated())
            {
                if (!mMomentHandleArray[liLoop].GetMoment().IsInhibited() &&
                    mMomentDescriptionArray[liLoop].mbCanBeInhibited)
                {
                    ++luUninhibitedMoments;
                }

                if (mMomentHandleArray[liLoop].GetMoment().GetState() == Moment::E_STATE_VALID &&
                    mMomentHandleArray[liLoop].GetMoment().CanSwitchToMeNow())
                {
                    ++muValidMoments;
                }
                else if (!mMomentHandleArray[liLoop].GetMoment().ConditionsAreMet() &&
                         !mMomentHandleArray[liLoop].GetMoment().IsInhibited() &&
                         mMomentDescriptionArray[liLoop].mbCanBeInhibited)
                {
                    ++luInvalidMoments;
                }
                else if (mMomentHandleArray[liLoop].GetMoment().ConditionsAreMet() &&
                         mMomentHandleArray[liLoop].GetMoment().IsInhibited())
                {
                    ++luPotentiallyValidMoments;
                }
                else if (mMomentHandleArray[liLoop].GetMoment().GetState() == Moment::E_STATE_VALID &&
                         !mMomentHandleArray[liLoop].GetMoment().CanSwitchToMeNow())
                {
                    mMomentHandleArray[liLoop].GetMoment().Inhibit();

                    // A moment the description says may not be inhibited is inhibited anyway
                    // (Release and the state change stand) and only its flag is cleared again.
                    if (!mMomentDescriptionArray[liLoop].mbCanBeInhibited)
                    {
                        mMomentHandleArray[liLoop].GetMoment().Uninhibit();
                        CGS_ASSERT_AT(!mMomentHandleArray[liLoop].GetMoment().IsInhibited(),
                                      "!mMomentHandleArray[liLoop].GetMoment().IsInhibited()",
                                      CGS_ARTIST_FILE, 167);
                        --luUninhibitedMoments;
                    }
                }
            }
        }
    }

    // ---- The max-active-moments rebalance (0x8223A41C..0x8223A660) ----------------------------
    // ArbStateCrashing::Construct @0x82259EA0 sets a limit of one, so every crash runs this.
    if (mbHasMaxLimit && luPotentiallyValidMoments > 0)
    {
        if (luUninhibitedMoments < muMaxActiveMomentLimit)
        {
            // ARTIST 0x8223A43C..0x8223A52C: each loop re-tests the budget at its top and leaves
            // through the pick; the held-back count guards all three loops.
            while (luPotentiallyValidMoments > 0 && luUninhibitedMoments < muMaxActiveMomentLimit)
            {
                u32 luMomentToUninhibit;   // DWARF cpp:179
                if (!PickBestInhibitedMoment(&luMomentToUninhibit, E_PICK_BEST_FUSSY))
                {
                    break;
                }
                mMomentHandleArray[luMomentToUninhibit].GetMoment().Uninhibit();
                --luPotentiallyValidMoments;
                ++luUninhibitedMoments;
            }
            while (luPotentiallyValidMoments > 0 && luUninhibitedMoments < muMaxActiveMomentLimit)
            {
                u32 luMomentToUninhibit;
                if (!PickBestInhibitedMoment(&luMomentToUninhibit, E_PICK_BEST_ANY))
                {
                    break;
                }
                mMomentHandleArray[luMomentToUninhibit].GetMoment().Uninhibit();
                ++luUninhibitedMoments;
            }
        }

        while (luPotentiallyValidMoments > 0 && luInvalidMoments > 0)
        {
            u32 luMomentToUninhibit;   // DWARF cpp:204
            u32 luMomentInhibit;       // DWARF cpp:205
            CGS_ASSERT_AT(PickBestInhibitedMoment(&luMomentToUninhibit, E_PICK_BEST_FUSSY),
                          "PickBestInhibitedMoment(&luMomentToUninhibit)", CGS_ARTIST_FILE, 207);
            mMomentHandleArray[luMomentToUninhibit].GetMoment().Uninhibit();
            CGS_ASSERT_AT(PickWorstUninhibitedMoment(&luMomentInhibit, E_PICK_WORST_FUSSY),
                          "PickWorstUninhibitedMoment(&luMomentInhibit)", CGS_ARTIST_FILE, 210);
            mMomentHandleArray[luMomentInhibit].GetMoment().Inhibit();
            --luPotentiallyValidMoments;
            --luInvalidMoments;
        }
    }
}

// ---- PickBestInhibitedMoment @0x8221C028 (DWARF BrnMomentSelector.h:164) ------------------------------
// The best moment to UN-inhibit. The score is (1 - recency) * the description's weighting (fsubs f30 = 1.0,
// flt_82001C98, then fmuls -- two roundings). The walk is over the HANDLE array's length (+0x194, asserted); an
// unallocated handle is skipped.
//   E_PICK_BEST_FUSSY: among the moments that are inhibited AND whose conditions are met, the highest score. The
//     first one seeds; a later one wins only when strictly higher (`fcmpu ; ble`, so a NaN never wins).
//   E_PICK_BEST_ANY: every allocated moment takes the seed arm, so the LAST allocated one wins whatever its
//     state (the console's own branch table, transcribed as-is).
//   Any other option streams "Unknown option: " << option (cpp:476) and runs as FUSSY.
// *lpuIndex is written even when nothing is found (the seed's 0). Returns whether anything was found.
bool MomentSelector::PickBestInhibitedMoment(u32* lpuIndex, EPickBestInhibitedOptions leOptions)
{
    CGS_ASSERT_AT(mbPrepared, "mbPrepared", CGS_ARTIST_FILE, 456);

    // DWARF locals cpp:458 / 481..486.
    bool lbIgnoreConditionsMet = false;
    switch (leOptions)
    {
    case E_PICK_BEST_FUSSY:
        lbIgnoreConditionsMet = false;
        break;
    case E_PICK_BEST_ANY:
        lbIgnoreConditionsMet = true;
        break;
    default:
        CGS_FAIL_STREAM_AT("Unknown option: " << leOptions, CGS_ARTIST_FILE, 476);
        break;
    }

    bool      lbGotBest       = false;
    u32       luBestIndex     = 0;
    const u32 luNumMoments    = mMomentHandleArray.GetLength();
    u32       luLoop;
    f32       lfBestWeighting = 0.0f;

    for (luLoop = 0; luLoop < luNumMoments; ++luLoop)
    {
        if (mMomentHandleArray[luLoop].IsAllocated())
        {
            if ((!lbGotBest && mMomentHandleArray[luLoop].GetMoment().IsInhibited() &&
                 mMomentHandleArray[luLoop].GetMoment().ConditionsAreMet()) || lbIgnoreConditionsMet)
            {
                luBestIndex     = luLoop;
                lbGotBest       = true;
                lfBestWeighting = mMomentDescriptionArray[luLoop].mfWeighting * (1.0f - mRecencyArray[luLoop]);
            }
            else if (mMomentHandleArray[luLoop].GetMoment().IsInhibited() &&
                     mMomentHandleArray[luLoop].GetMoment().ConditionsAreMet())
            {
                const f32 lfWeighting = mMomentDescriptionArray[luLoop].mfWeighting * (1.0f - mRecencyArray[luLoop]);
                if (lfWeighting > lfBestWeighting)
                {
                    luBestIndex     = luLoop;
                    lfBestWeighting = lfWeighting;
                }
            }
        }
    }

    *lpuIndex = luBestIndex;
    return lbGotBest;
}

// ---- PickWorstUninhibitedMoment @0x8221C358 (DWARF BrnMomentSelector.h:167) -------------------------
// The moment to inhibit in its place: the LOWEST (1 - recency) * weighting.
//   E_PICK_WORST_FUSSY: among the moments that are neither inhibited nor ready (conditions not met). The first
//     one seeds; a later one wins only when strictly lower (`fcmpu ; bge`, so a NaN never wins).
//   E_PICK_WORST_ANY: an inhibitable moment (description +0x0C) takes the seed arm, so the last such one wins.
//     Otherwise an inhibited or ready moment would compete only when its bare weighting is already below the
//     worst score (0x8221C63C..0x8221C670). That branch is unreachable: ANY seeds every inhibitable moment
//     first. It is transcribed as the console has it.
//   Any other option streams "Unknown option: " << option (cpp:545) and runs as FUSSY.
bool MomentSelector::PickWorstUninhibitedMoment(u32* lpuIndex, EPickWorstUninhibitedOptions leOptions)
{
    CGS_ASSERT_AT(mbPrepared, "mbPrepared", CGS_ARTIST_FILE, 526);

    // DWARF locals cpp:528 / 550..555 / 575.
    bool lbIgnoreConditionsMet = false;
    switch (leOptions)
    {
    case E_PICK_WORST_FUSSY:
        lbIgnoreConditionsMet = false;
        break;
    case E_PICK_WORST_ANY:
        lbIgnoreConditionsMet = true;
        break;
    default:
        CGS_FAIL_STREAM_AT("Unknown option: " << leOptions, CGS_ARTIST_FILE, 545);
        break;
    }

    bool      lbGotWorst       = false;
    u32       luWorstIndex     = 0;
    const u32 luNumMoments     = mMomentHandleArray.GetLength();
    u32       luLoop;
    f32       lfWorstWeighting = 0.0f;

    for (luLoop = 0; luLoop < luNumMoments; ++luLoop)
    {
        if (mMomentHandleArray[luLoop].IsAllocated())
        {
            if ((!lbGotWorst && !mMomentHandleArray[luLoop].GetMoment().IsInhibited() &&
                 !mMomentHandleArray[luLoop].GetMoment().ConditionsAreMet()) ||
                (lbIgnoreConditionsMet && mMomentDescriptionArray[luLoop].mbCanBeInhibited))
            {
                luWorstIndex     = luLoop;
                lbGotWorst       = true;
                lfWorstWeighting = mMomentDescriptionArray[luLoop].mfWeighting * (1.0f - mRecencyArray[luLoop]);
            }
            else if ((!mMomentHandleArray[luLoop].GetMoment().IsInhibited() &&
                      !mMomentHandleArray[luLoop].GetMoment().ConditionsAreMet()) ||
                     (lbIgnoreConditionsMet && mMomentDescriptionArray[luLoop].mbCanBeInhibited &&
                      mMomentDescriptionArray[luLoop].mfWeighting < lfWorstWeighting))
            {
                const f32 lfWeighting = mMomentDescriptionArray[luLoop].mfWeighting * (1.0f - mRecencyArray[luLoop]);
                if (lfWeighting < lfWorstWeighting)
                {
                    luWorstIndex     = luLoop;
                    lfWorstWeighting = lfWeighting;
                }
            }
        }
    }

    *lpuIndex = luWorstIndex;
    return lbGotWorst;
}

// Release --
//
// Signature attested by the console build: this only, and a constant 1 is loaded before
// the epilogue, so it returns true unconditionally (bool Release()).
//
// The loop counts DOWN from the DESCRIPTION array's length while indexing the HANDLE array
// (the two are kept the same length by AddMoment) -- the console seeds the counter with
// the +0x0A0 count word minus one and loops while it stays >= 0. The trailing scalar
// stores are emitted in the order kept below
// (+0x1C4, +0x1E1, +0x1C8, +0x1D0, +0x1E0); they are independent so the order is cosmetic.
bool MomentSelector::Release()
{
    const u32 luMomentCount = mMomentDescriptionArray.GetLength();

    for (s32 luLoop = static_cast<s32>(luMomentCount) - 1; luLoop >= 0; --luLoop)
    {
        mMomentHandleArray[static_cast<u32>(luLoop)].Release();
    }

    mfTimeActive        = 0.0f;   // +0x1C4
    mbPrepared          = false;  // +0x1E1
    miFramesActive      = 0;      // +0x1C8
    muValidMoments      = 0;      // +0x1D0
    mbHasSelectedMoment = false;  // +0x1E0

    return true;
}

// AddMoment(MomentDescription) --
//
// Signature attested by the console build: this, and the 16-byte MomentDescription arrives
// BY VALUE in two integer argument slots -- the prologue spills both into one contiguous
// 16-byte home-area slot and passes the address of that slot straight to
// Array<MomentDescription,10>::Append. The automatic C translation reports twelve
// parameters here; that is the home-area over-count, not the real arity.
//
// Both asserts carry this .cpp's own baked file name, which is what proves this function --
// unlike its six siblings -- really is homed in the .cpp.
// Registers all three parallel arrays in lock-step: description, a fresh (unallocated)
// handle, and a zero recency entry.
bool MomentSelector::AddMoment(MomentDescription lMoment)
{
    CGS_ASSERT_AT(!mbPrepared, "!mbPrepared", CGS_ARTIST_FILE, 282);
    CGS_ASSERT_AT(mMomentDescriptionArray.GetLength() < mMomentDescriptionArray.GetCapacity(),
               "mMomentDescriptionArray.GetLength() < mMomentDescriptionArray.GetCapacity()", CGS_ARTIST_FILE, 283);

    MomentController::MomentHandle lMomentHandle;
    lMomentHandle.Construct();                      // console: the single zero byte into the slot

    mMomentDescriptionArray.Append(lMoment);
    mMomentHandleArray.Append(lMomentHandle);
    mRecencyArray.Append(0.0f);

    return true;
}

// AddMoment(EType, EMomentParamID, f32, bool) --
//
// No standalone console symbol: the console inlines it into every caller. The shape is pinned
// by ArbStateRoaming::Construct, which for each of its three candidates fills a
// stack MomentDescription field-by-field in exactly this order --
//     <type> -> +0x00   0 -> +0x04   <weighting> -> +0x08   false -> +0x0C
// -- then loads it into the two integer argument slots and calls the by-value overload above.
//
// The parameter names are the declaration's own (EA really did spell them with member prefixes).
bool MomentSelector::AddMoment(Moment::EType meMomentType,
                               MomentParameterBank::EMomentParamID meMomentParamID,
                               f32 mfWeighting,
                               bool mbCanBeInhibited)
{
    MomentDescription lDescription;

    lDescription.meMomentType     = meMomentType;
    lDescription.meMomentParamID  = meMomentParamID;
    lDescription.mfWeighting      = mfWeighting;
    lDescription.mbCanBeInhibited = mbCanBeInhibited;

    return AddMoment(lDescription);
}

// SelectBestMomentWithExclusion
//
// It sits directly on the cutaway path: ArbStateRoaming::Update's DRIVING arm calls
// SelectBestMoment(random) (the header inline, which is just this with exclusion == -1) and
// the ONLY writer of mbHasSelectedMoment is the body below.
//
// Signature attested by the console build: this, a CgsNumeric::Random&, and the excluded
// slot (s32). The LRU arm moves the exclusion into the first argument slot before its
// call, which is what pins SelectBestLRUMomentWithExclusion's arity at (this, exclusion)
// with no Random.
//
// Dispatch on meSelectionMode (+0x1DC): 0 -> LRU, 1 -> random-weighted, anything else fires
// "unhandled type" and reports false. The console compares the mode UNSIGNED against 1,
// so the LRU arm is taken for 0 only.
bool MomentSelector::SelectBestMomentWithExclusion(CgsNumeric::Random& lRandom, s32 liExclusion)
{
    switch (meSelectionMode)
    {
        case E_MODE_LRU_BEST:
            return SelectBestLRUMomentWithExclusion(liExclusion);

        case E_MODE_RANDOM_BEST:
            return SelectBestRandomMomentWithExclusion(lRandom, liExclusion);

        default:
            CGS_FAIL_AT(
                "unhandled type",
                    CGS_ARTIST_FILE, 338 /* ARTIST 0x82250FEC */);

            return false;
    }
}

// SelectBestLRUMomentWithExclusion --    NEW 2026-08-23
//
// The least-recently-used picker, and the ONE the game actually runs: MomentSelector::
// Construct writes meSelectionMode = E_MODE_LRU_BEST (0) and nothing in this tree ever calls
// SetSelectionMode (grep is clean), so every selection in the shipped path lands here.
//
// Score each candidate as (1 - recency) * weighting and keep the best; on success latch the
// winner and drive its recency to 1.0 so it is the least attractive candidate next time --
// that is the whole "LRU" mechanism, and it is why MomentSelector::Update multiplies the
// recency array by mfRecencyFactor every frame (the decay back toward selectable).
//
// Signature attested by the console build: this and liExclusion. Returns bool.
//
// CONSOLE WALK, every branch attested:
//   * muValidMoments (+0x1D0) == 0 -> return false BEFORE touching anything else.
//   * GetLength() on the DESCRIPTION array (+0x0A0) -- its "Array used before
//               Construct/Clear was called" tripwire is emitted here, unconditionally.
//   * the running best score starts at 0.0f (so a candidate scoring exactly 0 still wins --
//     the compare is `>=`), and the recency value latched onto the winner is 1.0f.
//   loop, five ordered tests, each falling to the `continue` label:
//               IsAllocated / GetState()==E_STATE_VALID (+0x174==3) /
//     CanSwitchToMeNow (+0x178) / index != exclusion / score >= best.
//               The console re-fetches the handle and re-calls GetMoment() before EACH of the
//     two moment reads (two separate tripwires per iteration); hoisting them is the
//     same reads in the same order.
//   * the float compare skips the candidate only when its score is BELOW the best, i.e. it
//     is kept when score >= best, so on a TIE the LATER index wins. Preserved deliberately:
//     with ArbStateRoaming's three equal-weight (0.5f) candidates and equal recency, that
//     is what decides the pick.
//   * miSelectedMoment (+0x1D8) = best, mbHasSelectedMoment (+0x1E0) = true,
//     mRecencyArray[best] = 1.0f.
bool MomentSelector::SelectBestLRUMomentWithExclusion(s32 liExclusion)
{
    if (muValidMoments != 0)
    {
        const s32 liMomentCount = static_cast<s32>(mMomentDescriptionArray.GetLength());   // +0x0A0

        bool lbFoundOne    = false;
        s32  liBestMoment  = 0;
        f32  lfBestScore   = 0.0f;

        for (s32 liLoop = 0; liLoop < liMomentCount; ++liLoop)
        {
            if (!mMomentHandleArray[static_cast<u32>(liLoop)].IsAllocated())
            {
                continue;
            }

            if (mMomentHandleArray[static_cast<u32>(liLoop)].GetMoment().GetState() !=
                Moment::E_STATE_VALID)                                   // +0x174 == 3
            {
                continue;
            }

            if (!mMomentHandleArray[static_cast<u32>(liLoop)].GetMoment().CanSwitchToMeNow())
            {                                                            // +0x178
                continue;
            }

            if (liLoop == liExclusion)
            {
                continue;
            }

            const MomentDescription& lrDescription = mMomentDescriptionArray[static_cast<u32>(liLoop)];
            const f32 lfScore =
                (1.0f - mRecencyArray[static_cast<u32>(liLoop)]) * lrDescription.mfWeighting;

            if (lfScore >= lfBestScore)                                  // ties: the later index wins
            {
                lbFoundOne   = true;
                lfBestScore  = lfScore;
                liBestMoment = liLoop;
            }
        }

        if (lbFoundOne)
        {
            miSelectedMoment = liBestMoment;
            mbHasSelectedMoment = true;
            mRecencyArray[static_cast<u32>(liBestMoment)] = 1.0f;
            return true;
        }
    }
    return false;
}

// SelectBestRandomMomentWithExclusion -- the E_MODE_RANDOM_BEST arm of
// SelectBestMomentWithExclusion. Unreachable on the shipped path: Construct writes
// meSelectionMode = E_MODE_LRU_BEST and SetSelectionMode has no caller in this tree.
//
// The candidate loop and the success tail are identical to the LRU arm above; only the
// "keep the best" step differs -- every qualifying candidate is pushed into a local
// Selector<u32,10> (BrnMomentSelectorSelector.h) as {score, slot index} and the winner is
// drawn weighted-randomly.
//
// Weight contract: Selector::AddElement asserts 0.0f < lfWeight0To1 <= 1.0f, so a
// zero-scoring candidate trips that assert here where the LRU arm would accept it. That
// assert belongs to AddElement, not to this function, and is deliberately not pre-filtered.
bool MomentSelector::SelectBestRandomMomentWithExclusion(CgsNumeric::Random& lRandom,
                                                        s32 liExclusion)
{
    // ARTIST 0x8223A668: one shared `return false` exit (beq to 0x8223A864), as in the LRU arm.
    if (muValidMoments != 0)                                        // +0x1D0
    {
        Selector<u32, 10> lSelector;
        lSelector.Construct();

        const s32 liMomentCount = static_cast<s32>(mMomentDescriptionArray.GetLength());   // +0x0A0

        for (s32 liLoop = 0; liLoop < liMomentCount; ++liLoop)
        {
            if (!mMomentHandleArray[static_cast<u32>(liLoop)].IsAllocated())
            {
                continue;
            }

            if (mMomentHandleArray[static_cast<u32>(liLoop)].GetMoment().GetState() !=
                Moment::E_STATE_VALID)                                   // +0x174 == 3
            {
                continue;
            }

            if (!mMomentHandleArray[static_cast<u32>(liLoop)].GetMoment().CanSwitchToMeNow())
            {                                                            // +0x178
                continue;
            }

            if (liLoop == liExclusion)
            {
                continue;
            }

            // The element temporary is stored before the score is computed (stw 0x50(r1) at
            // 0x8223A7AC, ahead of the description GetItem call).
            lSelector.AddElement(mMomentDescriptionArray[liLoop].mfWeighting * (1.0f - mRecencyArray[liLoop]),
                                 liLoop);
        }

        if (lSelector.GetLength() != 0)
        {
            const s32 liSelectedMoment = static_cast<s32>(lSelector.GetSelection(lRandom));

            mbHasSelectedMoment = true;            // +0x1E0
            miSelectedMoment    = liSelectedMoment;   // +0x1D8
            mRecencyArray[static_cast<u32>(liSelectedMoment)] = 1.0f;
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// MomentSelector::ActualDebugRender -- print one line per registered moment candidate
// through the director's debug printer, colour-coded by the candidate's live state.
//
// Every line fades with the candidate's recency: the alpha byte is 255 minus the recency
// scaled by 128, so a freshly-picked moment (recency 1) prints at alpha 127 and a long-unused
// one at 255. The packed colour is that alpha in the top byte over a per-state constant.
//
// The state test order is the original build's, branch for branch. Candidates whose handle is
// not allocated print nothing at all. Two of the arms print through PrintName (which formats
// the moment's own identity line) and the rest print the moment's GetName() directly; the
// 0xFFFF00 arm is unreachable with the conditions that reach it, and is kept because the
// original build keeps it.
// ----------------------------------------------------------------------------
void MomentSelector::ActualDebugRender(DebugPrinter& lrDebugPrinter) const
{
    const u32 luCount = mMomentDescriptionArray.GetLength();

    for (u32 luI = 0; luI < luCount; ++luI)
    {
        const s32 liAlpha = 255 - static_cast<s32>(mRecencyArray[luI] * 128.0f);

        if (!mMomentHandleArray[luI].IsAllocated())
        {
            continue;
        }

        const Moment& lrMoment = mMomentHandleArray[luI].GetMoment();

        if (lrMoment.GetState() == Moment::E_STATE_VALID && lrMoment.CanSwitchToMeNow())
        {
            lrDebugPrinter.Print(lrMoment.GetName(),
                                 static_cast<CgsDev::RGBA>((liAlpha << 24) | 0xFF00));
        }
        else if (lrMoment.GetState() == Moment::E_STATE_VALID && !lrMoment.CanSwitchToMeNow())
        {
            lrDebugPrinter.Print(lrMoment.GetName(),
                                 static_cast<CgsDev::RGBA>((liAlpha << 24) | 0x8000));
        }
        else if (lrMoment.ConditionsAreMet() && !lrMoment.IsInhibited())
        {
            lrDebugPrinter.Print(lrMoment.GetName(),
                                 static_cast<CgsDev::RGBA>((liAlpha << 24) | 0x80FF));
        }
        else if (!lrMoment.ConditionsAreMet() && !lrMoment.IsInhibited()
                 && mMomentDescriptionArray[luI].mbCanBeInhibited)
        {
            lrDebugPrinter.PrintName(lrMoment,
                                     static_cast<CgsDev::RGBA>((liAlpha << 24) | 0xFF));
        }
        else if (lrMoment.ConditionsAreMet() && lrMoment.IsInhibited())
        {
            lrDebugPrinter.PrintName(lrMoment,
                                     static_cast<CgsDev::RGBA>((liAlpha << 24) | 0xFFFF));
        }
        else if (!lrMoment.ConditionsAreMet() && lrMoment.IsInhibited())
        {
            lrDebugPrinter.PrintName(lrMoment,
                                     static_cast<CgsDev::RGBA>((liAlpha << 24) | 0x808080));
        }
        else if (lrMoment.ConditionsAreMet() || lrMoment.IsInhibited())
        {
            lrDebugPrinter.Print(lrMoment.GetName(),
                                 static_cast<CgsDev::RGBA>((liAlpha << 24) | 0xFFFF00));
        }
        else
        {
            lrDebugPrinter.PrintName(lrMoment,
                                     static_cast<CgsDev::RGBA>((liAlpha << 24) | 0x404040));
        }
    }
}

} // namespace BrnDirector

#undef CGS_ARTIST_FILE
