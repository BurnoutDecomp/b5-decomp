// ============================================================================
// BrnDirector::ICEWrapper::Construct / ::Destruct / ::PlayMovie / ::GetCurrentMovie /
// ::IsPlayingMovie / ::Update, split out of SDKs/Packages/ICE/ICEWrapper.cpp: what keeps that
// TU off the link is UpdateAction, which indexes two dev-tools control->action converter
// tables whose contents are not recovered and which have no definition anywhere in the tree.
// (Update moved here 2026-09-27, OWNERLIST lane L5: it indexes neither table, and the pause
// camera needs it -- MainDirector::UpdateICE @0x82238FC0 calls it every frame.)
// DELETE-WHEN: those two tables are homed and ICEWrapper.cpp can mount -- then move
// these bodies back into it.
// MainDirector embeds the wrapper by value and Constructs it at boot, before the
// debug log exists: nothing here may log.
// ============================================================================

#include "GameSource/Director/BrnDirectorICEWrapper.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameSource/Director/BrnDirectorResourceManager.h"        // DirectorResourceManager::GetKeyAnim
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h" // CgsSystem::TimerStatusInterface (ICETimer::Update)

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
// on +0x11D70 (mICECamera's embedded director camera); mICETimer (+0x9B20) = 0.0 -- the
// inlined ICETimer::Construct, `stfsx f0(flt_82001CC0), r31, 0x9B20` @0x82533B3C. No
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

    // No ICE timestep until the first Update.
    mICETimer.Construct();
}

// ----------------------------------------------------------------------------
// Destruct @0x82533B58
//
// Tear down the runtime: destruct the ICE manager (+0xA40, @0x82533B70), then the ICE heap --
// `bl CgsMemory::HeapMalloc::Destruct` on the wrapper's own `this` @0x82533B78, i.e. the
// HeapMalloc base of mICEMemory at offset 0 (ICEMemory declares no Destruct of its own).
// ----------------------------------------------------------------------------
void ICEWrapper::Destruct()
{
    mICEManager.Destruct();
    mICEMemory.Destruct();
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

// ----------------------------------------------------------------------------
// Update @0x82540180
//
// The per-frame tick, called once a frame by MainDirector::UpdateICE @0x82238FC0:
//   0x825401A8  mICECameraAnchor.SetAnchor(lrSpace)       (CameraSpaceHandler::operator= on +0x11ED0)
//   0x825401B0  mICETimer.Update(lpTimer)                  (inlined: [+8] * [+4] -> +0x9B20)
//   0x825401C8  mICEManager.Update()
//   0x825401D0  the editor's Render (ICEController::Render on +0x2750; returns at once unless
//               the editor is up, miState > 0)
//   0x825401D4  if (the manager is playing a movie back (+0x2720 == manager +0x1CE0) ||
//                   the editor is up (+0x35E8 == controller +0xE98, > 0))
//   0x82540200..0x82540254  mCameraMover.Update(1.0f)      (inlined: UpdateFrameBegin, then
//               `bl UpdateFrameEnd`; the 1.0 is flt_82001C98)
// ----------------------------------------------------------------------------
void ICEWrapper::Update(const CgsSystem::TimerStatusInterface* lpTimer,
                        const ICE::CameraSpaceHandler& lrSpace)
{
    mICECameraAnchor.SetAnchor(lrSpace);
    mICETimer.Update(lpTimer);

    mICEManager.Update();
    mICEManager.GetEditor().Render();

    if (mICEManager.IsPlaybackDataSet() || mICEManager.GetEditor().AreMenusActive())
    {
        mCameraMover.Update(1.0f);
    }
}

} // namespace BrnDirector

namespace ICE
{

// ----------------------------------------------------------------------------
// ICETimer::Update (DWARF ICETimer.hpp:41). No ARTIST symbol: its one expansion is
// ICEWrapper::Update above, 0x825401B0..0x825401C4 --
//     lfs f0, 8(status) ; lfs f13, 4(status) ; fmuls f0, f0, f13 ; stfsx f0, this, 0x9B20
// -- the game timer status's multiplier (+8) times its base step (+4), one rounding.
// ----------------------------------------------------------------------------
void ICETimer::Update(const CgsSystem::TimerStatusInterface* lpStatus)
{
    const CgsSystem::TimerStatus* lpGameStatus = lpStatus->GetGameTimerStatus();
    mfTimestep = lpGameStatus->GetTimeStepMultiplier() * lpGameStatus->GetBaseTimeStep();
}

} // namespace ICE

namespace BrnDirector
{

} // namespace BrnDirector
