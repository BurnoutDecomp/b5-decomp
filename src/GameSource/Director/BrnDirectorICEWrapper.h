#ifndef BRN_DIRECTOR_ICE_WRAPPER_H
#define BRN_DIRECTOR_ICE_WRAPPER_H

#include "types.hpp"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"          // CgsMemory::HeapMalloc (base heap + embedded scratch heap)
#include "SDKs/Packages/ICE/ICEManager.hpp"                       // ICE::ICEManager (embedded by value)
#include "SDKs/Packages/ICE/ICEActionQueue.hpp"                   // ICE::ActionQueue / ICE::EControlToICEAction (dev-input queue)
#include "SDKs/Packages/ICE/ICECameraSpaceHandler.hpp"            // ICE::CameraSpaceHandler (per-frame reference spaces)
#include "SDKs/Packages/ICE/ICECamera.hpp"                        // ICE::ICECamera (embedded by value)
#include "SDKs/Packages/ICE/ICEData.hpp"                          // ICE::ICETakeData (GetICETakeData return)
#include "GameSource/Director/Camera/ICECameraMover.h"            // ICE::ICECameraMover (embedded by value)
#include "GameSource/Director/Camera/Camera.h"                    // BrnDirector::Camera::Camera (GetCamera return)
#include "GameSource/Director/Camera/Utils/BrnDebugController.h"  // Camera::Utils::DebugController
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h" // CgsResource::ID (PlayMovie take id / mCurrentMovieID)
#include "GameSource/Director/Utils/BrnVehicleRef.h"              // BrnDirector::VehicleRef (mVehicleRef + EType)
#include "GameSource/BurnoutConstants.h"                          // EActiveRaceCarIndex (PlayMovie race car)

// ============================================================================
// GameSource/Director/BrnDirectorICEWrapper.h
//
// BrnDirector::ICEWrapper -- the director-side owner of the ICE (In-game Camera
// Editor) runtime. It IS a general-purpose heap (CgsMemory::HeapMalloc base) the
// ICE system allocates from, embeds a second scratch heap, the top-level
// ICE::ICEManager (the live camera takes + the embedded editor/controller), the
// ICE::ICECameraMover that drives the active camera from the manager's current
// take, the per-frame reference-space cache, the ICE camera, the dev-tools action
// queue and the vehicle the takes anchor to. The director constructs one of these,
// plays recorded camera takes ("movies") through it, advances it each frame, and
// turns the in-game editor on/off in response to dev-tool messages.
//
// ----------------------------------------------------------------------------
// LAYOUT (member NAMES from this subsystem's role; OFFSETS/ORDER derived from the
// reconstructed functions' member accesses -- the ctor, Construct, Destruct,
// PlayMovie, Update, GetCurrentMovie, IsPlayingMovie and UpdateAction). Members are
// accessed BY NAME; the offsets are recorded for
// provenance, never used as casts. The heaps, manager, mover, camera, space handler
// and action queue are embedded BY VALUE and our rebuilt sizes differ from the
// recorded build, so the relative offsets below are NOT reproduced with padding --
// members are declared in their recorded ORDER and parity is by name.
//
//   +0x00000  CgsMemory::HeapMalloc  (base)        general heap; Destruct calls
//                                                  HeapMalloc::Destruct(this) at +0x0,
//                                                  ctor builds its allocator at +0x08
//   +0x00520  CgsMemory::HeapMalloc  mScratchHeap  second heap (ctor builds its
//                                                  allocator at +0x528)
//   +0x00A40  ICE::ICEManager        mICEManager   live takes + embedded editor
//   +0x09B20  f32                    mfTimeScale   per-frame sim-time scale
//   +0x11B24  DirectorResourceManager* mpResourceManager  take-data provider (PlayMovie)
//   +0x11B28  ICE::ActionQueue       mActionQueue  dev-tools input action stack
//                                                  (its miLength word lands at +0x11BC8;
//                                                  the ctor seeds it to the Stack's
//                                                  unconstructed sentinel, Construct Clears it)
//   +0x11BD0  ICE::ICECameraMover    mCameraMover  drives the camera from the active take
//   +0x11D60  ICE::ICECamera         mICECamera    the ICE camera (its embedded Camera at +0x10
//                                                  lands at +0x11D70 -- the Camera::Construct
//                                                  target Construct() pokes)
//   +0x11ED0  ICE::CameraSpaceHandler mCameraSpaceHandler  per-frame reference spaces
//   +0x120F0  BrnDirector::VehicleRef mVehicleRef  the vehicle the active take anchors to
//   +0x12100  CgsResource::ID        mCurrentMovieID  id of the take currently playing (8 bytes)
//   +0x12108  bool                   mbAcceptInput  dev-tools input gate for UpdateAction
//
// RECONCILED from the earlier placeholder layout: the old miReconstructFrame
// (+0x11BC8) / maMoverInputA (+0x11D60) / maMoverInputB (+0x11ED0) placeholders are
// RETIRED. The reconstructed functions prove +0x11BC8 is the action queue's own
// miLength sentinel word (not a separate member), +0x11D60 is mICECamera (its
// embedded Camera sits at +0x11D70), and +0x11ED0 is mCameraSpaceHandler.
//
// FLAG: BrnDirector::Camera::Utils::DebugController (UpdateAction's argument) and the
//   two dev-input converter tables are dev-tools types with no reconstructed home.
//   They are MINIMAL-SLICED below / in the .cpp, accessed by name; replace with their
//   real homes when the dev-tools camera-utils TU is reconstructed.
// ============================================================================

