// ============================================================================
// SDKs/Packages/ICE/ICEWrapper.cpp
//
// Runtime bodies for BrnDirector::ICEWrapper (the director-side ICE owner). The home
// (member layout) is GameSource/Director/BrnDirectorICEWrapper.h; members are accessed
// BY NAME here -- the struct-relative offsets quoted in comments are provenance only,
// never used as casts. The two functions left in this TU:
//   Update           per-frame: cache spaces, scale sim time, advance + render + drive mover
//   UpdateAction     queue this frame's dev-tools input actions
//
// The ctor / EditorOn / EditorOff / ReconstructCameraMover bodies live in the sibling
// TU GameSource/Director/BrnDirectorICEWrapper.cpp (same home); Construct, Destruct,
// PlayMovie, GetCurrentMovie and IsPlayingMovie are split into ICEWrapper_wG_11.cpp so
// they can be on the link while this TU cannot -- the two converter tables UpdateAction
// indexes are still unhomed (see the FLAG below).
// ============================================================================

#include "GameSource/Director/BrnDirectorICEWrapper.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h" // CgsSystem::TimerStatusInterface (sim-time step)

// includes folded in from the ICEWrapper_w*.cpp partfiles (2026-09-15)
#include "GameShared/GameClasses/Containers/CgsStack.h"   // CgsContainers::KI_STACK_UNCONSTRUCTED
#include "GameSource/Director/BrnDirectorResourceManager.h"        // DirectorResourceManager::GetKeyAnim

namespace BrnDirector
{

// ----------------------------------------------------------------------------
// FLAG (unhomed dev-tools dependency): the two control->action converter tables
// UpdateAction maps inputs through. The recorded code reads them as two file-scope
// arrays (NOT this-relative members) indexed by control 0..21: the press table for
// just-pressed controls, the pad/held table for is-pressed controls. Their element is
// ICE::EControlToICEAction (a single action id). Their CONTENTS are not recovered, so
// they are declared extern (the definitions are the not-yet-reconstructed static
// tables; the per-TU `cl /c` gate does not link). Replace with the real homed tables
// when the dev-tools ICE-input mapping TU lands.
// ----------------------------------------------------------------------------
extern const ICE::EControlToICEAction maPressConverter[];
extern const ICE::EControlToICEAction maPadConverter[];

namespace
{
    // Control loop bounds + the release-only control / its action, from UpdateAction's
    // recorded loop (controls 0..21 inclusive; control 21 also handles just-released).
    const s32 KI_NUM_INPUT_CONTROLS    = 22;
    const s32 KI_RELEASE_CONTROL       = 21;
    const s32 KI_RELEASE_ACTION_ID     = 28;
    const f32 KF_RELEASE_ACTION_DATA   = 1.0f;

