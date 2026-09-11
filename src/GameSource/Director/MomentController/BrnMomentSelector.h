#pragma once

// Home for BrnDirector::MomentDescription and BrnDirector::MomentSelector -- the
// establishing-shot "moment" picker that each director arbitrator state embeds BY VALUE
// (ArbStateRoaming @+0x1B8, ArbStateCrashing, ArbStateTakedown @+0x42C).
//
// LAYOUT + SIGNATURE AUTHORITY: the game's own declarations for this header (the real EA
// member list and member order), cross-checked store-for-store against the console build.
// The console offsets quoted below are provenance only -- access is
// BY NAME (the x64-host gate widens the embedded arrays), never by byte offset.
//
//   console offset map (all attested by an instruction in this file's own functions):
//     +0x000  mMomentDescriptionArray   Array<MomentDescription,10>  (count word @+0x0A0 == 10*0x10)
//     +0x0A4  mMomentHandleArray        Array<MomentHandle,10>       (count word @+0x194 == 0xA4+10*0x18)
//     +0x198  mRecencyArray             Array<f32,10>                (count word @+0x1C0)
//     +0x1C4  mfTimeActive              +0x1C8  miFramesActive       +0x1CC  mfRecencyFactor
//     +0x1D0  muValidMoments            +0x1D4  muMaxActiveMomentLimit
//     +0x1D8  miSelectedMoment          +0x1DC  meSelectionMode
//     +0x1E0  mbHasSelectedMoment       +0x1E1  mbPrepared           +0x1E2  mbHasMaxLimit
//   The 0x18 MomentHandle stride is pinned by SetMaxActiveMoments, which reads the
//   handle array's count at +0x0A4 + 0xF0 == +0x194.
//
// WHERE EACH BODY LIVES (recovered from each function's own baked __FILE__/__LINE__ assert,
// which is the definition site -- project rule 12):
//     SetRecencyFactor   (header inline, below)
//     SelectBestMoment   (header inline, below)
//     GetSelectedMoment   (header inline, below)
//     CancelSelection   (header inline, below)
//     SetMaxActiveMoments   (header inline, below)
//     SelectNewBestMoment   (header inline, out-of-class below)
//     Prepare
//    Release
//    AddMoment(MomentDescription)
//    AddMoment(EType,EMomentParamID,f32,bool)  -- inlined at every
//                             console call site (no standalone symbol); shape recovered from
//                             ArbStateRoaming::Construct, which builds the four
//                             MomentDescription words on the stack and passes them in the
//                             two integer argument slots.

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Containers/CgsArray.h"                  // Array<T,N> (global ns; declaration spells it CgsContainers::Array)
#include "GameSource/Director/MomentController/BrnMoment.h"              // Moment, Moment::EType
#include "GameSource/Director/MomentController/BrnMomentParameterBank.h" // MomentParameterBank::EMomentParamID
#include "GameSource/Director/MomentController/BrnMomentController.h"    // MomentController, MomentController::MomentHandle (by value)

namespace CgsNumeric { class Random; }   // SelectBestMoment / SelectNewBestMoment draw arg

namespace BrnDirector
{

struct DebugPrinter;                          // dev print sink (DebugRender / ActualDebugRender)
namespace Camera { class BehaviourManager; }  // Prepare threads it into MomentController::NewMoment

// The 16-byte candidate-moment record the selector stores by
// value in Array<MomentDescription,10>. Every field is declared AND console-attested:
//   +0x00 meMomentType     -- ArbStateRoaming::Construct writes 7/8/10 here (PLAYER_JUMPING,
//                             PLAYER_STUNT, NEW_CAR_JOINED) and Prepare forwards it as
//                             NewMoment's 1st arg.
//     +0x04 meMomentParamID  -- forwarded as NewMoment's 2nd arg; the roaming state passes 0.
//   +0x08 mfWeighting      -- read by SelectBestLRUMomentWithExclusion / Pick*Moment.
//     +0x0C mbCanBeInhibited -- read straight from +0x0C in Prepare's inhibit loop.
struct MomentDescription
{
    Moment::EType                       meMomentType;
    MomentParameterBank::EMomentParamID meMomentParamID;
    f32                                 mfWeighting;
    bool                                mbCanBeInhibited;
};

// No pointer members, so the console build 0x10 size holds on the x64 host build too.
static_assert(sizeof(MomentDescription) == 0x10, "MomentDescription layout drift");

class MomentSelector
{
public:
    // SelectBestMomentWithExclusion dispatches on
    // this (meSelectionMode @+0x1DC): 0 -> LRU, 1 -> random, anything else -> "unhandled type".
    enum ESelectionMode
    {
        E_MODE_LRU_BEST    = 0,
        E_MODE_RANDOM_BEST = 1,

