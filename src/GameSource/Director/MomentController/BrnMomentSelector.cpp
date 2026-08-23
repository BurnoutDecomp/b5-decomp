// Out-of-line bodies for BrnDirector::MomentSelector.
//
// Only the functions the console itself homed in BrnMomentSelector.cpp live here -- the
// six the X360's baked assert __FILE__/__LINE__ place in BrnMomentSelector.h
// (SetRecencyFactor / SelectBestMoment / SelectNewBestMoment / GetSelectedMoment /
// CancelSelection / SetMaxActiveMoments) are header inlines and are bodied in
// BrnMomentSelector.h, NOT here.
//
// Bodied in this TU:
//   MomentSelector::Construct  BrnMomentSelector.cpp:33   (no standalone X360 symbol -- the
//                              console inlines it; recovered from the copy inside
//                              ArbStateRoaming::Construct @0x82259CD0..0x82259CF8)
//   MomentSelector::Prepare    BrnMomentSelector.cpp:53   @0x82255C58
//   MomentSelector::Release    BrnMomentSelector.cpp:222  @0x8221BC90
//   MomentSelector::AddMoment(MomentDescription)          BrnMomentSelector.cpp:280 @0x82209F80
//   MomentSelector::AddMoment(EType,EMomentParamID,f32,bool) BrnMomentSelector.cpp:302
//
// Signature authority is the DecFIGS DWARF for this exact file, cross-checked against the
// X360 argument registers at every call site (Hex-Rays' argument lists for all five are
// wrong: it renders AddMoment's single by-value 16-byte record as twelve scalars, and it
// prints Prepare/SelectBestMoment's reference args as ints).
//
// Local variable names are the DWARF's own (luMomentCount / luLoop / lMomentDescription /
// luMomentsToNotInhibit / lMomentHandle / lDescription).

#include "GameSource/Director/MomentController/BrnMomentSelector.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [DIAG] one-shot jump-ladder rungs only

namespace BrnDirector
{

// ---------------------------------------------------------------------------------------
// Construct -- BrnMomentSelector.cpp:33
//
// No standalone X360 symbol (fully inlined). ArbStateRoaming::Construct @0x82259C00 emits it
// verbatim over the embedded selector at +0x1B8:
//     stfs 0.0 -> +0x1C4 (mfTimeActive)      stfs 0.0 -> +0x1CC (mfRecencyFactor)
//     stw  0   -> +0x0A0 / +0x194 / +0x1C0   (the three Array<> count words == Construct())
//     stw  0   -> +0x1C8 (miFramesActive)    stw  0   -> +0x1D0 (muValidMoments)
//     stw  0   -> +0x1DC (meSelectionMode == E_MODE_LRU_BEST)
//     stb  0   -> +0x1E1 (mbPrepared)        stw  0   -> +0x1D4 (muMaxActiveMomentLimit)
//     stb  0   -> +0x1E2 (mbHasMaxLimit)
// mbHasSelectedMoment (+0x1E0) and miSelectedMoment (+0x1D8) are deliberately NOT written --
// the console leaves both uninitialised until the first SelectBestMoment*. Faithful: do the
// same. (Release() is what clears mbHasSelectedMoment.)
// ---------------------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------------------
// Prepare -- BrnMomentSelector.cpp:53   @0x82255C58
//
// Signature FROM ASM: r3 = this, r4 = a MomentController, r5 = a Camera::BehaviourManager.
// Both are proved by the forwarding call at 0x82255D34 --
//     r3 = r27 (arg 2)  r4 = desc[0]  r5 = desc[4]  r6 = &mMomentHandleArray[i]  r7 = r26 (arg 3)
// -- which is exactly MomentController::NewMoment(EType, EMomentParamID, MomentHandle&,
// BehaviourManager&). It is confirmed from the caller side too: ArbStateRoaming::Prepare
// @0x82259E40 loads r4 from ArbStateSharedInfo +0x20 (mpMomentController) and r5 from +0x18
// (mpBehaviourManager). The DWARF agrees: Prepare(MomentController&, BehaviourManager&).
//
// Returns mbPrepared (`lbz r3, 0x1E1`), which is cleared by any NewMoment failure.
// ---------------------------------------------------------------------------------------
bool MomentSelector::Prepare(MomentController& lrMomentController,
                             Camera::BehaviourManager& lrBehaviourManager)
{
    // Read once, before the mbPrepared test -- the console runs GetLength()'s
    // "Array used before Construct/Clear was called" assert unconditionally at entry.
    const u32 luMomentCount = mMomentDescriptionArray.GetLength();   // cpp:55
    u32       luLoop;                                                // cpp:56

    if (!mbPrepared)
    {
        mbPrepared = true;

        // Give every registered description a live moment. A NewMoment failure does NOT
        // abort the loop; it only drops the prepared flag (the console keeps iterating).
        for (luLoop = 0; luLoop < luMomentCount; ++luLoop)
        {
            if (!mMomentHandleArray[luLoop].IsAllocated())
            {
                const MomentDescription lMomentDescription = mMomentDescriptionArray[luLoop];  // cpp:68

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
        // both arms rather than hoisting it (0x82255D84.. and 0x82255DC4..); kept as-is.
        if (mbHasMaxLimit)
        {
            u32 luMomentsToNotInhibit = muMaxActiveMomentLimit;   // cpp:87

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
                        // The console inlines MomentHandle::GetMoment() (its "mbIsAllocated"
                        // assert at BrnMomentController.h:141) then Moment::Inhibit()
                        // (stb mbIsInhibited=1; vtable+0x10 Release(); meState = SEARCHING).
                        mMomentHandleArray[luLoop].GetMoment()->Inhibit();
                    }
                }
            }
        }
    }

