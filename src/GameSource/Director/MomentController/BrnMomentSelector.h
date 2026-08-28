#pragma once

// Home for BrnDirector::MomentDescription and BrnDirector::MomentSelector -- the
// establishing-shot "moment" picker that each director arbitrator state embeds BY VALUE
// (ArbStateRoaming @X360 +0x1B8, ArbStateCrashing, ArbStateTakedown @X360 +0x42C).
//
// LAYOUT + SIGNATURE AUTHORITY: the DecFIGS DWARF
//   references/DecFIGS/dwarfdump/GameSource/Director/MomentController/BrnMomentSelector.h
// (real EA member list and declaration line numbers), cross-checked store-for-store against
// BURNOUT_X360_ARTIST.XEX. The console offsets quoted below are provenance only -- access is
// BY NAME (the x64-host gate widens the embedded arrays), never by byte offset.
//
//   X360 offset map (all attested by an instruction in this file's own functions):
//     +0x000  mMomentDescriptionArray   Array<MomentDescription,10>  (count word @+0x0A0 == 10*0x10)
//     +0x0A4  mMomentHandleArray        Array<MomentHandle,10>       (count word @+0x194 == 0xA4+10*0x18)
//     +0x198  mRecencyArray             Array<f32,10>                (count word @+0x1C0)
//     +0x1C4  mfTimeActive              +0x1C8  miFramesActive       +0x1CC  mfRecencyFactor
//     +0x1D0  muValidMoments            +0x1D4  muMaxActiveMomentLimit
//     +0x1D8  miSelectedMoment          +0x1DC  meSelectionMode
//     +0x1E0  mbHasSelectedMoment       +0x1E1  mbPrepared           +0x1E2  mbHasMaxLimit
//   The 0x18 MomentHandle stride is pinned by SetMaxActiveMoments @0x82208570, which reads the
//   handle array's count via `addi r31,r30,0xA4` + `lwz r11,0xF0(r31)` -> +0x194.
//
// WHERE EACH BODY LIVES (recovered from each function's own baked __FILE__/__LINE__ assert,
// which is the definition site -- project rule 12):
//   BrnMomentSelector.h:166  SetRecencyFactor      @0x821F57F0   (header inline, below)
//   BrnMomentSelector.h:172  SelectBestMoment      @0x82254DA8   (header inline, below)
//   BrnMomentSelector.h:182  GetSelectedMoment     @0x82219868   (header inline, below)
//   BrnMomentSelector.h:185  CancelSelection       @0x821F5868   (header inline, below)
//   BrnMomentSelector.h:189  SetMaxActiveMoments   @0x82208570   (header inline, below)
//   BrnMomentSelector.h:364  SelectNewBestMoment   @0x82254E18   (header inline, out-of-class below)
//   BrnMomentSelector.cpp:53  Prepare              @0x82255C58
//   BrnMomentSelector.cpp:222 Release              @0x8221BC90
//   BrnMomentSelector.cpp:280 AddMoment(MomentDescription) @0x82209F80
//   BrnMomentSelector.cpp:302 AddMoment(EType,EMomentParamID,f32,bool)  -- inlined at every
//                             X360 call site (no standalone symbol); shape recovered from
//                             ArbStateRoaming::Construct @0x82259C00, which builds the four
//                             MomentDescription words on the stack and passes them in r4:r5.

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Containers/CgsArray.h"                  // Array<T,N> (global ns; DWARF spells it CgsContainers::Array)
#include "GameSource/Director/MomentController/BrnMoment.h"              // Moment, Moment::EType
#include "GameSource/Director/MomentController/BrnMomentParameterBank.h" // MomentParameterBank::EMomentParamID
#include "GameSource/Director/MomentController/BrnMomentController.h"    // MomentController, MomentController::MomentHandle (by value)

namespace CgsNumeric { class Random; }   // SelectBestMoment / SelectNewBestMoment draw arg