        E_MODE_COUNT       = 2
    };

    enum EPickBestInhibitedOptions
    {
        E_PICK_BEST_FUSSY = 0,
        E_PICK_BEST_ANY   = 1
    };

    enum EPickWorstUninhibitedOptions
    {
        E_PICK_WORST_FUSSY = 0,
        E_PICK_WORST_ANY   = 1
    };

    // Capacity of all three parallel arrays. Console-pinned: AddMoment asserts
    // `mMomentDescriptionArray.GetLength() < ...GetCapacity()` against a literal 10, and the
    // count words sit at 10*sizeof(element) past each array base.
    static const u32 KU_MAX_MOMENTS = 10;

    // ---- lifecycle -------------------------------------------------------------------
    // Recovered from the copy the console build inlined into
    // ArbStateRoaming::Construct.
    void Construct();

    // Allocate a live Moment for every registered
    // description that has not got one yet, then apply the max-active inhibit policy.
    bool Prepare(MomentController& lrMomentController, Camera::BehaviourManager& lrBehaviourManager);

    // Hand every live moment back to the controller
    // and reset the per-activation state. Returns true.
    bool Release();

    // ---- candidate registration ------------------------------------------------------
    // The console passes the 16-byte record BY VALUE
    // in two integer argument slots; kept by value here.
    bool AddMoment(MomentDescription lMoment);

    // Field-wise convenience overload the console build inlines at every
    // call site. The declaration spells the parameters with the member-style names kept below.
    bool AddMoment(Moment::EType meMomentType,
                   MomentParameterBank::EMomentParamID meMomentParamID,
                   f32 mfWeighting,
                   bool mbCanBeInhibited);

    // ---- selection queries (header inlines; see the WHERE EACH BODY LIVES map above) ---
    // Named by the "HasSelectedMoment()" / "!HasSelectedMoment()"
    // assert strings baked into five separate console functions.
    // (the declaration makes it non-const; widened to const here so the const query sites compile --
    // strictly permissive, no behavioural difference.)
    bool HasSelectedMoment() const { return mbHasSelectedMoment; }

    // Assert HasSelectedMoment(), then return the
    // selected slot's moment. The console body inlines MomentHandle::GetMoment() verbatim
    // (its own "mbIsAllocated" assert, then the moment pointer read from the handle's +0x04).
    // FLAG: the declaration spells the return `Moment&`; kept as `Moment*` to match this tree's
    // established MomentController::MomentHandle::GetMoment() pointer convention. Same read.
    Moment* GetSelectedMoment() const
    {
        CGS_ASSERT(HasSelectedMoment(), "HasSelectedMoment()");
        return mMomentHandleArray[static_cast<u32>(miSelectedMoment)].GetMoment();
    }

    // Assert a moment is selected, then drop the flag
    // ONLY -- the console does not touch miSelectedMoment here.
    void CancelSelection()
    {
        CGS_ASSERT(HasSelectedMoment(), "HasSelectedMoment()");
        mbHasSelectedMoment = false;
    }

    // Pick with no exclusion (-1).
    bool SelectBestMoment(CgsNumeric::Random& lRandom)
    {
        CGS_ASSERT(!HasSelectedMoment(), "!HasSelectedMoment()");
        return SelectBestMomentWithExclusion(lRandom, -1);
    }

