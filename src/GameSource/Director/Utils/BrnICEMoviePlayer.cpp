#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"    // CgsCore::SPrintf
#include "GameSource/Director/BrnDirectorICEWrapper.h"     // BrnDirector::ICEWrapper (PlayMovie / IsPlayingMovie / GetCamera)
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"  // Camera::TextFileWriteSerialiser (SharedPlaylists::Serialise<S>)
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"   // Camera::TextFileReadSerialiser  (SharedPlaylists::Serialise<S>)

// ============================================================================
// GameSource/Director/Utils/BrnICEMoviePlayer.cpp
//
// Bodies for the ICE movie-player family. Reconstructed for semantic parity; members
// are accessed BY NAME (the struct layouts live in BrnICEMoviePlayer.h). The
// camera-behaviour layer the player drives is the minimal slice declared in the home
// (FLAGGED there); the calls below name the methods those bodies will land with when the
// real Camera TUs are reconstructed.
// ============================================================================

namespace BrnDirector
{

// ----------------------------------------------------------------------------
// File-local helpers / not-yet-reconstructed callees (DECLARATION-ONLY).
// FLAG: bodies for these are NOT in this TU; the per-TU `cl /c` gate only needs the
// declarations. The real / trap bodies link later.
// ----------------------------------------------------------------------------

// Build a take resource id from an (ICE-group, take-index) pair by formatting the
// group's generated name and hashing it. File-scope free function -- the home does not
// need to expose it.
static const CgsResource::ID MakeCGSId(IceMovie::EIceGroup leGroup, u32 luTakeIndex);

// FLAG: the dev-menu registration callee that (de)registers a playlist's per-movie
// remove-data entry with the debug component. Not-yet-reconstructed dev-tools TU; this is
// a DECLARATION-ONLY external callee (its body links later -- the per-TU `cl /c` gate
// only needs the declaration). The descriptor is built by the caller and passed by
// address; the trailing playlist + name identify the menu owner.
void IceMovieDebugMenuRegisterRemoveEntry(const void* lpDescriptor,
                                          const char* lpcDebugName,
                                          ICEMoviePlaylist* lpPlaylist);

// ----------------------------------------------------------------------------
// BrnDirector::MakeCGSId
//
// Formats the generated take name for an ICE group + 1-based take index, then hashes it
// into a resource id. World-signature takes assert a positive take index; an unknown
// group asserts. (The 1-based index is the `luTakeIndex + 1` display form.)
// ----------------------------------------------------------------------------
static const CgsResource::ID MakeCGSId(IceMovie::EIceGroup leGroup, u32 luTakeIndex)
{
    char lacName[64];
    const u32 luDisplayIndex = luTakeIndex + 1;

    switch (leGroup)
    {
    case IceMovie::E_ICE_GROUP_EVENTS_START:
        CgsCore::SPrintf(lacName, sizeof(lacName), "Events_Start_%i", luDisplayIndex);
        break;
    case IceMovie::E_ICE_GROUP_EVENTS_END:
        CgsCore::SPrintf(lacName, sizeof(lacName), "Events_End_%i", luDisplayIndex);
        break;
    case IceMovie::E_ICE_GROUP_WORLD_LANDMARK:
        CgsCore::SPrintf(lacName, sizeof(lacName), "World_LandMark_%i", luDisplayIndex);
        break;
    case IceMovie::E_ICE_GROUP_WORLD_SIGNATURE:
        CGS_ASSERT(static_cast<s32>(luTakeIndex) != -1, "luTakeIndex > 0");
        // World-signature is the ONE group that formats the RAW 0-based take index, not the
        // +1 display index: the X360 computes (luTakeIndex + 1) - 1 == luTakeIndex here
        // (XEX MakeCGSId @0x821F79F8 `v8 - 1`; PS3 DecFIGS @0x4D390 `World_Signature_%i,
        // luTakeIndex`). All other groups use luDisplayIndex.
        CgsCore::SPrintf(lacName, sizeof(lacName), "World_Signature_%i", luTakeIndex);
        break;
    case IceMovie::E_ICE_GROUP_VEHICLE_CAR:
        CgsCore::SPrintf(lacName, sizeof(lacName), "Vehicle_Car_%i", luDisplayIndex);
        break;
    case IceMovie::E_ICE_GROUP_VEHICLE_RIV:
        CgsCore::SPrintf(lacName, sizeof(lacName), "Vehicle_Rival_%i", luDisplayIndex);
        break;
    case IceMovie::E_ICE_GROUP_GENERIC_ALL:
        CgsCore::SPrintf(lacName, sizeof(lacName), "Generic_All_%i", luDisplayIndex);
        break;
    default:
        CGS_ASSERT(false, "Invalid ICE group: ");
        lacName[0] = '\0';
        break;
    }

    CgsResource::ID lResultId;
    // The name hasher returns a 32-bit value; widen it (zero-extended) into the id's
    // 64-bit hash field.
    const u32 luNameHash = static_cast<u32>(CgsResource::ID::HashString(reinterpret_cast<const u8*>(lacName)));
    lResultId.SetHash(static_cast<u64>(luNameHash));
    return lResultId;
}

// ----------------------------------------------------------------------------
// BrnDirector::IceMovie::GetCgsID
//
// Resolve this movie's take id: the stored id for E_REF_TYPE_CGSID, a generated
// (group,take) id for E_REF_TYPE_GROUP_TAKE, otherwise assert an invalid ref type.
// (The branch on the stored ref type: ref type 0 returns the stored id, ref type 1
// generates one, any other value asserts.)
// ----------------------------------------------------------------------------
const CgsResource::ID IceMovie::GetCgsID() const
{
    if (meRefType == E_REF_TYPE_CGSID)
    {
        return mCgsID;
    }

    if (meRefType == E_REF_TYPE_GROUP_TAKE)
    {
        return MakeCGSId(meGroup, muTakeIndex);
    }

    CGS_ASSERT(false, "Invalid ref type ");
    return mCgsID;
}

// ----------------------------------------------------------------------------
// BrnDirector::ICEMoviePlaylist::GetMovieCount
//
// The number of movies in the list == the order array's live-element count. Asserts the
// order array was Construct/Clear'd (its count is off the -1 sentinel).
// ----------------------------------------------------------------------------
s32 ICEMoviePlaylist::GetMovieCount() const
{
    CGS_ASSERT(mMoviePoolIndicies.GetCount() != -1,
               "Array used before Construct/Clear was called");
    return mMoviePoolIndicies.GetCount();
}

// ----------------------------------------------------------------------------
// BrnDirector::ICEMoviePlaylist::DebugMenuNewMovie
//
// Dev-menu command: drop the current remove-data menu entry, insert a fresh default
// movie (Generic_All, take 31, vehicle ref type 1, flash on) before the new-movie slot,
// then re-register the menu entry. lpContext is the dev-menu context (unused by the body
// beyond the descriptor the registration callee builds from this playlist).
// ----------------------------------------------------------------------------
void ICEMoviePlaylist::DebugMenuNewMovie(void* /*lpContext*/)
{
    // The dev-menu item descriptor the registration callee consumes. FLAG: only the
    // leading enable flag and the owning debug component are recovered; the middle fields
    // are a not-yet-recovered dev-menu-item span, modelled as a zeroed block so the
    // descriptor is the right shape to pass by address.
    struct DebugMenuItemDescriptor
    {
        bool            mbEnabled;
        u8              maUnrecovered[27];
        DebugComponent* mpDebugComponent;
    };

    // Drop the current remove-data menu entry before changing the movie list.
    {
        DebugMenuItemDescriptor lDescriptor = { true, { 0 }, mpDebugComponent };
        IceMovieDebugMenuRegisterRemoveEntry(&lDescriptor, mpDebugName, this);
    }

    IceMovie lNewMovie;
    lNewMovie.SetMovie(IceMovie::E_ICE_GROUP_GENERIC_ALL, 31u);
    lNewMovie.SetStartPosition(0.0f);
    lNewMovie.SetVehicle(VehicleRef::E_RACE_CAR, 0u);
    lNewMovie.SetShouldFlash(true);
    InsertMovieBefore(miDebugMenuNewMovieIndex - 1, lNewMovie);

    // Re-register the remove-data menu entry for the new movie layout.
    {
        DebugMenuItemDescriptor lDescriptor = { false, { 0 }, mpDebugComponent };
        IceMovieDebugMenuRegisterRemoveEntry(&lDescriptor, mpDebugName, this);
    }
}

// ----------------------------------------------------------------------------
// BrnDirector::ICEMoviePlayer::Construct
//
// Build the player: construct the embedded playlist and camera, default-construct the
// two interpolator handles and helper indices, and clear all playback state.
// ----------------------------------------------------------------------------
void ICEMoviePlayer::Construct()
{
    mPlaylist.Construct();
    mCamera.Construct();

    mInInterpolator  = Camera::BehaviourHandle<Camera::BehaviourInterpolate>();
    mOutInterpolator = Camera::BehaviourHandle<Camera::BehaviourInterpolate>();
    mFromBehaviourHelperIndex = Camera::BehaviourHelperIndex();
    mToBehaviourHelperIndex   = Camera::BehaviourHelperIndex();

    mpICEWrapper             = 0;
    mpInterpolateInParams    = 0;
    mpInterpolateOutParams   = 0;
    mfInterpolateInDuration  = 0.0f;
    mfInterpolateOutDuration = 0.0f;
    mfMaxInterpolateOutOverlapTime = 0.0f;
    miCurrentMovie = 0;

    mbInterpolateIn        = false;
    mbInterpolateOutNow    = false;
    mbHasReachedEnd        = false;
    mbIsLooping            = false;
    mbIsPlaying            = false;
    mbShouldInterpolateOut = false;
    mbInterpolateOutUpdatesDuringPause = false;
    mbFirstFrameOfPlaying  = false;
}

// ----------------------------------------------------------------------------
// BrnDirector::ICEMoviePlayer::Prepare
//
// Bind the ICE wrapper this player drives. Asserts a non-NULL wrapper; always returns
// true (the prepare cannot otherwise fail).
// ----------------------------------------------------------------------------
bool ICEMoviePlayer::Prepare(ICEWrapper* lpICEWrapper)
{
    CGS_ASSERT(lpICEWrapper != 0, "lpICEWrapper != NULL");
    mpICEWrapper = lpICEWrapper;
    return true;
}

// ----------------------------------------------------------------------------
// BrnDirector::ICEMoviePlayer::StartCurrentMovie  (private helper)
//
// Kick off the movie at miCurrentMovie: resolve it from the playlist pool, then drive
// the ICE wrapper to play its take at the stored start position. Clears the first-frame
// flag (mbFirstFrameOfPlaying is cleared on every "start this movie" step).
// ----------------------------------------------------------------------------
void ICEMoviePlayer::StartCurrentMovie()
{
    const s32       liPoolIndex = mPlaylist.mMoviePoolIndicies.GetItem(miCurrentMovie);
    const IceMovie& lrMovie     = mPlaylist.mMoviePool[liPoolIndex];

    mpICEWrapper->PlayMovie(lrMovie.GetCgsID(), lrMovie.GetStartPosition(),
                            meTargetVehicleRefType, meTargetRaceCar);
    mbFirstFrameOfPlaying = false;
}

// ----------------------------------------------------------------------------
// BrnDirector::ICEMoviePlayer::Play
//
// Start playing the playlist from the first movie: set the playing/first-frame flags,
// clear end/interpolate-out state, clear the camera, then kick the first movie's take.
// ----------------------------------------------------------------------------
void ICEMoviePlayer::Play()
{
    CGS_ASSERT(mpICEWrapper != 0, "mpICEWrapper != NULL");
    CGS_ASSERT(!mbIsPlaying, "!mbIsPlaying");

    mbIsLooping           = false;
    mbIsPlaying           = true;
    mbHasReachedEnd       = false;
    mbInterpolateOutNow   = false;
    mbFirstFrameOfPlaying = true;
    miCurrentMovie        = 0;

    mCamera.Clear();

    const s32       liPoolIndex = mPlaylist.mMoviePoolIndicies.GetItem(miCurrentMovie);
    const IceMovie& lrMovie     = mPlaylist.mMoviePool[liPoolIndex];

    mpICEWrapper->PlayMovie(lrMovie.GetCgsID(), lrMovie.GetStartPosition(),
                            meTargetVehicleRefType, meTargetRaceCar);
}

// ----------------------------------------------------------------------------
// BrnDirector::ICEMoviePlayer::Loop
//
// Begin looping playback: start playing, then set the looping flag.
// ----------------------------------------------------------------------------
void ICEMoviePlayer::Loop()
{
    CGS_ASSERT(mpICEWrapper != 0, "mpICEWrapper != NULL");
    CGS_ASSERT(!mbIsPlaying, "!mbIsPlaying");

    Play();
    mbIsLooping = true;
}

// ----------------------------------------------------------------------------
// BrnDirector::ICEMoviePlayer::CutToInterpolateOut
//
// Request an immediate cut into the blend-out: only valid while playing, looping, and
// flagged to interpolate out. Sets the "interpolate out now" flag.
// ----------------------------------------------------------------------------
void ICEMoviePlayer::CutToInterpolateOut()
{
    CGS_ASSERT(mpICEWrapper != 0, "mpICEWrapper != NULL");
    CGS_ASSERT(mbIsPlaying, "mbIsPlaying");
    CGS_ASSERT(mbIsLooping, "mbIsLooping");
    CGS_ASSERT(mbShouldInterpolateOut, "mbShouldInterpolateOut");

    mbInterpolateOutNow = true;
}

// ----------------------------------------------------------------------------
// BrnDirector::ICEMoviePlayer::Stop
//
// Stop playback and tear down any live interpolators: clear the playing/end/interpolate
// flags, then release each allocated interpolator handle back to its manager.
// ----------------------------------------------------------------------------
void ICEMoviePlayer::Stop()
{
    CGS_ASSERT(mpICEWrapper != 0, "mpICEWrapper != NULL");
    CGS_ASSERT(mbIsPlaying, "mbIsPlaying");

    mbIsPlaying     = false;
    mbHasReachedEnd = false;
    mbInterpolateIn = false;

    if (mInInterpolator.IsAllocated())
    {
        mInInterpolator.GetManager()->ReleaseBehaviour(mInInterpolator);
        mInInterpolator.Clear();
    }

    if (mOutInterpolator.IsAllocated())
    {
        mOutInterpolator.GetManager()->ReleaseBehaviour(mOutInterpolator);
        mOutInterpolator.Clear();
    }
}

// ----------------------------------------------------------------------------
// BrnDirector::ICEMoviePlayer::Update
//
// Per-frame playback tick (only does work while playing):
//   1. Drive the camera from whichever source is active this frame: the out-interpolator
//      (if allocated; flag end-reached when it finishes), else the in-interpolator (if
//      allocated; release it and clear the interpolate-in flag when it finishes), else
//      the ICE wrapper's live take camera.
//   2. If a blend-out was requested and none exists yet, allocate the out-interpolator
//      and configure its mode / target-helper / duration / camera-A / camera-B, then set
//      it up.
//   3. On the first play frame, fire the current movie's flash hook if requested.
//   4. When the wrapper has finished the current movie and nothing is interpolating,
//      advance to the next movie (or loop / end / request blend-out as configured),
//      firing the new movie's flash hook before starting it.
// ----------------------------------------------------------------------------
void ICEMoviePlayer::Update(Camera::BehaviourManager& lrBehaviourManager)
{
    if (!mbIsPlaying)
    {
        return;
    }

    if (mOutInterpolator.IsAllocated())
    {
        Camera::BehaviourInterpolate* lpOut = mOutInterpolator.GetBehaviour();
        mCamera = lpOut->GetCamera();
        if (lpOut->HasFinished())
        {
            mbHasReachedEnd = true;
        }
    }
    else if (mInInterpolator.IsAllocated())
    {
        Camera::BehaviourInterpolate* lpIn = mInInterpolator.GetBehaviour();
        mCamera = lpIn->GetCamera();
        if (lpIn->HasFinished())
        {
            mbInterpolateIn = false;
            mInInterpolator.GetManager()->ReleaseBehaviour(mInInterpolator);
        }
    }
    else
    {
        mCamera = mpICEWrapper->GetCamera();
    }

    if (mbInterpolateOutNow && !mOutInterpolator.IsAllocated())
    {
        lrBehaviourManager.NewBehaviourInterpolate(mOutInterpolator, 0, 0, 1);

        Camera::BehaviourInterpolate* lpOut = mOutInterpolator.GetBehaviour();
        lpOut->SetInterpolationMode(mbInterpolateOutUpdatesDuringPause ? 2 : 0);
        lpOut->SetParameters(mpInterpolateOutParams);
        lpOut->SetupDuration(mfInterpolateOutDuration);
        lpOut->SetupCameraAFromCamera(mpICEWrapper->GetCamera());
        lpOut->SetupCameraBFromHelper(mToBehaviourHelperIndex, lrBehaviourManager);
        lpOut->Setup();
    }

    if (mbFirstFrameOfPlaying)
    {
        const s32       liPoolIndex = mPlaylist.mMoviePoolIndicies.GetItem(miCurrentMovie);
        const IceMovie& lrMovie     = mPlaylist.mMoviePool[liPoolIndex];
        if (lrMovie.GetShouldFlash())
        {
            ApplyFlashHookToCamera();
        }
    }

    if (!mpICEWrapper->IsPlayingMovie()
        && !mInInterpolator.IsAllocated()
        && !mOutInterpolator.IsAllocated()
        && !mbHasReachedEnd)
    {
        ++miCurrentMovie;
        if (miCurrentMovie < mPlaylist.GetMovieCount())
        {
            const s32       liPoolIndex = mPlaylist.mMoviePoolIndicies.GetItem(miCurrentMovie);
            const IceMovie& lrMovie     = mPlaylist.mMoviePool[liPoolIndex];
            if (lrMovie.GetShouldFlash())
            {
                ApplyFlashHookToCamera();
            }
            StartCurrentMovie();
        }
        else if (mbIsLooping)
        {
            miCurrentMovie = 0;
            const s32       liPoolIndex = mPlaylist.mMoviePoolIndicies.GetItem(miCurrentMovie);
            const IceMovie& lrMovie     = mPlaylist.mMoviePool[liPoolIndex];
            if (lrMovie.GetShouldFlash())
            {
                ApplyFlashHookToCamera();
            }
            StartCurrentMovie();
        }
        else if (!mbShouldInterpolateOut)
        {
            mbHasReachedEnd = true;
        }
        else
        {
            mbInterpolateOutNow = true;
        }
    }

    mbFirstFrameOfPlaying = false;
}

// ----------------------------------------------------------------------------
// BrnDirector::ICEMoviePlayer::GetCamera
//
// The player's current camera. Asserts playback is active (the camera is only meaningful
// while a movie is playing).
// ----------------------------------------------------------------------------
const Camera::Camera& ICEMoviePlayer::GetCamera() const
{
    CGS_ASSERT(mbIsPlaying, "mbIsPlaying");
    return mCamera;
}

// ----------------------------------------------------------------------------
// BrnDirector::ICEMoviePlayer::InterpolateFrom
//
// Begin blending the camera IN from the given source over a duration: only valid when
// not already playing or already interpolating in. Records the helper/params/duration,
// allocates an in-interpolator, configures its mode/parameters/duration and its A/B
// camera references (A == the source helper, B == the ICE wrapper's live take), sets it
// up, and flags the interpolate-in state. Each setup step asserts the handle is still
// allocated and that Setup() has not yet been called.
// ----------------------------------------------------------------------------
void ICEMoviePlayer::InterpolateFrom(Camera::BehaviourManager& lrBehaviourManager,
                                     Camera::BehaviourHelperIndex lFromHelper,
                                     f32 lfDuration,
                                     const Camera::BehaviourInterpolate::Parameters* lpParams,
                                     bool lbUpdatesDuringPause)
{
    CGS_ASSERT(!mbIsPlaying, "Asking to interpolate while already playing");
    CGS_ASSERT(!mbInterpolateIn, "Already interpolating in");

    mpInterpolateInParams     = lpParams;
    mfInterpolateInDuration   = lfDuration;
    mFromBehaviourHelperIndex = lFromHelper;

    lrBehaviourManager.NewBehaviourInterpolate(mInInterpolator, 0, 0, 1);
    CGS_ASSERT(mInInterpolator.IsAllocated(), "IsAllocated()");

    lrBehaviourManager.SetBehaviourUpdatesDuringPause(mInInterpolator, lbUpdatesDuringPause);

    CGS_ASSERT(mInInterpolator.IsAllocated(), "IsAllocated()");
    mInInterpolator.GetBehaviour()->SetInterpolationMode(lbUpdatesDuringPause ? 2 : 0);

    CGS_ASSERT(mInInterpolator.IsAllocated(), "IsAllocated()");
    {
        Camera::BehaviourInterpolate* lpInterpolate = mInInterpolator.GetBehaviour();
        lpInterpolate->SetParameters(mpInterpolateInParams);
    }

    CGS_ASSERT(mInInterpolator.IsAllocated(), "IsAllocated()");
    {
        Camera::BehaviourInterpolate* lpInterpolate = mInInterpolator.GetBehaviour();
        CGS_ASSERT(!lpInterpolate->HasFinished(), "Can't setup duration after Setup() has been called");
        lpInterpolate->SetupDuration(mfInterpolateInDuration);
    }

    CGS_ASSERT(mInInterpolator.IsAllocated(), "IsAllocated()");
    {
        Camera::BehaviourInterpolate* lpInterpolate = mInInterpolator.GetBehaviour();
        CGS_ASSERT(!lpInterpolate->HasFinished(), "Can't setup camera A after Setup() has been called");
        lpInterpolate->SetupCameraAFromHelper(mFromBehaviourHelperIndex, lrBehaviourManager);
    }

    CGS_ASSERT(mInInterpolator.IsAllocated(), "IsAllocated()");
    {
        Camera::BehaviourInterpolate* lpInterpolate = mInInterpolator.GetBehaviour();
        CGS_ASSERT(!lpInterpolate->HasFinished(), "Can't setup camera B after Setup() has been called");
        // FLAG: camera B is the ICE wrapper's live take camera (the source passes a
        // pointer to the wrapper here). The wrapper-side camera accessor is its own TU.
        lpInterpolate->SetupCameraBFromCamera(mpICEWrapper->GetCamera());
    }

    CGS_ASSERT(mInInterpolator.IsAllocated(), "IsAllocated()");
    mInInterpolator.GetBehaviour()->Setup();

    mbInterpolateIn = true;
}

// ----------------------------------------------------------------------------
// BrnDirector::SharedPlaylists::Construct
//
// Construct the five shared playlists (race intro, post race, three pause playlists),
// then seed each with its fixed list of takes (all referenced as group/take pairs). The
// take lists differ per playlist; muCurrentPausePlaylist starts at 0.
// ----------------------------------------------------------------------------
void SharedPlaylists::Construct()
{
    mRaceIntroPlaylist.Construct();
    mPostRacePlaylist.Construct();
    for (u32 luPause = 0; luPause < KU_NUM_PAUSE_PLAYLISTS; ++luPause)
    {
        maPausePlaylists[luPause].Construct();
    }

    // Seed table: one row == (playlist, ICE group, take index, vehicle ref type, flash).
    // Each row appends one movie to the named playlist (insert-before-current-end). The
    // group/take/vehicle/flash values are the reconstructed per-playlist seed sequence.
    struct SeedEntry
    {
        ICEMoviePlaylist*   mpPlaylist;
        IceMovie::EIceGroup meGroup;
        u32                 muTake;
        VehicleRef::EType   meVehicleType;
        bool                mbFlash;
    };

    // Every seeded row uses vehicle ref type 0 (player car); muVehicleIndex is 0
    // throughout. The only per-row variation is group / take / flash (pause-1 rows fire
    // the flash hook).
    const VehicleRef::EType leVeh0 = VehicleRef::E_PLAYER_CAR;

    const SeedEntry laSeeds[] =
    {
        // Race-intro playlist (4 entries).
        { &mRaceIntroPlaylist, IceMovie::E_ICE_GROUP_GENERIC_ALL,  42u, leVeh0, false },
        { &mRaceIntroPlaylist, IceMovie::E_ICE_GROUP_EVENTS_START, 20u, leVeh0, false },
        { &mRaceIntroPlaylist, IceMovie::E_ICE_GROUP_GENERIC_ALL,  12u, leVeh0, false },
        { &mRaceIntroPlaylist, IceMovie::E_ICE_GROUP_GENERIC_ALL,  41u, leVeh0, false },

        // Post-race playlist (4 entries).
        { &mPostRacePlaylist,  IceMovie::E_ICE_GROUP_GENERIC_ALL,  31u, leVeh0, false },
        { &mPostRacePlaylist,  IceMovie::E_ICE_GROUP_GENERIC_ALL,  32u, leVeh0, false },
        { &mPostRacePlaylist,  IceMovie::E_ICE_GROUP_GENERIC_ALL,  27u, leVeh0, false },
        { &mPostRacePlaylist,  IceMovie::E_ICE_GROUP_GENERIC_ALL,  12u, leVeh0, false },

        // Pause playlist 0 (4 entries).
        { &maPausePlaylists[0], IceMovie::E_ICE_GROUP_EVENTS_START, 0u, leVeh0, false },
        { &maPausePlaylists[0], IceMovie::E_ICE_GROUP_EVENTS_START, 2u, leVeh0, false },
        { &maPausePlaylists[0], IceMovie::E_ICE_GROUP_EVENTS_START, 3u, leVeh0, false },
        { &maPausePlaylists[0], IceMovie::E_ICE_GROUP_EVENTS_END,   0u, leVeh0, false },

        // Pause playlist 1 (11 entries; each fires the flash hook on start).
        { &maPausePlaylists[1], IceMovie::E_ICE_GROUP_GENERIC_ALL, 36u, leVeh0, true },
        { &maPausePlaylists[1], IceMovie::E_ICE_GROUP_GENERIC_ALL,  7u, leVeh0, true },
        { &maPausePlaylists[1], IceMovie::E_ICE_GROUP_GENERIC_ALL, 19u, leVeh0, true },
        { &maPausePlaylists[1], IceMovie::E_ICE_GROUP_GENERIC_ALL, 21u, leVeh0, true },
        { &maPausePlaylists[1], IceMovie::E_ICE_GROUP_GENERIC_ALL, 28u, leVeh0, true },
        { &maPausePlaylists[1], IceMovie::E_ICE_GROUP_GENERIC_ALL, 32u, leVeh0, true },
        { &maPausePlaylists[1], IceMovie::E_ICE_GROUP_GENERIC_ALL, 27u, leVeh0, true },
        { &maPausePlaylists[1], IceMovie::E_ICE_GROUP_GENERIC_ALL, 18u, leVeh0, true },
        { &maPausePlaylists[1], IceMovie::E_ICE_GROUP_GENERIC_ALL,  3u, leVeh0, true },
        { &maPausePlaylists[1], IceMovie::E_ICE_GROUP_GENERIC_ALL, 24u, leVeh0, true },
        { &maPausePlaylists[1], IceMovie::E_ICE_GROUP_GENERIC_ALL, 12u, leVeh0, true },

        // Pause playlist 2 (13 entries).
        { &maPausePlaylists[2], IceMovie::E_ICE_GROUP_GENERIC_ALL, 43u, leVeh0, false },
        { &maPausePlaylists[2], IceMovie::E_ICE_GROUP_GENERIC_ALL, 19u, leVeh0, false },
        { &maPausePlaylists[2], IceMovie::E_ICE_GROUP_GENERIC_ALL, 36u, leVeh0, false },
        { &maPausePlaylists[2], IceMovie::E_ICE_GROUP_GENERIC_ALL,  7u, leVeh0, false },
        { &maPausePlaylists[2], IceMovie::E_ICE_GROUP_GENERIC_ALL, 21u, leVeh0, false },
        { &maPausePlaylists[2], IceMovie::E_ICE_GROUP_GENERIC_ALL, 28u, leVeh0, false },
        { &maPausePlaylists[2], IceMovie::E_ICE_GROUP_GENERIC_ALL, 32u, leVeh0, false },
        { &maPausePlaylists[2], IceMovie::E_ICE_GROUP_GENERIC_ALL, 27u, leVeh0, false },
        { &maPausePlaylists[2], IceMovie::E_ICE_GROUP_GENERIC_ALL, 18u, leVeh0, false },
        { &maPausePlaylists[2], IceMovie::E_ICE_GROUP_GENERIC_ALL,  3u, leVeh0, false },
        { &maPausePlaylists[2], IceMovie::E_ICE_GROUP_GENERIC_ALL, 24u, leVeh0, false },
        { &maPausePlaylists[2], IceMovie::E_ICE_GROUP_GENERIC_ALL, 26u, leVeh0, false },
        { &maPausePlaylists[2], IceMovie::E_ICE_GROUP_GENERIC_ALL, 12u, leVeh0, false },
    };

    const u32 luNumSeeds = sizeof(laSeeds) / sizeof(laSeeds[0]);
    for (u32 luSeed = 0; luSeed < luNumSeeds; ++luSeed)
    {
        const SeedEntry& lrSeed = laSeeds[luSeed];

        IceMovie lMovie;
        lMovie.SetMovie(lrSeed.meGroup, lrSeed.muTake);
        lMovie.SetStartPosition(0.0f);
        lMovie.SetVehicle(lrSeed.meVehicleType, 0u);
        lMovie.SetShouldFlash(lrSeed.mbFlash);

        lrSeed.mpPlaylist->InsertMovieBefore(lrSeed.mpPlaylist->GetMovieCount(), lMovie);
    }

    muCurrentPausePlaylist = 0;
}



// ----------------------------------------------------------------------------
// BrnDirector::SharedPlaylists::GetPausePlaylist @0x821F59F8
//
//   lwz   r11, 0x1888(this)   ; muCurrentPausePlaylist
//   cmplwi r11, 3             ; < KU_NUM_PAUSE_PLAYLISTS -> assert on >=
//   ...assert...
//   lwz   r11, 0x1888(this)   ; reload muCurrentPausePlaylist
//   addi  r11, r11, 2         ; skip mRaceIntroPlaylist (0) + mPostRacePlaylist (1)
//   mulli r11, r11, 0x4E8     ; * sizeof(ICEMoviePlaylist) (1256)
//   add   r3,  r11, this      ; &maPausePlaylists[muCurrentPausePlaylist]
//
// The (index + 2) * 0x4E8 + this address is exactly &maPausePlaylists[index]: the two
// skipped playlists are mRaceIntroPlaylist (+0x0000) and mPostRacePlaylist (+0x04E8), so
// maPausePlaylists[0] sits at +0x09D0 == 2 * 0x4E8 and each pause slot is one 0x4E8-byte
// playlist further on.
// ----------------------------------------------------------------------------
const ICEMoviePlaylist&
SharedPlaylists::GetPausePlaylist() const
{
    CGS_ASSERT(muCurrentPausePlaylist < KU_NUM_PAUSE_PLAYLISTS,
               "muCurrentPausePlaylist < KI_NUM_PAUSE_PLAYLISTS");
    return maPausePlaylists[muCurrentPausePlaylist];
}

// ----------------------------------------------------------------------------
// NOTE -- BrnDirector::ICEMoviePlaylist::Serialise<S> is BLOCKED (all three instances:
//   <TextFileWriteSerialiser> @0x8224CBE8, <TextFileReadSerialiser> @0x8224CDE8,
//   <DebugMenuSerialiser> @0x8224B240).
//
// Unlike the SharedPlaylists visitor below, the playlist visitor is not a field walk: it drives the
// movie ObjectPool/Array machinery -- serialise a movie count through a "Ignore this" scratch slot,
// Construct() when the file count is smaller than the live list, InsertMovieBefore() a fresh default
// IceMovie when it is larger, then recurse into each movie via IceMovie::Serialise labelling them
// from a per-movie "MovieN" rodata name table. Three faithfulness gaps block a precise reconstruction:
//   1. The movie-count scratch slot (X360 stores it at playlist +0x4DC, label "Ignore this") is not
//      pinned to a named trailing member -- resolving it needs the ObjectPool<...,20,s32> sizings
//      verified so +0x4DC maps to miDebugMenuNewMovieIndex vs miDebugSize.
//   2. The per-movie label table is a 10-byte-stride "Movie1".."Movie20" rodata blob of which only
//      "Movie1" is symbolised; the rest are inferred from the stride, not recovered.
//   3. The InsertMovieBefore default IceMovie is a partial inline init (refType INVALID, startPos 0,
//      vehicleType 0, flash true; other fields left uninitialised) whose shape (Construct-inlined vs
//      explicit) is not pinned.
// The DebugMenuSerialiser instance additionally needs the un-homed DebugMenuSerialiser type (its
// per-movie path is AddToPath + CgsDev::DebugComponent (Un)RegisterVariable, not IceMovie::Serialise).
// Deferred; blocked precisely rather than fabricating the name table / mis-mapping the scratch slot.
// (SharedPlaylists::Serialise below recurses into this visitor; it compiles against the declaration,
// so its own reconstruction is unaffected by this block.)
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// BrnDirector::SharedPlaylists::Serialise<S> -- the ONE shared-playlists field-walk visitor body.
//
// Serialises only the three pause-camera playlists and the current-pause-playlist index (the race
// intro / post-race playlists are NOT part of the debug save). Hands each of the three pause
// playlists to the serialiser as a nested block (S recurses into ICEMoviePlaylist::Serialise), then
// the current-playlist index as a scalar u32. The body is uniform across S; only S's inlined
// helpers differ:
//   - TextFileWriteSerialiser @0x82258C18: three Serialise<ICEMoviePlaylist> section writes, then
//     FormatName + fprintf "%s : %d\n" for the index.
//   - TextFileReadSerialiser  @0x82258CC0: three nested-block reads (consume header line + recurse),
//     then fscanf "%s : %d\n" for the index.
// The DebugMenuSerialiser instance (@0x82252D30) shares this source body but is NOT instantiated
// here -- its serialiser type has no reconstructed home yet (see note below).
//
// Playlist offsets verified against the asm: maPausePlaylists[0/1/2] at +0x09D0/+0x0EB8/+0x13A0
// (0x4E8-byte stride) and muCurrentPausePlaylist at +0x1888.
// ----------------------------------------------------------------------------
template<class TSerialiser>
void SharedPlaylists::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Playlists/Pause playlist 0", maPausePlaylists[0]);
    lrSerialiser.Serialise("Playlists/Pause playlist 1", maPausePlaylists[1]);
    lrSerialiser.Serialise("Playlists/Pause playlist 2", maPausePlaylists[2]);
    lrSerialiser.Serialise("Playlists/Current playlist", muCurrentPausePlaylist);
}

// Explicit instantiations -- one per file serialiser the shared playlists are saved/loaded through.
// BLOCKED (not instantiated): Serialise<Camera::DebugMenuSerialiser> @0x82252D30 -- DebugMenuSerialiser
// has no reconstructed home yet (its pause-playlist recursion is the un-homed sub_8224F218 nested-block
// helper + Process<unsigned int> for the index). It lands with the DebugMenuSerialiser TU.
template void SharedPlaylists::Serialise<Camera::TextFileWriteSerialiser>(Camera::TextFileWriteSerialiser&);
template void SharedPlaylists::Serialise<Camera::TextFileReadSerialiser>(Camera::TextFileReadSerialiser&);

} // namespace BrnDirector
