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
//
// Signature authority is the declarations for this exact file, cross-checked against the
// argument slots the console build actually uses at every call site (the automatic C
// translation's argument lists for all five are wrong: it renders AddMoment's single
// by-value 16-byte record as twelve scalars, and it prints Prepare/SelectBestMoment's
// reference args as ints).
//
// Local variable names are the declaration's own (luMomentCount / luLoop / lMomentDescription /
// luMomentsToNotInhibit / lMomentHandle / lDescription).

#include "GameSource/Director/MomentController/BrnMomentSelector.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [DIAG] one-shot jump-ladder rungs only
#include "GameSource/Director/MomentController/BrnMomentSelectorSelector.h"   // Selector<u32,10> (the random arm)

namespace BrnDirector
{

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

            for (luLoop = 0; luLoop < luMomentCount; ++luLoop)
            {
                if (luMomentsToNotInhibit != 0)
                {
                    if (mMomentHandleArray[luLoop].IsAllocated() &&
                        mMomentDescriptionArray[luLoop].mbCanBeInhibited)
                    {
                        --luMomentsToNotInhibit;
                    }
                }
                else
                {
                    if (mMomentHandleArray[luLoop].IsAllocated() &&
                        mMomentDescriptionArray[luLoop].mbCanBeInhibited)
                    {
                        // The console inlines MomentHandle::GetMoment (with its
                        // "mbIsAllocated" assert) then Moment::Inhibit (raise
                        // mbIsInhibited; vtable +0x10 Release(); meState = SEARCHING).
                        mMomentHandleArray[luLoop].GetMoment()->Inhibit();
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
//         if (mMomentHandleArray[i].GetMoment()->GetState() != E_STATE_VALID) continue;
//         if ( mMomentHandleArray[i].GetMoment()->CanSwitchToMeNow()) ++muValidMoments;
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

    for (u32 luLoop = 0; luLoop < luMomentCount; ++luLoop)
    {
        if (!mMomentHandleArray[luLoop].IsAllocated())
        {
            continue;
        }

        const Moment* lpMoment = mMomentHandleArray[luLoop].GetMoment();

        if (lpMoment->GetState() == Moment::E_STATE_VALID && lpMoment->CanSwitchToMeNow())
        {
            ++muValidMoments;
        }
    }

    return muValidMoments;
}

void MomentSelector::Update(f32 lfTimestep)
{
    CGS_ASSERT(mbPrepared, "mbPrepared");

    mfTimeActive += lfTimestep;      // +0x1C4
    ++miFramesActive;                // +0x1C8

    const s32 liMomentCount = static_cast<s32>(mMomentDescriptionArray.GetLength());  // +0x0A0

    u32 luUninhibited        = 0;    // live, not inhibited, and inhibitable by policy
    u32 luConditionsNotMet   = 0;    // conditions not met, not inhibited, inhibitable
    u32 luInhibitedCandidate = 0;    // conditions met but currently inhibited

    muValidMoments = 0;              // +0x1D0 -- recounted from scratch every frame

    for (s32 liLoop = 0; liLoop < liMomentCount; ++liLoop)
    {
        // Never re-classify the slot that is already selected.
        if (mbHasSelectedMoment && miSelectedMoment == liLoop)
        {
            continue;
        }

        // Recency decay (the console mutates the array element in place through GetItem).
        mRecencyArray[static_cast<u32>(liLoop)] *= mfRecencyFactor;   // +0x198[i] *= +0x1CC

        if (!mMomentHandleArray[static_cast<u32>(liLoop)].IsAllocated())
        {
            continue;
        }

        Moment* lpMoment = mMomentHandleArray[static_cast<u32>(liLoop)].GetMoment();
        const MomentDescription& lrDescription = mMomentDescriptionArray[static_cast<u32>(liLoop)];

        // (A) running, and the policy is allowed to inhibit it.
        if (!lpMoment->IsInhibited() && lrDescription.mbCanBeInhibited)
        {
            ++luUninhibited;
        }

        // (B) VALID and switchable right now: this is the count the roaming
        // state reads.
        if (lpMoment->IsValid() && lpMoment->CanSwitchToMeNow())
        {
            ++muValidMoments;
            continue;
        }

        // (C) waiting on its conditions, not inhibited, inhibitable.
        if (!lpMoment->ConditionsAreMet() && !lpMoment->IsInhibited() &&
            lrDescription.mbCanBeInhibited)
        {
            ++luConditionsNotMet;
            continue;
        }

        // (D) ready but held back: a candidate for the rebalance below.
        if (lpMoment->ConditionsAreMet() && lpMoment->IsInhibited())
        {
            ++luInhibitedCandidate;
            continue;
        }

        // (E) VALID but NOT switchable: inhibit it.
        if (!lpMoment->IsValid() || lpMoment->CanSwitchToMeNow())
        {
            continue;
        }

        lpMoment->Inhibit();

        // ⚠️ FAITHFUL QUIRK: when the description forbids inhibiting, the console inhibits it
        // ANYWAY and then immediately un-inhibits it, asserts the byte really came back down
        // (the assert lives in BrnMomentSelector.h), and decrements the uninhibited count.
        // Inhibit()'s
        // side effects (the virtual Release() and meState = E_STATE_INVALID_SEARCHING) are NOT
        // undone -- only the flag is. Reproduced exactly.
        if (!lrDescription.mbCanBeInhibited)
        {
            lpMoment->SetInhibited(false);
            CGS_ASSERT(!lpMoment->IsInhibited(),
                       "!mMomentHandleArray[liLoop].GetMoment()->IsInhibited()");
            --luUninhibited;
        }
    }

    // [DIAG] NOT IN THE console BINARY. Rung 6 of the `[jump-ladder]`: the selector saw at
    // least one moment go VALID + switchable. This is the number ArbStateRoaming::Update's
    // DRIVING arm gates SelectBestMoment on, and it was PINNED AT 0 for the whole project
    // (MomentController::NewMoment was a stub that allocated nothing, so every handle stayed
    // !IsAllocated() and the loop above `continue`d on all of them). One-shot.
    {
        static bool sbLoggedFirstValid = false;
        if (!sbLoggedFirstValid && muValidMoments != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLoggedFirstValid = true;
            *CgsDev::Log::gpDebugPrint
                << "[FLAG PC bring-up] [jump-ladder] MomentSelector muValidMoments="
                << static_cast<s32>(muValidMoments)
                << " of " << liMomentCount
                << " candidates (frames=" << static_cast<s32>(miFramesActive) << ")\n";
        }
    }

    // [GATED -- the max-active-moments REBALANCE]
    //   if (mbHasMaxLimit && luInhibitedCandidate != 0)
    //   {
    //       // walk luUninhibited toward muMaxActiveMomentLimit: un-inhibit the best inhibited
    //       // candidate while under budget (PickBestInhibitedMoment), and when over
    //       // budget swap -- PickWorstUninhibitedMoment picks the victim, the
    //       // moment's vtable slot 4 Release() runs and meState goes to E_STATE_INVALID_INACTIVE.
    //   }
    // WHY GATED: PickBestInhibitedMoment (202 instructions) and PickWorstUninhibitedMoment (222)
    // have no body anywhere in this tree, and writing them is a wave of its own. The gate itself
    // is FALSE for every consumer that exists today: mbHasMaxLimit is raised only by
    // SetMaxActiveMoments and NOTHING in the tree calls it -- grep is
    // clean, and the three arbitrator states that embed a MomentSelector all go straight from
    // Construct to AddMoment. So this block cannot execute even if it were written, and the
    // three counters it consumes are computed above regardless.
    // DELETE-WHEN: PickBestInhibitedMoment + PickWorstUninhibitedMoment land.
    (void)luConditionsNotMet;
    (void)luInhibitedCandidate;
    (void)luUninhibited;
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
    CGS_ASSERT(!mbPrepared, "!mbPrepared");
    CGS_ASSERT(mMomentDescriptionArray.GetLength() < mMomentDescriptionArray.GetCapacity(),
               "mMomentDescriptionArray.GetLength() < mMomentDescriptionArray.GetCapacity()");

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

// SelectBestMomentWithExclusion --    NEW 2026-08-23
//
// ⭐ THIS WAS A GROUP-F STUB IN DirectorLinkStubs.cpp UNTIL TODAY, AND THE STUB WAS
// `return false` -- i.e. "no moment was selected", every frame, forever. It sits directly on
// the cutaway path: ArbStateRoaming::Update's DRIVING arm calls SelectBestMoment(random)
// (the header inline, which is just this with exclusion == -1) and the ONLY
// writer of mbHasSelectedMoment is the body below. With the stub standing, even a correctly
// allocated, valid, switchable jump moment could never be picked.
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
            CGS_ASSERT(false, "unhandled type");
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
    if (muValidMoments == 0)
    {
        return false;
    }

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

        if (mMomentHandleArray[static_cast<u32>(liLoop)].GetMoment()->GetState() !=
            Moment::E_STATE_VALID)                                   // +0x174 == 3
        {
            continue;
        }

        if (!mMomentHandleArray[static_cast<u32>(liLoop)].GetMoment()->CanSwitchToMeNow())
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

    if (!lbFoundOne)
    {
        return false;
    }

    miSelectedMoment    = liBestMoment;   // +0x1D8
    mbHasSelectedMoment = true;           // +0x1E0
    mRecencyArray[static_cast<u32>(liBestMoment)] = 1.0f;

    // [DIAG] NOT IN THE CONSOLE BINARY. Rung 7 of the `[jump-ladder]`: a cutaway moment was
    // actually PICKED. One-shot; costs one predictable branch per successful selection and
    // nothing at all once it has fired.
    {
        static bool sbLoggedFirstSelection = false;
        if (!sbLoggedFirstSelection && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLoggedFirstSelection = true;
            *CgsDev::Log::gpDebugPrint
                << "[FLAG PC bring-up] [jump-ladder] MomentSelector SELECTED moment slot="
                << liBestMoment
                << " type=" << static_cast<s32>(
                       mMomentDescriptionArray[static_cast<u32>(liBestMoment)].meMomentType)
                << " (7=PLAYER_JUMPING 8=PLAYER_STUNT 10=NEW_CAR_JOINED)"
                << " valid=" << static_cast<s32>(muValidMoments)
                << " excl=" << liExclusion << "\n";
        }
    }

    return true;
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
    if (muValidMoments == 0)                                        // +0x1D0
    {
        return false;
    }

    Selector<u32, 10> lSelector;
    lSelector.Construct();

    const s32 liMomentCount = static_cast<s32>(mMomentDescriptionArray.GetLength());   // +0x0A0

    for (s32 liLoop = 0; liLoop < liMomentCount; ++liLoop)
    {
        if (!mMomentHandleArray[static_cast<u32>(liLoop)].IsAllocated())
        {
            continue;
        }

        if (mMomentHandleArray[static_cast<u32>(liLoop)].GetMoment()->GetState() !=
            Moment::E_STATE_VALID)                                   // +0x174 == 3
        {
            continue;
        }

        if (!mMomentHandleArray[static_cast<u32>(liLoop)].GetMoment()->CanSwitchToMeNow())
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

        lSelector.AddElement(lfScore, static_cast<u32>(liLoop));
    }

    if (lSelector.GetLength() == 0)
    {
        return false;
    }

    const s32 liSelectedMoment = static_cast<s32>(lSelector.GetSelection(lRandom));

    mbHasSelectedMoment = true;            // +0x1E0
    miSelectedMoment    = liSelectedMoment;   // +0x1D8
    mRecencyArray[static_cast<u32>(liSelectedMoment)] = 1.0f;

    // [DIAG] one-shot jump-ladder rung 7 for the random arm; remove with the other diagnostics.
    {
        static bool sbLoggedFirstRandomSelection = false;
        if (!sbLoggedFirstRandomSelection && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLoggedFirstRandomSelection = true;
            *CgsDev::Log::gpDebugPrint
                << "[FLAG PC bring-up] [jump-ladder] MomentSelector SELECTED moment (RANDOM arm) slot="
                << liSelectedMoment
                << " type=" << static_cast<s32>(
                       mMomentDescriptionArray[static_cast<u32>(liSelectedMoment)].meMomentType)
                << " valid=" << static_cast<s32>(muValidMoments)
                << " excl=" << liExclusion << "\n";
        }
    }

    return true;
}

} // namespace BrnDirector