// FLAG: forward declarations for pointer-only references in this header.
//   * CgsSystem::TimerStatusInterface -- the Update frame-timer argument (referenced by
//     pointer only here; the .cpp includes its home for the timestep accessors).
//   * ICE::ICEGroup -- GetShakeGroup's return type; no reconstructed home yet (see the
//     ICEData.hpp ICEGroup note), referenced by pointer only.
namespace CgsSystem { class TimerStatusInterface; }
namespace ICE       { struct ICEGroup; }

namespace BrnDirector
{

// The take-data provider PlayMovie resolves through (mpResourceManager). Held by
// pointer here; the .cpp includes BrnDirectorResourceManager.h for GetKeyAnim.
class DirectorResourceManager;

// ----------------------------------------------------------------------------
// ICEPlayingMovie -- the snapshot GetCurrentMovie returns: the playing take's id, its
// normalised playback position, and whether a take is actually playing.
// ----------------------------------------------------------------------------
struct ICEPlayingMovie
{
    CgsResource::ID mID;                          // the playing take's id
    f32             mfPlaybackPositionParameter;  // normalised playback position
    bool            mbIsValid;                    // a take is playing
};

class ICEWrapper : public CgsMemory::HeapMalloc
{
public:
    // Construct the wrapper object: builds the two heaps + the manager + the mover and
    // seeds the action queue to its unconstructed sentinel (the queue is made usable by
    // Construct()/UpdateAction's Clear).
    ICEWrapper();

    // Construct the runtime: build the vehicle ref + ICE camera, clear the action queue,
    // clear the playback / accept-input / movie-id state, and zero the sim-time scale.
    void Construct();

    // Tear down the runtime: destruct the ICE manager, then destruct the base heap.
    void Destruct();

    // Turn the in-game ICE editor ON for editor mode liMode. Disables the debug
    // console for the editing session, clears any active movie playback, then drives
    // the manager's editor into the requested mode.
    void EditorOn(s32 liMode);

    // Turn the in-game ICE editor OFF. Re-enables the debug console; if the editor is
    // currently active it transitions the editor back to its idle/play state.
    void EditorOff();

    // Rebuild the camera mover for this frame from the manager's currently-active
    // take and the director's ICE resource manager.
    void ReconstructCameraMover(const ICE::IResourceManager* lpResourceManager);

    // Start playing a recorded ICE take. Signature order: the take id, then the
    // normalised start position, then the vehicle-ref type the take is anchored to,
    // then the active race car it targets.
    void PlayMovie(CgsResource::ID lTakeId, f32 lfStartPosition,
                   VehicleRef::EType leVehicleRefType, EActiveRaceCarIndex leRaceCar);

    // Per-frame tick: cache the reference spaces, compute the sim-time scale from the
    // frame timer, advance the manager, render the editor, and drive the camera mover.
    void Update(const CgsSystem::TimerStatusInterface* lpTimer,
                const ICE::CameraSpaceHandler& lrSpace);

    // Push the dev-tools input actions for this frame onto the action queue (only when
    // input is accepted). Press / held / release controls map through the converter
    // tables to queued ICE ActionRefs.
    void UpdateAction(const Camera::Utils::DebugController& lrController);