    return mbPrepared;
}

// ---------------------------------------------------------------------------------------
// Update -- BrnMomentSelector.cpp:115   @0x82239FC0   ⭐ NEW 2026-08-01
//
// Signature FROM ASM: r3 = this, f1 = the timestep (`fmr f31, f1`, then f31 is added to
// +0x1C4). Nothing is returned. Hex-Rays' `(_DWORD* result, double a2)` is the usual PPC
// float-ABI artefact.
//
// Advance the selector's accumulators, decay every candidate's recency score, and re-classify
// every live moment into four running counters -- one of which is muValidMoments, the count
// ArbStateRoaming::Update's DRIVING arm reads every frame to decide whether to ask for an
// establishing shot. Then inhibit any moment that is VALID but cannot be switched to.
//
// LOOP SHAPE (0x8223A088..0x8223A418), every branch attested:
//   skip the currently-selected slot;  mRecencyArray[i] *= mfRecencyFactor;
//   skip !IsAllocated();   then five ordered tests on the moment, four of which `continue`.
// The console re-fetches the handle and re-calls GetMoment() before EVERY one of those tests
// (nine separate `mbIsAllocated` tripwires at BrnMomentController.h:141 in one loop body);
// hoisting them is the same reads in the same order, so this keeps one local per test group.
// ---------------------------------------------------------------------------------------
void MomentSelector::Update(f32 lfTimestep)
{
    CGS_ASSERT(mbPrepared, "mbPrepared");                            // cpp:117 (0x75)

    mfTimeActive += lfTimestep;      // +0x1C4
    ++miFramesActive;                // +0x1C8

    const s32 liMomentCount = static_cast<s32>(mMomentDescriptionArray.GetLength());  // +0x0A0

    u32 luUninhibited        = 0;    // r28 -- live, not inhibited, and inhibitable by policy
    u32 luConditionsNotMet   = 0;    // r20 -- conditions not met, not inhibited, inhibitable
    u32 luInhibitedCandidate = 0;    // r22 -- conditions met but currently inhibited

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

        // (A) 0x8223A110 -- running, and the policy is allowed to inhibit it.
        if (!lpMoment->IsInhibited() && lrDescription.mbCanBeInhibited)
        {
            ++luUninhibited;
        }

        // (B) 0x8223A170 -- VALID and switchable right now: this is the count the roaming
        // state reads.
        if (lpMoment->IsValid() && lpMoment->CanSwitchToMeNow())
        {
            ++muValidMoments;
            continue;
        }

        // (C) 0x8223A208 -- waiting on its conditions, not inhibited, inhibitable.
        if (!lpMoment->ConditionsAreMet() && !lpMoment->IsInhibited() &&
            lrDescription.mbCanBeInhibited)
        {
            ++luConditionsNotMet;
            continue;
        }

        // (D) 0x8223A2B0 -- ready but held back: a candidate for the rebalance below.
        if (lpMoment->ConditionsAreMet() && lpMoment->IsInhibited())
        {
            ++luInhibitedCandidate;
            continue;
        }

        // (E) 0x8223A340 -- VALID but NOT switchable: inhibit it.
        if (!lpMoment->IsValid() || lpMoment->CanSwitchToMeNow())
        {
            continue;
        }

        lpMoment->Inhibit();                                   // 0x8223A3A4

        // ⚠️ FAITHFUL QUIRK: when the description forbids inhibiting, the console inhibits it
        // ANYWAY and then immediately un-inhibits it, asserts the byte really came back down
        // (BrnMomentSelector.h line 0xA7), and decrements the uninhibited count. Inhibit()'s
        // side effects (the virtual Release() and meState = E_STATE_INVALID_SEARCHING) are NOT
        // undone -- only the flag is. Reproduced exactly.
        if (!lrDescription.mbCanBeInhibited)
        {
            lpMoment->SetInhibited(false);                      // 0x8223A3DC
            CGS_ASSERT(!lpMoment->IsInhibited(),
                       "!mMomentHandleArray[liLoop].GetMoment()->IsInhibited()");   // :167
            --luUninhibited;                                    // 0x8223A40C
        }
    }

    // [DIAG] NOT IN THE X360 BINARY. Rung 6 of the `[jump-ladder]`: the selector saw at
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

    // [GATED @0x8223A41C..0x8223A658 -- the max-active-moments REBALANCE]
    //   if (mbHasMaxLimit && luInhibitedCandidate != 0)
    //   {
    //       // walk luUninhibited toward muMaxActiveMomentLimit: un-inhibit the best inhibited
    //       // candidate while under budget (PickBestInhibitedMoment @0x8221C028), and when over
    //       // budget swap -- PickWorstUninhibitedMoment @0x8221C358 picks the victim, the
    //       // moment's vtable slot 4 Release() runs and meState goes to E_STATE_INVALID_INACTIVE.
    //   }
    // WHY GATED: PickBestInhibitedMoment (202 asm lines) and PickWorstUninhibitedMoment (222)
    // have no body anywhere in this tree, and writing them is a wave of its own. The gate itself
    // is FALSE for every consumer that exists today: mbHasMaxLimit is raised only by
    // SetMaxActiveMoments (BrnMomentSelector.h:189) and NOTHING in the tree calls it -- grep is
    // clean, and the three arbitrator states that embed a MomentSelector all go straight from
    // Construct to AddMoment. So this block cannot execute even if it were written, and the
    // three counters it consumes are computed above regardless.
    // DELETE-WHEN: PickBestInhibitedMoment + PickWorstUninhibitedMoment land.
    (void)luConditionsNotMet;
    (void)luInhibitedCandidate;
    (void)luUninhibited;
}