    // The take channel Update samples for the mover's per-frame integer value.
    const s32 KI_MOVER_VALUE_CHANNEL   = 41;
}

// ----------------------------------------------------------------------------
// BrnDirector::ICEWrapper::Update
//
// Per-frame tick:
//   * copy the incoming reference spaces into the cache,
//   * compute the sim-time scale from the game timer (base step * multiplier),
//   * advance the ICE manager and render the editor,
//   * if a movie is playing OR the editor is active, drive the camera mover: advance
//     its sim time, sample the active take's per-frame value (channel 41) into the
//     mover, then finish the mover's frame.
//
// Member map (provenance): CameraSpaceHandler::operator= (+0x11ED0 = lrSpace);
// mfTimeScale = game-timer-status (+8) * (+4) (+0x9B20); ICEManager::Update (+0xA40);
// ICEController::Render (mController, +0x2750); the gate reads mbPlaybackDataSet
// (manager +0x1CE0) || editor menus-active (+0x2BA8); the mover work is at +0x11BD0,
// its take pointer at +0x110, its per-frame value at +0x188.
// ----------------------------------------------------------------------------
void ICEWrapper::Update(const CgsSystem::TimerStatusInterface* lpTimer,
                        const ICE::CameraSpaceHandler& lrSpace)
{
    // Cache this frame's reference spaces.
    mCameraSpaceHandler = lrSpace;

    // Sim-time scale is the PRODUCT of the game timer status' two step fields read
    // directly off the timer (the +8 multiplier times the +4 base step). FLAG: field
    // names are mfTimeStepMultiplier (+8) and mfBaseTimeStep (+4) of the game TimerStatus,
    // which sits at the head of the TimerStatusInterface.
    const CgsSystem::TimerStatus* lpGameStatus = lpTimer->GetGameTimerStatus();
    mfTimeScale = lpGameStatus->GetTimeStepMultiplier() * lpGameStatus->GetBaseTimeStep();

    // Advance the manager, then let the editor draw its overlay.
    mICEManager.Update();
    mICEManager.GetEditor().Render();

    // Drive the mover while a movie is playing or the editor is up.
    if (mICEManager.IsPlaybackDataSet() || mICEManager.GetEditor().AreMenusActive())
    {
        const f32 lfSimTime = 1.0f;

        ICE::ICETake* lpTake = mCameraMover.GetTake();
        if (lpTake != 0)
        {
            mCameraMover.UpdateSimTime(lfSimTime);

            lpTake = mCameraMover.GetTake();
            if (lpTake->GetParameter() <= 0.0f)
                mCameraMover.SetCurrentTakeValueInt(0);

            mCameraMover.SetCurrentTakeValueInt(lpTake->GetValueInt(KI_MOVER_VALUE_CHANNEL));
        }

        mCameraMover.UpdateFrameEnd(lfSimTime);
    }
}

// ----------------------------------------------------------------------------
// BrnDirector::ICEWrapper::UpdateAction
//
// Translate this frame's dev-tools controller input into queued ICE actions (only when
// input is accepted). The action queue is Cleared, then for each control 0..21:
//   * just-pressed  -> push { press-converter[control], control value }
//   * is-pressed    -> push { pad-converter[control],   control value }
// and, for the release control (21) only, just-released -> push { 28, 1.0 }.
// The press / held pushes are bounded by the queue's capacity (skipped when full);
// the release push checks IsFull explicitly. Each push asserts the queue was
// Construct/Clear'd (the Stack's used-before-Construct invariant).
//
// Member map (provenance): the accept-input gate is mbAcceptInput (+0x12108); the
// queue is mActionQueue (+0x11B28, length word +0x11BC8); the converter tables are the
// two file-scope arrays; the control value is controller+4*control.
// ----------------------------------------------------------------------------
void ICEWrapper::UpdateAction(const Camera::Utils::DebugController& lrController)
{
    // The control index is an int in the recorded loop; the query accessors take the
    // controller's own enum, so each index is named through it explicitly.
    typedef Camera::Utils::DebugController::EControl EControl;

    if (!mbAcceptInput)
        return;

    mActionQueue.Clear();

    for (s32 liControl = 0; liControl < KI_NUM_INPUT_CONTROLS; ++liControl)
    {
        const EControl leControl = static_cast<EControl>(liControl);

        if (lrController.GetJustPressed(leControl))
        {
            ICE::ActionRef lAction;
            lAction.SetID(maPressConverter[liControl].mAction);
            lAction.SetData(lrController.GetControlValue(liControl));
            // Bounded by the queue's capacity (the recorded code skips the push when full).
            if (!mActionQueue.IsFull())
                mActionQueue.Push(lAction);
        }

        if (lrController.GetIsPressed(leControl))
        {
            ICE::ActionRef lAction;
            lAction.SetID(maPadConverter[liControl].mAction);
            lAction.SetData(lrController.GetControlValue(liControl));
            if (!mActionQueue.IsFull())
                mActionQueue.Push(lAction);
        }

        if (liControl == KI_RELEASE_CONTROL && lrController.GetJustReleased(static_cast<EControl>(KI_RELEASE_CONTROL)))
        {
            ICE::ActionRef lAction;
            lAction.SetID(KI_RELEASE_ACTION_ID);
            lAction.SetData(KF_RELEASE_ACTION_DATA);
            if (!mActionQueue.IsFull())
                mActionQueue.Push(lAction);
        }
    }
}

} // namespace BrnDirector