    bool IsEditorActive() const { return mICEManager.GetEditor().AreMenusActive(); }
    void SetAcceptInput(bool lbAcceptInput) { mbAcceptInput = lbAcceptInput; }
    bool WasEditorActive() const { return mbWasEditorActive; }
    void SetWasEditorActive(bool lbWasEditorActive)
    {
        mbWasEditorActive = lbWasEditorActive;
    }

    // Snapshot the currently-playing movie (id + playback position + validity).
    ICEPlayingMovie GetCurrentMovie();

    // True while a take started by PlayMovie is still playing.
    bool IsPlayingMovie();

    // The wrapper's live take camera (the player copies it / blends from it).
    const Camera::Camera& GetCamera();

    // FLAG: minimal-slice decls reached through the ICEWrapper* by the sibling header
    // BrnDirectorResourceManager.h (its inline GetKeyAnim / GetShakeTakes). They are
    // DECLARATION-ONLY here -- their bodies land with the ICE take-data accessor
    // follow-on; the per-TU `cl /c` gate does not link. ICEGroup has no reconstructed
    // home yet, so GetShakeGroup's return type is forward-declared (pointer-only).
    ICE::ICETakeData* GetICETakeData(CgsResource::ID lTakeId);
    ICE::ICEGroup*    GetShakeGroup();

private:
    // +0x00520  Second heap embedded after the base HeapMalloc (the base heap itself is
    // the CgsMemory::HeapMalloc this class derives from -- Destruct's HeapMalloc::Destruct(this)
    // and the ctor's allocator-at-+0x08 build target it).
    CgsMemory::HeapMalloc mScratchHeap;

    // +0x00A40  The top-level ICE manager (live takes + embedded editor/controller).
    ICE::ICEManager mICEManager;

    // +0x09B20  Per-frame sim-time scale (base time-step * multiplier). Update writes it
    // from the frame timer; Construct zeroes it.
    f32 mfTimeScale;

    // +0x11B24  The take-data provider PlayMovie resolves take ids through.
    DirectorResourceManager* mpResourceManager;

    // +0x11B28  The dev-tools input action stack (its miLength word at +0x11BC8 is the
    // Stack's constructed/sentinel marker the ctor seeds and Construct/UpdateAction Clear).
    ICE::ActionQueue mActionQueue;

    // +0x11BD0  Drives the active camera from the manager's current take.
    ICE::ICECameraMover mCameraMover;

    // +0x11D60  The ICE camera (its embedded BrnDirector::Camera::Camera lands at
    // +0x11D70 -- the address Construct() hands to Camera::Camera::Construct).
    ICE::ICECamera mICECamera;

    // +0x11ED0  The per-frame reference-space cache Update copies the incoming spaces into.
    ICE::CameraSpaceHandler mCameraSpaceHandler;

    // FLAG (provisional names): two ICE load-state scalars the DWARF lists just before
    // mVehicleRef. Construct zeroes exactly these two (+0x120E4 and +0x120E8); their
    // roles beyond "ICE load-state" are not recovered, so the names are provisional.
    // They are modelled with an explicit pad so the two scalars land at +0x120E4/+0x120E8
    // and mVehicleRef stays at +0x120F0 (the +0x120F0..+0x120FC ref fields Construct seeds).
    // +0x120E4  s32  miICELoadStateA  (zeroed by Construct)
    // +0x120E8  s32  miICELoadStateB  (zeroed by Construct)
    s32 miICELoadStateA;
    s32 miICELoadStateB;
    u8  maVehicleRefPad[4];   // pad so mVehicleRef lands at +0x120F0 (0x120E8 + 4 + 4)

    // +0x120F0  The vehicle the active take is anchored to.
    VehicleRef mVehicleRef;

    // +0x12100  The id of the take currently playing (8 bytes; stored by PlayMovie, read
    // by GetCurrentMovie).
    CgsResource::ID mCurrentMovieID;

    // +0x12108  Dev-tools input gate -- UpdateAction only queues actions while this is set.
    bool mbAcceptInput;
    u8 maAcceptInputPadding[7];

    // +0x12110  Previous-frame editor state used to detect editor activation.
    bool mbWasEditorActive;
};

} // namespace BrnDirector

#endif // BRN_DIRECTOR_ICE_WRAPPER_H