// ---------------------------------------------------------------------------------------
// Release -- BrnMomentSelector.cpp:222   @0x8221BC90
//
// Signature FROM ASM: r3 = this only; `li r3, 1` before the epilogue, so it returns true
// unconditionally (DWARF: bool Release()).
//
// The loop counts DOWN from the DESCRIPTION array's length while indexing the HANDLE array
// (the two are kept the same length by AddMoment) -- console `r31 = *(this+0xA0) - 1` then
// `while (r31 >= 0)`. The trailing scalar stores are emitted in the order kept below
// (+0x1C4, +0x1E1, +0x1C8, +0x1D0, +0x1E0); they are independent so the order is cosmetic.
// ---------------------------------------------------------------------------------------
bool MomentSelector::Release()
{
    const u32 luMomentCount = mMomentDescriptionArray.GetLength();   // cpp:224

    for (s32 luLoop = static_cast<s32>(luMomentCount) - 1; luLoop >= 0; --luLoop)   // cpp:225
    {
        mMomentHandleArray[static_cast<u32>(luLoop)].Release();
    }

    mfTimeActive        = 0.0f;   // +0x1C4 (flt_82001CC0 == 0.0f)
    mbPrepared          = false;  // +0x1E1
    miFramesActive      = 0;      // +0x1C8
    muValidMoments      = 0;      // +0x1D0
    mbHasSelectedMoment = false;  // +0x1E0

    return true;
}