// ============================================================================
// FOLDED FROM ICEWrapper_wG_09.cpp (wave G) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// The BrnDirector::ICEWrapper default constructor, split out of
// GameSource/Director/BrnDirectorICEWrapper.cpp: that TU's other bodies need
// ICEController::EditorOn / ::SetState, ICECameraMover::Construct and
// DebugInterface::Enable/DisableConsole, none of which has a body in the tree.
// DELETE-WHEN: BrnDirectorICEWrapper.cpp can mount -- then move this body back into it.
// This constructor runs before the debug log exists (MainDirector embeds the wrapper
// by value), so it must not log.
// ============================================================================


namespace BrnDirector
{

ICEWrapper::ICEWrapper()
{
    // The action stack starts unconstructed: any use before Construct/Clear asserts.
    mActionQueue.miLength = CgsContainers::KI_STACK_UNCONSTRUCTED;
}

} // namespace BrnDirector

// ============================================================================
// FOLDED FROM ICEWrapper_wG_11.cpp (wave G) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// BrnDirector::ICEWrapper::Construct / ::Destruct / ::PlayMovie / ::GetCurrentMovie /
// ::IsPlayingMovie, split out of SDKs/Packages/ICE/ICEWrapper.cpp: what keeps that TU
// off the link is its remaining pair, Update and UpdateAction, which index two
// dev-tools control->action converter tables whose contents are not recovered and which
// have no definition anywhere in the tree.
// DELETE-WHEN: those two tables are homed and ICEWrapper.cpp can mount -- then move
// these bodies back into it.
// MainDirector embeds the wrapper by value and Constructs it at boot, before the
// debug log exists: nothing here may log.
// ============================================================================