namespace BrnDirector
{

struct DebugPrinter;                          // dev print sink (DebugRender / ActualDebugRender)
namespace Camera { class BehaviourManager; }  // Prepare threads it into MomentController::NewMoment

// DWARF BrnMomentSelector.h:39. The 16-byte candidate-moment record the selector stores by
// value in Array<MomentDescription,10>. Every field is DWARF-named AND asm-attested:
//   +0x00 meMomentType     -- ArbStateRoaming::Construct writes 7/8/10 here (PLAYER_JUMPING,
//                             PLAYER_STUNT, NEW_CAR_JOINED) and Prepare forwards it as
//                             NewMoment's 1st arg (r4).
//   +0x04 meMomentParamID  -- forwarded as NewMoment's 2nd arg (r5); the roaming state passes 0.
//   +0x08 mfWeighting      -- read by SelectBestLRUMomentWithExclusion / Pick*Moment.
//   +0x0C mbCanBeInhibited -- `lbz r11, 0xC(r3)` in Prepare's inhibit loop.
struct MomentDescription
{
    Moment::EType                       meMomentType;      // BrnMomentSelector.h:40
    MomentParameterBank::EMomentParamID meMomentParamID;   // BrnMomentSelector.h:41
    f32                                 mfWeighting;       // BrnMomentSelector.h:42
    bool                                mbCanBeInhibited;  // BrnMomentSelector.h:43
};

// No pointer members, so the X360 0x10 size holds on the x64 host build too.
static_assert(sizeof(MomentDescription) == 0x10, "MomentDescription layout drift");

// DWARF BrnMomentSelector.h:92.
class MomentSelector
{
public:
    // DWARF BrnMomentSelector.h:95. SelectBestMomentWithExclusion @0x82250FC8 dispatches on
    // this (meSelectionMode @+0x1DC): 0 -> LRU, 1 -> random, anything else -> "unhandled type".
    enum ESelectionMode
    {
        E_MODE_LRU_BEST    = 0,
        E_MODE_RANDOM_BEST = 1,

        E_MODE_COUNT       = 2
    };

    // DWARF BrnMomentSelector.h:215 / :221.
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

    // Capacity of all three parallel arrays. X360-pinned: AddMoment @0x82209F80 asserts
    // `mMomentDescriptionArray.GetLength() < ...GetCapacity()` with `cmplwi r11, 0xA`, and the
    // count words sit at 10*sizeof(element) past each array base.
    static const u32 KU_MAX_MOMENTS = 10;

    // ---- lifecycle -------------------------------------------------------------------
    // BrnMomentSelector.cpp:33. Recovered from the copy the X360 inlined into
    // ArbStateRoaming::Construct @0x82259CD0..0x82259CF8.
    void Construct();

    // BrnMomentSelector.cpp:53 (@0x82255C58). Allocate a live Moment for every registered
    // description that has not got one yet, then apply the max-active inhibit policy.
    bool Prepare(MomentController& lrMomentController, Camera::BehaviourManager& lrBehaviourManager);

    // BrnMomentSelector.cpp:222 (@0x8221BC90). Hand every live moment back to the controller
    // and reset the per-activation state. Returns true.
    bool Release();

    // ---- candidate registration ------------------------------------------------------
    // BrnMomentSelector.cpp:280 (@0x82209F80). The console passes the 16-byte record BY VALUE
    // in the r4:r5 GPR pair; kept by value here.
    bool AddMoment(MomentDescription lMoment);

    // BrnMomentSelector.cpp:302. Field-wise convenience overload the X360 inlines at every
    // call site. DWARF spells the parameters with the member-style names kept below.
    bool AddMoment(Moment::EType meMomentType,
                   MomentParameterBank::EMomentParamID meMomentParamID,
                   f32 mfWeighting,
                   bool mbCanBeInhibited);

    // ---- selection queries (header inlines; see the WHERE EACH BODY LIVES map above) ---
    // BrnMomentSelector.h:179. Named by the "HasSelectedMoment()" / "!HasSelectedMoment()"
    // assert strings baked into five separate X360 functions.
    // (DWARF declares it non-const; widened to const here so the const query sites compile --
    // strictly permissive, no behavioural difference.)
    bool HasSelectedMoment() const { return mbHasSelectedMoment; }

    // BrnMomentSelector.h:182 (@0x82219868): assert HasSelectedMoment(), then return the
    // selected slot's moment. The X360 body inlines MomentHandle::GetMoment() verbatim
    // (its own "mbIsAllocated" assert at BrnMomentController.h:141 then `lwz r3, 4(handle)`).
    // FLAG: DWARF spells the return `Moment&`; kept as `Moment*` to match this tree's
    // established MomentController::MomentHandle::GetMoment() pointer convention. Same read.
    Moment* GetSelectedMoment() const
    {
        CGS_ASSERT(HasSelectedMoment(), "HasSelectedMoment()");
        return mMomentHandleArray[static_cast<u32>(miSelectedMoment)].GetMoment();
    }

    // BrnMomentSelector.h:185 (@0x821F5868). Assert a moment is selected, then drop the flag
    // ONLY -- the console does not touch miSelectedMoment here.
    void CancelSelection()
    {
        CGS_ASSERT(HasSelectedMoment(), "HasSelectedMoment()");
        mbHasSelectedMoment = false;
    }

    // BrnMomentSelector.h:172 (@0x82254DA8). Pick with no exclusion (-1).
    bool SelectBestMoment(CgsNumeric::Random& lRandom)
    {
        CGS_ASSERT(!HasSelectedMoment(), "!HasSelectedMoment()");
        return SelectBestMomentWithExclusion(lRandom, -1);
    }