// ---------------------------------------------------------------------------------------
// AddMoment(MomentDescription) -- BrnMomentSelector.cpp:280   @0x82209F80
//
// Signature FROM ASM: r3 = this, and the 16-byte MomentDescription arrives BY VALUE in the
// r4:r5 GPR pair -- the prologue spills both as doublewords into one contiguous 16-byte
// home-area slot (`std r4, arg_20` / `std r5, arg_28`) and passes &that slot straight to
// Array<MomentDescription,10>::Append. Hex-Rays reports twelve parameters here; that is the
// home-area over-count, not the real arity.
//
// Both asserts carry BrnMomentSelector.cpp line numbers (282 / 283), which is what proves
// this function -- unlike its six siblings -- really is homed in the .cpp.
// Registers all three parallel arrays in lock-step: description, a fresh (unallocated)
// handle, and a zero recency entry.
// ---------------------------------------------------------------------------------------
bool MomentSelector::AddMoment(MomentDescription lMoment)
{
    CGS_ASSERT(!mbPrepared, "!mbPrepared");                                                       // cpp:282
    CGS_ASSERT(mMomentDescriptionArray.GetLength() < mMomentDescriptionArray.GetCapacity(),
               "mMomentDescriptionArray.GetLength() < mMomentDescriptionArray.GetCapacity()");    // cpp:283

    MomentController::MomentHandle lMomentHandle;   // cpp:285
    lMomentHandle.Construct();                      // console: the single `stb 0` into the slot

    mMomentDescriptionArray.Append(lMoment);
    mMomentHandleArray.Append(lMomentHandle);
    mRecencyArray.Append(0.0f);                     // flt_82001CC0 == 0.0f

    return true;
}

// ---------------------------------------------------------------------------------------
// AddMoment(EType, EMomentParamID, f32, bool) -- BrnMomentSelector.cpp:302
//
// No standalone X360 symbol: the console inlines it into every caller. The shape is pinned
// by ArbStateRoaming::Construct @0x82259C00, which for each of its three candidates fills a
// stack MomentDescription field-by-field in exactly this order --
//     stw <type> -> +0x00   stw 0 -> +0x04   stfs f31 -> +0x08   stb 0 -> +0x0C
// -- then loads it into r4:r5 and calls the by-value overload above.
//
// The parameter names are the DWARF's own (EA really did spell them with member prefixes).
// ---------------------------------------------------------------------------------------
bool MomentSelector::AddMoment(Moment::EType meMomentType,
                               MomentParameterBank::EMomentParamID meMomentParamID,
                               f32 mfWeighting,
                               bool mbCanBeInhibited)
{
    MomentDescription lDescription;   // cpp:304

    lDescription.meMomentType     = meMomentType;
    lDescription.meMomentParamID  = meMomentParamID;
    lDescription.mfWeighting      = mfWeighting;
    lDescription.mbCanBeInhibited = mbCanBeInhibited;

    return AddMoment(lDescription);
}

