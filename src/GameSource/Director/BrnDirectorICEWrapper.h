#ifndef BRN_DIRECTOR_ICE_WRAPPER_H
#define BRN_DIRECTOR_ICE_WRAPPER_H

#include "types.hpp"
#include "ppmalloc/EAGeneralAllocator.h"                 // EA::Allocator::GeneralAllocator (two embedded heaps)
#include "SDKs/Packages/ICE/ICEManager.hpp"              // ICE::ICEManager (embedded by value)
#include "GameSource/Director/Camera/ICECameraMover.h"   // ICE::ICECameraMover (embedded by value)
#include "GameSource/Director/Camera/Camera.h"           // BrnDirector::Camera::Camera (GetCamera return)
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"  // CgsResource::ID (PlayMovie take id)
#include "GameSource/Director/Utils/BrnVehicleRef.h"     // BrnDirector::VehicleRef::EType (PlayMovie ref type)
#include "GameSource/BurnoutConstants.h"                 // EActiveRaceCarIndex (PlayMovie race car)

// ============================================================================
// GameSource/Director/BrnDirectorICEWrapper.h
//
// BrnDirector::ICEWrapper -- the director-side owner of the ICE (In-game Camera
// Editor) runtime. It embeds, by value: two general-purpose heaps the ICE system
// allocates from, the top-level ICE::ICEManager (the live camera takes + the
// embedded editor/controller), and the ICE::ICECameraMover that drives the active
// camera from the manager's current take. The director constructs one of these,
// turns the in-game editor on/off in response to dev-tool messages, and rebuilds
// the camera mover each frame the dev tools are active.
//
// ----------------------------------------------------------------------------
// LAYOUT (member NAMES from this subsystem's role; OFFSETS/ORDER derived from the
// four reconstructed functions' behavior). Members are accessed BY NAME; the
// offsets are recorded for provenance, never used as casts. Because the two heaps,
// the manager, and the mover are embedded BY VALUE and our rebuilt sizes differ,
// the relative offsets below are NOT reproduced with padding across those members
// -- they are declared in their recorded ORDER and parity is by name.
//
//   +0x00008  GeneralAllocator mEditHeap          (ctor builds the heap engine)
//   +0x00528  GeneralAllocator mScratchHeap        (ctor builds the heap engine)
//   +0x00A40  ICEManager       mICEManager         (ctor default-constructs it)
//   +0x11BC8  s32              miReconstructFrame   (ctor stores 0x7FFFFFFF == S32_MAX;
//                                                    the "never reconstructed yet" frame
//                                                    sentinel, 8 bytes before the mover)
//   +0x11BD0  ICECameraMover   mCameraMover         (ctor default-constructs it)
//   +0x11D60  (mover-input block A; passed by address to ICECameraMover::Construct)
//   +0x11ED0  (mover-input block B; passed by address to ICECameraMover::Construct)
//
// FLAG: the leading 8 bytes (the +0x0..0x8 region, before mEditHeap) and the region
//   between mICEManager and miReconstructFrame are NOT touched by these four
//   functions and their contents are unknown; they are left unmodelled (no
//   speculative members -- GROW this header when a function that reads them is
//   reconstructed).
// FLAG: maMoverInputA / maMoverInputB are the two ICEWrapper members at
//   +0x11D60 / +0x11ED0 that ReconstructCameraMover passes BY ADDRESS to
//   ICE::ICECameraMover::Construct. Their element types are not yet recovered (the
//   Construct callee is a separate, not-yet-reconstructed TU), so they are modelled
//   as opaque byte blocks, named, and passed by their (decayed) address into the
//   void* Construct parameters -- accessed by name, never poked through an offset.
//   The byte sizes are placeholders sufficient to take their address; replace with
//   the real types when ICECameraMover::Construct is reconstructed.
// ============================================================================

namespace BrnDirector
{

class ICEWrapper
{
public:
    // Construct the wrapper: builds the two heaps, the ICE manager, the camera mover,
    // and seeds the reconstruct-frame sentinel to S32_MAX.
    ICEWrapper();

    // Turn the in-game ICE editor ON for editor mode liMode. Disables the debug
    // console for the editing session, clears any active movie playback, then drives
    // the manager's editor into the requested mode.
    void EditorOn(s32 liMode);

    // Turn the in-game ICE editor OFF. Re-enables the debug console; if the editor is
    // currently active it transitions the editor back to its idle/play state.
    void EditorOff();

    // Rebuild the camera mover for this frame from the manager's currently-active
    // take. liContext is the per-call director context the mover blends against.
    void ReconstructCameraMover(s32 liContext);

    // ------------------------------------------------------------------------
    // FLAG: minimal-slice decls the ICE movie-player family (BrnICEMoviePlayer)
    // drives through an ICEWrapper*. DECLARATION-ONLY -- bodies land with the ICE
    // wrapper / manager TUs that own them; the per-TU `cl /c` gate does not link.
    // ------------------------------------------------------------------------

    // Start playing a recorded ICE take. Signature order matches the DWARF: the take
    // id, then the normalised start position, then the vehicle-ref type the take is
    // anchored to, then the active race car it targets.
    void PlayMovie(CgsResource::ID lTakeId, f32 lfStartPosition,
                   VehicleRef::EType leVehicleRefType, EActiveRaceCarIndex leRaceCar);

    // True while a take started by PlayMovie is still playing.
    bool IsPlayingMovie();

    // The wrapper's live take camera (the player copies it / blends from it).
    const Camera::Camera& GetCamera();

    // NOTE: the sibling header BrnDirectorResourceManager.h calls inlined wrapper
    // accessors (GetICETakeData / GetShakeGroup) through an ICEWrapper*. Those are
    // NOT among this TU's four ledger functions (they are folded into their
    // callers), and they reach types not yet reconstructed (ICEGroup), so they are
    // intentionally NOT declared here -- they belong to the ICE-takedata accessor
    // follow-on, which will GROW this class when those return types exist. Adding
    // them now would be a speculative, unattested signature.

private:
    // +0x0008 / +0x0528  The two heaps the ICE system allocates from. The ctor builds
    // each with its inline configuration; in this rebuild they default-construct (the
    // GeneralAllocator engine's Init/AddCore configuration is the heap-setup
    // follow-on -- parity is by named member, not the ctor params).
    EA::Allocator::GeneralAllocator mEditHeap;      // +0x0008
    EA::Allocator::GeneralAllocator mScratchHeap;   // +0x0528

    // +0x0A40  The top-level ICE manager (live takes + embedded editor/controller).
    ICE::ICEManager mICEManager;

    // +0x11BC8  Frame index of the last camera-mover reconstruction. Seeded to
    // S32_MAX (0x7FFFFFFF) by the ctor -- the "not reconstructed this frame yet"
    // sentinel. Stored 8 bytes ahead of the mover.
    s32 miReconstructFrame;

    // +0x11BD0  Drives the active camera from the manager's current take.
    ICE::ICECameraMover mCameraMover;

    // +0x11D60 / +0x11ED0  The two mover-input blocks ReconstructCameraMover hands to
    // ICECameraMover::Construct by address (see FLAG above -- opaque until that callee
    // is reconstructed). Sizes are placeholders chosen only to be addressable.
    u8 maMoverInputA[0x170];   // +0x11D60
    u8 maMoverInputB[0x80];    // +0x11ED0
};

} // namespace BrnDirector

#endif // BRN_DIRECTOR_ICE_WRAPPER_H
