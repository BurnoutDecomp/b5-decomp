// ============================================================================
// SDKs/Packages/ICE/ICEManager.cpp
//
// ICE::ICEManager -- the top-level owner/driver of the active ICE camera takes.
// Reconstructed from the X360 ARTIST 2007-02 spine against the frozen layout in
// ICEManager.hpp:
//   Construct      @0x8253DCF0   build the four takes + the controller, init state
//   Destruct       @0x825333F0   tear down the controller menus, null owned pointers
//   GetCameraTake  @0x8252CAD0   select the active take (playback vs editor)
//   Update         @0x82540010   drive the controller, then advance movie playback
//
// The reconstruction reverses compiler inlining of the embedded ICETake /
// ICEController / ICETimer accesses back into logical member calls (semantic
// parity by named members; offsets in ICEManager.hpp are X360 facts, not used as
// casts here). Asserts use CGS_ASSERT (the baked X360 path/line are dropped --
// the macro supplies __FILE__/__LINE__).
// ============================================================================

#include "SDKs/Packages/ICE/ICEManager.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace ICE
{

// ---------------------------------------------------------------------------
// ICEManager (ctor) @0x827E13B8
//
// Constructs the embedded takes. In the X360 build this default-constructs the
// four live takes (mTake / mCopyTake / mShakeTake / mPlaybackTake) and inlines the
// embedded controller's construction (its own takes and a pair of cleared state
// flags). Here the controller and all takes are members, so member construction
// performs the same work: each ICETake default-constructs (zeroing its value table
// and defaulting its Cubic1D followers), and mController constructs in turn. The
// scalar playback state is left for Construct(ICEPointers*) to initialise.
//
// FLAG: the controller's own construction (its second take and the two state flags
//       the X360 ctor zeroes at controller+0x7260/+0x7262) belongs to the
//       ICEController TU; this manager ctor invokes it via member construction once
//       ICEController is reconstructed to its full layout (it is a minimal slice).
// ---------------------------------------------------------------------------
ICEManager::ICEManager()
{
}

// ---------------------------------------------------------------------------
// Construct (@0x8253DCF0)
//
// Initialise the manager from the ICEPointers bundle:
//   * zero the generic-camera playback state (group hash = 0, take index = -1),
//     clear the movie-playback flag and the smooth-exit flag,
//   * cache the three subsystem pointers (memory manager, file/text sink, timer)
//     out of the bundle,
//   * Construct all four live takes against the bundle's resource manager,
//   * Construct the embedded controller from the whole bundle.
//
// X360 stores (this-relative): +0x1CEC=0 (hash), +0x1CE8=-1 (index), +0x1CF0=0
// (smooth exit, byte), +0x1CE0=0 (playback flag, byte); the three pointers come
// from a2[0]/a2[1]/a2[4] (mpICEMemory/mpICEFileHandler/mpICETimer) stored at
// +0x1CFC/+0x1CF8/+0x1CF4; each ICETake::Construct is passed a2[5]
// (mpResourceManager). The DWARF spells the bundle ICEPointers (ICEMemory.hpp).
// ---------------------------------------------------------------------------
void ICEManager::Construct(ICEPointers* lpICEPointers)
{
    // Generic-camera ("play by name/index") state -- nothing queued yet.
    muPlayGenericGroupHash = 0;
    miPlayGenericTakeIndex = -1;

    // Not playing back a movie, and no smooth exit pending.
    mbPlaybackDataSet = false;
    mbSmoothExit      = false;

    // Cache the subsystem pointers out of the bundle.
    mpICEMemory   = lpICEPointers->mpICEMemory;
    mpFileHandler = lpICEPointers->mpICEFileHandler;
    mpTimer       = lpICEPointers->mpICETimer;

    // Build the four live takes against the bundle's resource manager. The X360
    // passes a2[5] (mpResourceManager) to each ICETake::Construct (an inlined
    // const IResourceManager*).
    const IResourceManager* lpResourceManager = lpICEPointers->mpResourceManager;
    mTake.Construct(lpResourceManager);
    mCopyTake.Construct(lpResourceManager);
    mShakeTake.Construct(lpResourceManager);
    mPlaybackTake.Construct(lpResourceManager);

    // Build the embedded editor/driver from the whole bundle.
    mController.Construct(lpICEPointers);
}

// ---------------------------------------------------------------------------
// Destruct (@0x825333F0)
//
// Tear down: drop the two owned subsystem pointers the manager cached (the file
// sink and the ICE memory manager -- the X360 zeroes +0x1CF8 / +0x1CFC), then let
// the controller destruct its menus and release the three editor pointers it owns.
//
// X360: stw 0 at +0x1CF8 and +0x1CFC, then ICEController::DestructMenus(this+0x1D10),
// then `if (p) p = 0;` over the controller's +0xF70/+0xF74/+0xF78 pointers.
// ---------------------------------------------------------------------------
void ICEManager::Destruct()
{
    // Release the manager-cached subsystem pointers (ownership passes back to the
    // bundle owner; the manager no longer references them).
    mpFileHandler = nullptr;
    mpICEMemory   = nullptr;

    // Tear down the editor menus.
    mController.DestructMenus();

    // Null the three editor pointers the controller owns (guarded clears, matching
    // the X360 `if (p) p = 0;` sequence over controller +0xF70/+0xF74/+0xF78).
    if (mController.mpMenuB)
        mController.mpMenuB = nullptr;   // +0xF70
    if (mController.mpMenuA)
        mController.mpMenuA = nullptr;   // +0xF74 (read first in the X360, lower addr cleared after)
    if (mController.mpMenuC)
        mController.mpMenuC = nullptr;   // +0xF78
}

// ---------------------------------------------------------------------------
// GetCameraTake (@0x8252CAD0)
//
// Select the take that currently drives the camera: while a movie is playing back
// (mbPlaybackDataSet) it is the playback take; otherwise it is the controller's
// live edit take (X360 returns controller+0x28 == &mController.mEditTake).
// ---------------------------------------------------------------------------
ICETake* ICEManager::GetCameraTake()
{
    if (mbPlaybackDataSet)
        return &mPlaybackTake;

    return mController.GetEditTake();
}

// ---------------------------------------------------------------------------
// Update (@0x82540010)
//
// Per-frame pass:
//   1. If the editor/menus are active (controller miMenusActive > 0), run the
//      controller's editor update and DO NOTHING ELSE (the editor owns the camera).
//   2. Otherwise, if a movie is playing back (mbPlaybackDataSet), advance the
//      playback take's parameter by this frame's timestep:
//        increment = (timestep / takeLength);
//        newParameter = clamp(currentParameter + increment, .., 1.0);
//      and when it reaches 1.0, stop playback (clear mbPlaybackDataSet). The new
//      parameter is pushed into the playback take via SetParameter(p, false, false).
//
// Asserts (faithful to the X360): a zero-length movie is illegal, and a single
// frame's increment must be a small (sub-1.0) fraction -- both fire BeginAssert/
// FireAssert/EndAssert via CGS_ASSERT.
//
// FLAG: the X360 calls the playback take's parameter setter via sub_82534118
// (an unnamed thunk to the ICETake parameter-advance: r3=&mPlaybackTake, f1=param,
// r5=0, r6=0). That shape matches ICETake::SetParameter(f32, bool, bool); the
// pseudocode's stray `a2` arg is a Hex-Rays artifact (r4 is never set before the
// call). Reconstructed as mPlaybackTake.SetParameter(newParameter, false, false).
// ---------------------------------------------------------------------------
void ICEManager::Update()
{
    // 1. Editor active -> let the controller drive, then bail.
    if (mController.AreMenusActive())
    {
        mController.Update();
    }

    // 2. Not editing and a movie is queued -> advance playback.
    if (!mController.AreMenusActive() && mbPlaybackDataSet)
    {
        // The movie length comes from the playback take's bound take data.
        const ICETakeData* lpTakeData = mPlaybackTake.GetData();
        f32 lfLength = lpTakeData ? lpTakeData->GetLength() : 0.0f;

        CGS_ASSERT(lfLength != 0.0f, "playing back a ICE movie of zero length");

        // Parameter advance for this frame: timestep scaled by 1/length.
        const f32 lfIncrement = (1.0f / lfLength) * mpTimer->GetTimestep();

        // A single frame must not advance more than the whole movie. The X360
        // tests |increment| against ~1.1920929e-7 (FLT_EPSILON) -- i.e. the
        // increment must be (nearly) zero to be "too long"; preserved verbatim.
        const f32 lfFltEpsilon = 0.00000011920929f;
        const bool lbTooLong = (lfIncrement <= lfFltEpsilon) && (lfIncrement >= -lfFltEpsilon);
        CGS_ASSERT(!lbTooLong, "ICE movie is too long to playback");

        // Step the parameter and clamp at the end of the movie.
        f32 lfParameter = mPlaybackTake.GetParameter() + lfIncrement;
        if (lfParameter >= 1.0f)
        {
            lfParameter       = 1.0f;
            mbPlaybackDataSet = false;   // movie finished -> stop playing back
        }

        mPlaybackTake.SetParameter(lfParameter, false, false);
    }
}

} // namespace ICE