    //  defined out-of-class at the foot of this header
    // (the console's assert line 364 is well past the class body, so EA defined it there too).
    bool SelectNewBestMoment(CgsNumeric::Random& lRandom);

    // ---- tuning (header inlines) ------------------------------------------------------
    // The two compared constants are 0.0f and 1.0f, both named by the assert string itself.
    void SetRecencyFactor(f32 lfRecencyFactor01)
    {
        CGS_ASSERT(lfRecencyFactor01 >= 0.0f && lfRecencyFactor01 < 1.0f,
                   "lfRecencyFactor01 >= 0.0f && lfRecencyFactor01 < 1.0f");
        mfRecencyFactor = lfRecencyFactor01;
    }

    // Note the bound is the HANDLE array's live length
    // (console reads +0xA4+0xF0), not the description array's.
    void SetMaxActiveMoments(u32 luMaxMoments)
    {
        CGS_ASSERT(!mbPrepared, "!mbPrepared");
        CGS_ASSERT(luMaxMoments < mMomentHandleArray.GetLength(),
                   "luMaxMoments < mMomentHandleArray.GetLength()");
        muMaxActiveMomentLimit = luMaxMoments;
        mbHasMaxLimit          = true;
    }

    // DECLARATION-ONLY: declared API whose bodies still live only in the console build ----
    // Each is homed in BrnMomentSelector.cpp at the quoted line; none is referenced by any
    // body in this tree today, so leaving them undefined costs the link nothing. Calling one
    // WILL open an unresolved external until its body lands.
    // BODIED 2026-08-01 in BrnMomentSelector.cpp -- it is
    // called UNCONDITIONALLY as the first statement of ArbStateRoaming::Update's DRIVING arm.
    void Update(f32 lfTimestep);

    void Destruct();

    // ⭐  -- BODIED 2026-08-29 (crash-camera wave) as a header inline. It has NO standalone
    // console symbol because the console inlines it, and ArbStateCrashing::Construct
    // shows the whole body: after SetRecencyFactor / before SetMaxActiveMoments it emits one
    // bare zero store to the state's +0x3A4 == mMomentSelector + 0x1DC == meSelectionMode,
    // with no
    // call. (The store is redundant with Construct()'s own seed, which is exactly what an
    // inlined `SetSelectionMode(E_MODE_LRU_BEST)` right after `Construct()` looks like.)
    void SetSelectionMode(ESelectionMode leSelectionMode) { meSelectionMode = leSelectionMode; }

    // ⭐  -- BODIED 2026-08-29 (crash-camera wave) as a header inline, same reasoning: no
    // standalone console symbol, and ArbStateCrashing::Update case 0 (PREPARING's
    // predecessor arm) inlines it verbatim immediately before MomentSelector::Prepare --
    //     0.0f -> selector +0x1C4   ; mfTimeActive   = 0.0f
    //     0 -> selector +0x1C8   ; miFramesActive = 0
    // It was DECLARATION-ONLY, i.e. an unresolved external for the first caller that needed it.
    void ResetTimeActive() { mfTimeActive = 0.0f; miFramesActive = 0; }

    f32  GetTimeActive();

    // ⭐  -- BODIED 2026-08-29 (crash-camera wave) as a header inline. No standalone console
    // symbol; ArbStateCrashing::Prepare inlines it to a bare read of the state's +0x390
    // == selector +0x1C8 == miFramesActive, compared against 2. (The declaration types the
    // member u32 despite the `mi` prefix; the accessor's s32 return is the declaration's.)
    s32  GetFramesActive() const { return static_cast<s32>(miFramesActive); }

    // ⭐ BODIED 2026-08-29 in BrnMomentSelector.cpp -- it is on
    // ArbStateCrashing::Prepare's straight-line path (the "no frames active yet and no valid
    // moment" gate), so leaving it declaration-only was an unresolved external the moment the
    // crash camera mounted. It is the RE-COUNT, not the cached read: it walks the handle array
    // and STORES the result back into muValidMoments. Its cheap sibling is GetNumValidMoments.
    u32  SnoopNumValidMoments();