    // BrnMomentSelector.h:364 (@0x82254E18) -- defined out-of-class at the foot of this header
    // (the console's assert line 364 is well past the class body, so EA defined it there too).
    bool SelectNewBestMoment(CgsNumeric::Random& lRandom);

    // ---- tuning (header inlines) ------------------------------------------------------
    // BrnMomentSelector.h:166 (@0x821F57F0). The two compared constants are flt_82001CC0 (0.0f)
    // and flt_82001C98 (1.0f), both named by the assert string itself.
    void SetRecencyFactor(f32 lfRecencyFactor01)
    {
        CGS_ASSERT(lfRecencyFactor01 >= 0.0f && lfRecencyFactor01 < 1.0f,
                   "lfRecencyFactor01 >= 0.0f && lfRecencyFactor01 < 1.0f");
        mfRecencyFactor = lfRecencyFactor01;
    }

    // BrnMomentSelector.h:189 (@0x82208570). Note the bound is the HANDLE array's live length
    // (console reads +0xA4+0xF0), not the description array's.
    void SetMaxActiveMoments(u32 luMaxMoments)
    {
        CGS_ASSERT(!mbPrepared, "!mbPrepared");
        CGS_ASSERT(luMaxMoments < mMomentHandleArray.GetLength(),
                   "luMaxMoments < mMomentHandleArray.GetLength()");
        muMaxActiveMomentLimit = luMaxMoments;
        mbHasMaxLimit          = true;
    }

    // ---- DECLARATION-ONLY: DWARF-attested API whose bodies still live only in the X360 ----
    // Each is homed in BrnMomentSelector.cpp at the quoted line; none is referenced by any
    // body in this tree today, so leaving them undefined costs the link nothing. Calling one
    // WILL open an unresolved external until its body lands.
    // :113 -> cpp:115 (@0x82239FC0). BODIED 2026-08-01 in BrnMomentSelector.cpp -- it is
    // called UNCONDITIONALLY as the first statement of ArbStateRoaming::Update's DRIVING arm.
    void Update(f32 lfTimestep);

    void Destruct();                                      // :119 -> cpp:246

    // ⭐ :123 -- BODIED 2026-08-29 (crash-camera wave) as a header inline. It has NO standalone
    // X360 symbol because the console inlines it, and ArbStateCrashing::Construct @0x82259EA0
    // shows the whole body: after SetRecencyFactor / before SetMaxActiveMoments it emits one
    // bare `stw r31(0), 0x3A4(this)` == mMomentSelector + 0x1DC == meSelectionMode, with no
    // call. (The store is redundant with Construct()'s own seed, which is exactly what an
    // inlined `SetSelectionMode(E_MODE_LRU_BEST)` right after `Construct()` looks like.)
    void SetSelectionMode(ESelectionMode leSelectionMode) { meSelectionMode = leSelectionMode; }

    // ⭐ :142 -- BODIED 2026-08-29 (crash-camera wave) as a header inline, same reasoning: no
    // standalone X360 symbol, and ArbStateCrashing::Update @0x8226BFB0 case 0 (PREPARING's
    // predecessor arm) inlines it verbatim immediately before MomentSelector::Prepare --
    //     stfs flt_82001CC0(0.0), 0x1C4(selector)   ; mfTimeActive   = 0.0f
    //     stw  0,                 0x1C8(selector)   ; miFramesActive = 0
    // It was DECLARATION-ONLY, i.e. an unresolved external for the first caller that needed it.
    void ResetTimeActive() { mfTimeActive = 0.0f; miFramesActive = 0; }

    f32  GetTimeActive();                                 // :145

    // ⭐ :148 -- BODIED 2026-08-29 (crash-camera wave) as a header inline. No standalone X360
    // symbol; ArbStateCrashing::Prepare @0x822655E8 inlines it to a bare `lwz r11, 0x390(state)`
    // == selector +0x1C8 == miFramesActive, compared against 2. (DWARF types the member u32
    // despite the `mi` prefix; the accessor's s32 return is the DWARF's.)
    s32  GetFramesActive() const { return static_cast<s32>(miFramesActive); }

    // :154 -> cpp:255 (@0x8221BD28). ⭐ BODIED 2026-08-29 in BrnMomentSelector.cpp -- it is on
    // ArbStateCrashing::Prepare's straight-line path (the "no frames active yet and no valid
    // moment" gate), so leaving it declaration-only was an unresolved external the moment the
    // crash camera mounted. It is the RE-COUNT, not the cached read: it walks the handle array
    // and STORES the result back into muValidMoments. Its cheap sibling is GetNumValidMoments.
    u32  SnoopNumValidMoments();

