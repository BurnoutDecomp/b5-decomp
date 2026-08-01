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

} // namespace BrnDirector