    //  BODIED 2026-08-01 as a header inline. It is the plain read of the cached count,
    // NOT the recount: ArbStateRoaming::Update's DRIVING arm does a bare
    // read of the state's +0x388 == mMomentSelector +0x1D0 == muValidMoments, with no call. (Its
    // sibling SnoopNumValidMoments is the RE-COUNT -- 130 instructions that walk the
    // handle array and STORE the result back into +0x1D0 -- so the two are genuinely different
    // functions and only this one matches the roaming arm's single load.)
    u32  GetNumValidMoments() const { return muValidMoments; }

    // ⭐  -- BODIED 2026-08-29 (crash-camera wave) as a header inline, same shape as the two
    // above: no standalone console symbol, and ArbStateCrashing::Update opens with the
    // inlined read of the state's +0x3A9 == selector +0x1E1 == mbPrepared, gating the
    // per-frame MomentSelector::Update call. ⚠️ That +0x3A9 is one of the offsets the
    // automatic C translation prints as a `field_3A9` on the OWNING STATE; it is this member.
    bool IsPrepared() const { return mbPrepared; }
    //  ⭐ BODIED 2026-08-29 (crash-camera wave) as the inline forwarder it is. The
    // console has no standalone symbol for it: ArbStateCrashing::Update emits a
    // DIRECT call to MomentSelector::ActualDebugRender, which is what an inlined public forwarder
    // onto a private worker looks like -- byte for byte the DebugPrinter::Print -> ActualPrint
    // shape this tree already recovered. Spelling the call site as ActualDebugRender instead
    // would be reaching a private member from outside the class.
    void DebugRender(DebugPrinter& lrDebugPrinter) const { ActualDebugRender(lrDebugPrinter); }

private:
    // DECLARATION-ONLY (same note as above). SelectBestMoment / SelectNewBestMoment above are
    // the only referrers, and they are header inlines -- so this only becomes an unresolved
    // external in a TU that actually calls one of those two.
    bool SelectBestMomentWithExclusion(CgsNumeric::Random& lRandom, s32 liExclusion);
    bool SelectBestLRUMomentWithExclusion(s32 liExclusion);
    bool SelectBestRandomMomentWithExclusion(CgsNumeric::Random& lRandom, s32 liExclusion);
    bool PickBestInhibitedMoment(u32* lpuIndex, EPickBestInhibitedOptions leOptions);
    bool PickWorstUninhibitedMoment(u32* lpuIndex, EPickWorstUninhibitedOptions leOptions);
    void ActualDebugRender(DebugPrinter& lrDebugPrinter) const;

    // Member list, in declaration order --------
    Array<MomentDescription, KU_MAX_MOMENTS>                  mMomentDescriptionArray; // +0x000
    Array<MomentController::MomentHandle, KU_MAX_MOMENTS>     mMomentHandleArray;      // +0x0A4
    Array<f32, KU_MAX_MOMENTS>                                mRecencyArray;           // +0x198

    f32            mfTimeActive;            // +0x1C4  (Release zeroes it)
    u32            miFramesActive;          // +0x1C8  (declaration types it uint32_t despite the mi prefix)
    f32            mfRecencyFactor;         // +0x1CC
    u32            muValidMoments;          // +0x1D0
    u32            muMaxActiveMomentLimit;  // +0x1D4
    s32            miSelectedMoment;        // +0x1D8
    ESelectionMode meSelectionMode;         // +0x1DC
    bool           mbHasSelectedMoment;     // +0x1E0
    bool           mbPrepared;              // +0x1E1
    bool           mbHasMaxLimit;           // +0x1E2
};

// The console reads miSelectedMoment BEFORE clearing
// mbHasSelectedMoment, then re-picks excluding it -- preserve that order.
inline bool MomentSelector::SelectNewBestMoment(CgsNumeric::Random& lRandom)
{
    CGS_ASSERT(HasSelectedMoment(), "HasSelectedMoment()");

    const s32 liExclusion = miSelectedMoment;
    mbHasSelectedMoment   = false;

    return SelectBestMomentWithExclusion(lRandom, liExclusion);
}

} // namespace BrnDirector