// ---------------------------------------------------------------------------------------
// SelectBestMomentWithExclusion -- BrnMomentSelector.cpp:320   @0x82250FC8   NEW 2026-08-23
//
// ⭐ THIS WAS A GROUP-F STUB IN DirectorLinkStubs.cpp UNTIL TODAY, AND THE STUB WAS
// `return false` -- i.e. "no moment was selected", every frame, forever. It sits directly on
// the cutaway path: ArbStateRoaming::Update's DRIVING arm calls SelectBestMoment(random)
// (the header inline @0x82254DA8, which is just this with exclusion == -1) and the ONLY
// writer of mbHasSelectedMoment is the body below. With the stub standing, even a correctly
// allocated, valid, switchable jump moment could never be picked.
//
// Signature FROM ASM: r3 = this, r4 = CgsNumeric::Random&, r5 = the excluded slot (s32).
// The LRU arm re-loads r5 into r4 (`mr r4, r5` @0x8225102C) before its call, which is what
// pins SelectBestLRUMomentWithExclusion's arity at (this, exclusion) with no Random.
//
// Dispatch on meSelectionMode (+0x1DC): 0 -> LRU, 1 -> random-weighted, anything else fires
// "unhandled type" (BrnMomentSelector.cpp:338) and reports false. `cmplwi 1 / blt / beq` is
// an UNSIGNED compare, so the LRU arm is taken for 0 only.
// ---------------------------------------------------------------------------------------
bool MomentSelector::SelectBestMomentWithExclusion(CgsNumeric::Random& lRandom, s32 liExclusion)
{
    switch (meSelectionMode)
    {
        case E_MODE_LRU_BEST:
            return SelectBestLRUMomentWithExclusion(liExclusion);

        case E_MODE_RANDOM_BEST:
            return SelectBestRandomMomentWithExclusion(lRandom, liExclusion);

        default:
            CGS_ASSERT(false, "unhandled type");   // cpp:338
            return false;
    }
}

// ---------------------------------------------------------------------------------------
// SelectBestLRUMomentWithExclusion -- BrnMomentSelector.cpp:351   @0x8221BE50   NEW 2026-08-23
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
// Signature FROM ASM: r3 = this, r4 = liExclusion. Returns bool (`li r3,1` / `li r3,0`).
//
// ASM WALK (0x8221BE50..0x8221C024), every branch attested:
//   0x8221BE6C  muValidMoments (+0x1D0) == 0  -> return false BEFORE touching anything else.
//   0x8221BE78  GetLength() on the DESCRIPTION array (+0xA0) -- its "Array used before
//               Construct/Clear was called" tripwire is emitted here, unconditionally.
//   0x8221BEAC  f31 = flt_82001CC0 == 0.0f (the running best score, so a candidate scoring
//               exactly 0 still wins -- the compare is `>=`), f30 = flt_82001C98 == 1.0f.
//   loop 0x8221BEE0..0x8221BFD0, five ordered tests, each falling to the `continue` label:
//               IsAllocated / GetState()==E_STATE_VALID (+0x174==3) /
//               CanSwitchToMeNow() (+0x178) / index != exclusion / score >= best.
//               The console re-fetches the handle and re-calls GetMoment() before EACH of the
//               two moment reads (two separate BrnMomentController.h:141 tripwires per
//               iteration); hoisting them is the same reads in the same order.
//   0x8221BFB4  `fcmpu; blt -> skip` == keep when score >= best, so on a TIE the LATER index
//               wins. Preserved deliberately: with ArbStateRoaming's three equal-weight
//               (0.5f) candidates and equal recency, that is what decides the pick.
//   0x8221BFE4  miSelectedMoment (+0x1D8) = best, mbHasSelectedMoment (+0x1E0) = true,
//               mRecencyArray[best] = 1.0f (f30).
// ---------------------------------------------------------------------------------------
bool MomentSelector::SelectBestLRUMomentWithExclusion(s32 liExclusion)
{
    if (muValidMoments == 0)                                        // 0x8221BE6C
    {
        return false;
    }

    const s32 liMomentCount = static_cast<s32>(mMomentDescriptionArray.GetLength());   // +0x0A0

    bool lbFoundOne    = false;   // r26
    s32  liBestMoment  = 0;       // r27
    f32  lfBestScore   = 0.0f;    // f31 (flt_82001CC0)

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

        if (lfScore >= lfBestScore)                                  // 0x8221BFB4 (ties: later wins)
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
    mRecencyArray[static_cast<u32>(liBestMoment)] = 1.0f;   // f30 == flt_82001C98

    // [DIAG] NOT IN THE X360 BINARY. Rung 7 of the `[jump-ladder]`: a cutaway moment was
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

} // namespace BrnDirector