    // :151 -- BODIED 2026-08-01 as a header inline. It is the plain read of the cached count,
    // NOT the recount: ArbStateRoaming::Update's DRIVING arm @0x822644D4 does a bare
    // `lwz r10, 0x388(r31)` == mMomentSelector +0x1D0 == muValidMoments, with no call. (Its
    // sibling SnoopNumValidMoments @0x8221BD28 is the RE-COUNT -- 130 asm lines that walk the
    // handle array and STORE the result back into +0x1D0 -- so the two are genuinely different
    // functions and only this one matches the roaming arm's single load.)
    u32  GetNumValidMoments() const { return muValidMoments; }

    // ⭐ :157 -- BODIED 2026-08-29 (crash-camera wave) as a header inline, same shape as the two
    // above: no standalone X360 symbol, and ArbStateCrashing::Update @0x8226BFB0 opens with the
    // inlined `lbz r11, 0x3A9(state)` == selector +0x1E1 == mbPrepared, gating the per-frame
    // MomentSelector::Update call. ⚠️ That +0x3A9 is one of the offsets Hex-Rays prints as a
    // `field_3A9` on the OWNING STATE; it is this member.
    bool IsPrepared() const { return mbPrepared; }
    // :193 -- ⭐ BODIED 2026-08-29 (crash-camera wave) as the inline forwarder it is. The
    // console has no standalone symbol for it: ArbStateCrashing::Update @0x8226BFB0 emits a
    // DIRECT `bl MomentSelector::ActualDebugRender`, which is what an inlined public forwarder
    // onto a private worker looks like -- byte for byte the DebugPrinter::Print -> ActualPrint
    // shape this tree already recovered. Spelling the call site as ActualDebugRender instead
    // would be reaching a private member from outside the class.
    void DebugRender(DebugPrinter& lrDebugPrinter) const { ActualDebugRender(lrDebugPrinter); }

private:
    // DECLARATION-ONLY (same note as above). SelectBestMoment / SelectNewBestMoment above are
    // the only referrers, and they are header inlines -- so this only becomes an unresolved
    // external in a TU that actually calls one of those two.
    bool SelectBestMomentWithExclusion(CgsNumeric::Random& lRandom, s32 liExclusion);   // :200 -> cpp:320 (@0x82250FC8)
    bool SelectBestLRUMomentWithExclusion(s32 liExclusion);                             // :206 -> cpp:351 (@0x8221BE50)
    bool SelectBestRandomMomentWithExclusion(CgsNumeric::Random& lRandom, s32 liExclusion); // :213 -> cpp:406 (@0x8223A668)
    bool PickBestInhibitedMoment(u32* lpuIndex, EPickBestInhibitedOptions leOptions);    // :230 -> cpp:454 (@0x8221C028)
    bool PickWorstUninhibitedMoment(u32* lpuIndex, EPickWorstUninhibitedOptions leOptions); // :235 -> cpp:524 (@0x8221C358)
    void ActualDebugRender(DebugPrinter& lrDebugPrinter) const;                         // :239 -> cpp:595 (@0x8221C6D8)

    // ---- DWARF member list, in declaration order (BrnMomentSelector.h:250..266) --------
    Array<MomentDescription, KU_MAX_MOMENTS>                  mMomentDescriptionArray; // :250  X360 +0x000
    Array<MomentController::MomentHandle, KU_MAX_MOMENTS>     mMomentHandleArray;      // :251  X360 +0x0A4
    Array<f32, KU_MAX_MOMENTS>                                mRecencyArray;           // :253  X360 +0x198

    f32            mfTimeActive;            // :255  X360 +0x1C4  (Release zeroes it)
    u32            miFramesActive;          // :256  X360 +0x1C8  (DWARF types it uint32_t despite the mi prefix)
    f32            mfRecencyFactor;         // :257  X360 +0x1CC
    u32            muValidMoments;          // :258  X360 +0x1D0
    u32            muMaxActiveMomentLimit;  // :259  X360 +0x1D4
    s32            miSelectedMoment;        // :260  X360 +0x1D8
    ESelectionMode meSelectionMode;         // :262  X360 +0x1DC
    bool           mbHasSelectedMoment;     // :264  X360 +0x1E0
    bool           mbPrepared;              // :265  X360 +0x1E1
    bool           mbHasMaxLimit;           // :266  X360 +0x1E2
};

// BrnMomentSelector.h:364 (@0x82254E18). The console reads miSelectedMoment BEFORE clearing
// mbHasSelectedMoment, then re-picks excluding it -- preserve that order.
inline bool MomentSelector::SelectNewBestMoment(CgsNumeric::Random& lRandom)
{
    CGS_ASSERT(HasSelectedMoment(), "HasSelectedMoment()");

    const s32 liExclusion = miSelectedMoment;
    mbHasSelectedMoment   = false;

    return SelectBestMomentWithExclusion(lRandom, liExclusion);
}

} // namespace BrnDirector
