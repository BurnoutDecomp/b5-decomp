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

#include "GameSource/Director/BrnDirectorICEWrapper.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameSource/Director/BrnDirectorResourceManager.h"        // DirectorResourceManager::GetKeyAnim

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