namespace BrnDirector
{

// ----------------------------------------------------------------------------
// Construct
//
// Build the runtime state: construct the vehicle ref and seed its bound-state fields
// (the inlined VehicleRef::Set(E_PLAYER_CAR, ..)), clear the dev-tools action queue,
// zero the two ICE load-state scalars, construct the ICE camera, drop the sim-time
// scale. The heaps / manager / mover are built by the constructor.
//
// Member map (provenance): VehicleRef::Construct(&mVehicleRef @+0x120F0) then
// +0x120F0=0, +0x120FC=1, +0x120F8=0, +0x120F4=-1; mActionQueue's miLength at +0x11BC8
// is zeroed; +0x120E8 then +0x120E4 are the two load-state scalars; Camera::Construct
// on +0x11D70 (mICECamera's embedded director camera); mfTimeScale at +0x9B20. No
// write to the manager playback flag, the current-movie id or the accept-input gate.
// ----------------------------------------------------------------------------
void ICEWrapper::Construct()
{
    mVehicleRef.Construct();

    // VehicleRef::Set(E_PLAYER_CAR, ..) inlined -- the ref is bound to the player car.
    mVehicleRef.meType         = VehicleRef::E_PLAYER_CAR;
    mVehicleRef.mbSet          = true;
    mVehicleRef.muRef          = 0;
    mVehicleRef.miRaceCarIndex = -1;

    // Make the dev-tools action queue usable (off the unconstructed sentinel the
    // constructor seeds).
    mActionQueue.Clear();

    // Reset the two ICE load-state scalars; miICELoadStateB is the stage word
    // ICEWrapper::Prepare switches on, so a fresh wrapper enters Prepare at stage 0.
    miICELoadStateB = 0;
    miICELoadStateA = 0;

    // The ICE camera's bring-up is inlined at this call site to exactly its embedded
    // director camera's Construct -- the only store the recorded call makes.
    mICECamera.GetCamera()->Construct();

    // No sim-time scale until the first Update.
    mfTimeScale = 0.0f;
}

// ----------------------------------------------------------------------------
// Destruct
//
// Tear down the runtime: destruct the ICE manager (+0xA40), then the base heap --
// the CgsMemory::HeapMalloc this wrapper IS.
// ----------------------------------------------------------------------------
void ICEWrapper::Destruct()
{
    mICEManager.Destruct();
    HeapMalloc::Destruct();
}

// ----------------------------------------------------------------------------
// PlayMovie
//
// Start playing a recorded camera take ("movie"):
//   * resolve the take data through the resource manager (assert it exists),
//   * bind + start the manager's playback take at the requested start position (which
//     sets the manager's playback flag),
//   * point the camera mover at the now-active take,
//   * advance the manager once so the take is live this frame,
//   * aim the vehicle ref at the requested race car / ref type,
//   * remember the playing movie's id.
//
// Member map (provenance): GetKeyAnim via mpResourceManager (+0x11B24); the
// SetDataPointers + SetParameter + flag-set on the manager's mPlaybackTake (+0x15A8)
// is the manager's SetTakeToPlay; the mover's take pointer store is at the mover's
// +0x110 (SetTake); ICEManager::Update (+0xA40); VehicleRef::Set(&mVehicleRef @+0x120F0,
// refType, raceCar, 1); mCurrentMovieID store at +0x12100 (8 bytes).
// ----------------------------------------------------------------------------
void ICEWrapper::PlayMovie(CgsResource::ID lTakeId, f32 lfStartPosition,
                           VehicleRef::EType leVehicleRefType, EActiveRaceCarIndex leRaceCar)
{
    ICE::ICETakeData* lpTakeData = mpResourceManager->GetKeyAnim(lTakeId);
    CGS_ASSERT(lpTakeData != 0, "Invalid ICE Movie Requested");

    // Bind + start the manager's playback take at the requested position (sets the
    // manager's playback-active flag).
    mICEManager.SetTakeToPlay(lpTakeData, lfStartPosition);

    // Drive the mover from whichever take is now active (the playback take).
    mCameraMover.SetTake(mICEManager.GetCameraTake());

    // Advance the manager once so the take is live this frame.
    mICEManager.Update();

    // Aim the vehicle ref at the requested race car for this take's ref type.
    mVehicleRef.Set(leVehicleRefType, leRaceCar, true);

    // Remember the movie now playing.
    mCurrentMovieID = lTakeId;
}

// ----------------------------------------------------------------------------
// GetCurrentMovie
//
// Snapshot the currently-playing movie: when a take is playing, return its id and
// normalised playback position and mark the snapshot valid; otherwise the snapshot is
// invalid. (Reads the manager's playback flag, this wrapper's stored movie id, and the
// manager's current take parameter -- the playback take's mfParameter.)
// ----------------------------------------------------------------------------
ICEPlayingMovie ICEWrapper::GetCurrentMovie()
{
    ICEPlayingMovie lCurrentMovie;

    if (mICEManager.IsPlaybackDataSet())
    {
        lCurrentMovie.mID = mCurrentMovieID;
        lCurrentMovie.mfPlaybackPositionParameter = mICEManager.GetCurrentTakeParameter();
        lCurrentMovie.mbIsValid = true;
    }
    else
    {
        lCurrentMovie.mbIsValid = false;
    }

    return lCurrentMovie;
}

// ----------------------------------------------------------------------------
// IsPlayingMovie
//
// True while a take started by PlayMovie is still playing (the manager's playback
// flag, manager +0x1CE0).
// ----------------------------------------------------------------------------
bool ICEWrapper::IsPlayingMovie()
{
    return mICEManager.IsPlaybackDataSet();
}

} // namespace BrnDirector
