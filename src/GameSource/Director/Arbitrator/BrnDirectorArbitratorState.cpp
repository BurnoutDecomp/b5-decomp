// ============================================================================
// GameSource/Director/Arbitrator/BrnDirectorArbitratorState.cpp
//
// BrnDirector::ArbitratorState -- the abstract base every arbitrator state derives from.
//
// WHY THIS TU EXISTS: the base's own out-of-line members had NO home anywhere in the tree,
// so the moment the director spine joined the exe link every derived state failed with
// "unresolved external ArbitratorState::<virtual>" (the derived vtables reference the base
// slots the derived class does not override, and every derived ctor calls the base ctor).
//
// AUTHORITY: the DecFIGS DWARF for this file
// (references/DecFIGS/dwarfdump/GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h)
// splits the base's members by DEFINITION SITE, which is what tells us which ones are real
// out-of-line bodies and which are one-line defaults declared in the class body:
//
//   ArbitratorState()   BrnDirectorArbitratorState.h:93    <- in-class, empty
//   Construct()         BrnDirectorArbitratorState.cpp:27  <- OUT-OF-LINE (bodied here)
//   Prepare(info)       BrnDirectorArbitratorState.h:104   <- in-class one-liner
//   Update(info)        BrnDirectorArbitratorState.h:108   <- in-class one-liner
//   PostUpdate(info)    BrnDirectorArbitratorState.cpp:41   (no caller in the committed tree)
//   Release(info)       BrnDirectorArbitratorState.cpp:58  <- OUT-OF-LINE (bodied here)
//   Destruct()          BrnDirectorArbitratorState.h:119   <- in-class one-liner
//   CanRun(info) const  BrnDirectorArbitratorState.cpp:68  <- OUT-OF-LINE (bodied here)
//   CycleCamera()       BrnDirectorArbitratorState.cpp:49   (no caller in the committed tree)
//   GetName() const     BrnDirectorArbitratorState.h:130   <- in-class one-liner
//
// The five in-class one-liners stay in the header (declared there, defined here only because
// this project's header keeps them non-inline). The X360 export set contains NO symbol for any
// of them -- every one is inlined into its caller or folded by ICF -- so their BODIES are
// SHAPE-ATTESTED from the call sites, not transcribed. Each is flagged individually below.
// None of them is ever reached for a state that overrides it, which every shipped state does
// for Construct/Update/GetName.
//
// DELETE-WHEN: an ARTIST export (or a second DWARF drop with locals) pins the three
// out-of-line bodies; replace the flagged approximations with the transcription.
// ============================================================================

#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"

namespace BrnDirector
{
    // ------------------------------------------------------------------------
    // BrnDirectorArbitratorState.cpp:27  --  Construct()
    //
    // The base half of every derived Construct(). Two independent attestations fix its
    // content:
    //   * the container's ConstructAll @0x8224F020 calls vtable slot 0 on a state that has
    //     only just been placement-constructed, and every derived Construct() in this tree
    //     opens by zeroing the two base flag bytes (console +0x170 / +0x171) and constructing
    //     the by-value camera at +0x10 -- those stores ARE the inlined base;
    //   * Camera::Construct @0x8220A5B8 is the only initialiser mCamera has (its C++ ctor is
    //     `= default`), and ArbitratorState::GetCamera() hands the camera straight to
    //     consumers, so it cannot be left uninitialised.
    // ------------------------------------------------------------------------
    void ArbitratorState::Construct()
    {
        mCamera.Construct();
        ResetBaseCameraFlags();
    }

    // ------------------------------------------------------------------------
    // BrnDirectorArbitratorState.cpp:58  --  Release(ArbStateSharedInfo&)
    //
    // "Have you finished releasing?" -- the staged-release protocol every director object
    // uses (MainDirector::Release / DirectorModule::Release / BehaviourManager's release
    // walk all share it): return false to be called again next frame, true when done.
    // ArbitratorStateContainer::ReleaseAll @0x821F5EA0 calls it in a loop and ASSERTS the
    // result (BrnDirectorArbitratorStateContainer.cpp:120-121), so a base state that owns
    // nothing must report "already released" on the first call.
    // FLAG (shape-attested): no ARTIST symbol survives for this body; the return value is
    // pinned by the caller's assert, the empty body by the base owning no releasable state.
    // ------------------------------------------------------------------------
    bool ArbitratorState::Release(ArbStateSharedInfo& lrSharedInfo)
    {
        (void)lrSharedInfo;
        return true;
    }

    // ------------------------------------------------------------------------
    // BrnDirectorArbitratorState.cpp:68  --  CanRun(ArbStateSharedInfo&) const
    //
    // The arbitrator's admission test. Only states with a real precondition override it
    // (ArbStateCrashing::CanRun @0x821F6258 is one of the few that exists as its own symbol);
    // the base answers "no reason not to".
    // FLAG (shape-attested): pinned by the override pattern, not by a transcription.
    // ------------------------------------------------------------------------
    bool ArbitratorState::CanRun(ArbStateSharedInfo& lrSharedInfo) const
    {
        (void)lrSharedInfo;
        return true;
    }

    // ---- the five in-class one-liners (DWARF: defined at BrnDirectorArbitratorState.h:93 /
    //      :104 / :108 / :119 / :130). Kept out-of-line here so the header stays a pure
    //      declaration; the bodies are the header's own defaults. ---------------------------

    // .h:93 -- the C++ ctor. mCamera is `= default`-constructible and the two flags are set by
    // Construct(), which the container always runs before any state is used.
    ArbitratorState::ArbitratorState()
        : mCamera()
        , mbDebugDisplayActive(false)
        , mbCycleCameraThisFrame(false)
    {
    }

    // .h:104 -- "am I ready to become the active state?". Every state with real preparation
    // work overrides this; the base is ready immediately.
    // FLAG (shape-attested): return value pinned by Arbitrator::Update's
    // E_STATE_PREPARING -> E_STATE_ACTIVE edge, which advances only on true.
    bool ArbitratorState::Prepare(ArbStateSharedInfo& lrSharedInfo)
    {
        (void)lrSharedInfo;
        return true;
    }

    // .h:108 -- the per-frame tick. The base drives no camera.
    void ArbitratorState::Update(ArbStateSharedInfo& lrSharedInfo)
    {
        (void)lrSharedInfo;
    }

    // .h:119 -- the explicit (non-C++-destructor) teardown slot. The base owns nothing.
    void ArbitratorState::Destruct()
    {
    }

    // .h:130 -- the debug name. Every shipped state overrides it with its own literal (the
    // whole family sits together in .rodata at 0x821F62E0..0x821F6740); the base's own string
    // is not among them, so this literal is FLAGged as a stand-in rather than transcribed.
    const char* ArbitratorState::GetName() const
    {
        return "ArbitratorState";
    }
}
