#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"    // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // CgsDev::Log::gpDebugPrint (boot witness)
#include "GameSource/Director/BrnDirectorICEWrapper.h"     // BrnDirector::ICEWrapper (PlayMovie / IsPlayingMovie / GetCamera)

// ============================================================================
// GameSource/Director/Utils/BrnICEMoviePlayer.cpp
//
// The ICE movie player and playlist family: ICEMoviePlayer's whole body set, the
// ICEMoviePlaylist / SharedPlaylists build-and-query bodies, and the take-id helper they
// share. Reconstructed for semantic parity; members
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

// ---- The three Serialise<S> visitor bodies with their instantiation sets, and
//      ICEMoviePlaylist::DebugMenuNewMovie, live in the sibling TU
//      BrnICEMoviePlayerSerialise.cpp. Merge them back into this one when the Camera
//      serialiser TUs and the dev-menu registration callee are reconstructed.
// ----------------------------------------------------------------------------
// BrnDirector::ICEMoviePlaylist::Construct
//
// Reset the playlist to empty: both object pools back to their all-free state, the play
// order array back to zero elements, no debug owner, and the dev-menu "new movie" slot at
// 1 (the 1-based insert position DebugMenuNewMovie later turns back into a 0-based index).
//
// The console body is entirely inlined container work, and it is the pools' Clear() shape
// -- occupancy bits cleared, free queue refilled DESCENDING (19..0 stored front-to-back)
// and the free count set to the capacity -- NOT the lifecycle Construct() that only clears
// the occupancy word. The play-order array gets Construct() (count = 0).
// miDebugSize is deliberately left alone: it is the throwaway "Ignore this" scratch slot
// the Serialise visitor writes, and the console does not touch it here.
// ----------------------------------------------------------------------------
void ICEMoviePlaylist::Construct()
{
    mMoviePool.Clear();
    mMoviePoolIndicies.Construct();
    mDebugMenuRemoveData.Clear();

    mpDebugComponent = 0;
    mpDebugName      = 0;

    // The dev menu's insert position is 1-based (DebugMenuNewMovie inserts before
    // miDebugMenuNewMovieIndex - 1), so an empty playlist starts it at 1.
    miDebugMenuNewMovieIndex = 1;
}

// ----------------------------------------------------------------------------
// BrnDirector::ICEMoviePlaylist::InsertMovieBefore
//
// Insert a copy of lrMovie into the playlist so that it lands at play position
// liDesiredZeroBasedIndex:
//   1. take a slot out of the movie pool and copy the movie into it,
//   2. splice that pool index into the play-order array at the requested position,
//   3. take the matching slot out of the dev-menu remove-command pool and point it back at
//      this playlist / that movie slot,
//   4. re-derive the dev menu's 1-based "new movie" position from the new movie count.
//
// The two pools are allocated in lock-step and have the same capacity, so the console
// asserts the two slot indices came back equal. The per-movie copy is the
// compiler-generated IceMovie copy-assign (the console emits it as a block move of that
// struct's console size); expressed here as the assignment it came from.
// ----------------------------------------------------------------------------
void ICEMoviePlaylist::InsertMovieBefore(s32 liDesiredZeroBasedIndex, const IceMovie& lrMovie)
{
    const s32 liMoviePoolIndex = mMoviePool.AllocateObject();
    CGS_ASSERT(liMoviePoolIndex != -1, "liMoviePoolIndex != -1");

    mMoviePool[liMoviePoolIndex] = lrMovie;

    CGS_ASSERT(liDesiredZeroBasedIndex >= 0, "liDesiredZeroBasedIndex >= 0");
    mMoviePoolIndicies.InsertBefore(static_cast<u32>(liDesiredZeroBasedIndex), liMoviePoolIndex);

    const s32 liIndex = mDebugMenuRemoveData.AllocateObject();
    CGS_ASSERT(liIndex != -1, "liIndex != -1");

    mDebugMenuRemoveData[liIndex].mpThisPlaylist = this;
    mDebugMenuRemoveData[liIndex].miIndex        = liMoviePoolIndex;
    CGS_ASSERT(liIndex == liMoviePoolIndex, "liIndex == liMoviePoolIndex");

    // GetCount() carries the "Array used before Construct/Clear was called" assert.
    miDebugMenuNewMovieIndex = mMoviePoolIndicies.GetCount() + 1;
}

// ----------------------------------------------------------------------------
// BrnDirector::ICEMoviePlaylist::GetMovieCount
//
// The number of movies in the list == the order array's live-element count. Asserts the
// order array was Construct/Clear'ed (its count is off the -1 sentinel).
// ----------------------------------------------------------------------------
s32 ICEMoviePlaylist::GetMovieCount() const
{
    CGS_ASSERT(mMoviePoolIndicies.GetCount() != -1,
               "Array used before Construct/Clear was called");
    return mMoviePoolIndicies.GetCount();
}

// ----------------------------------------------------------------------------
// BrnDirector::SharedPlaylists::Construct
//
// Construct the five shared playlists (race intro, post race, three pause playlists),
// then seed each with its fixed list of takes (all referenced as group/take pairs). The
// take lists differ per playlist; muCurrentPausePlaylist starts at 0.
//
// Construct order follows the console: the three pause playlists run as a counted loop,
// then the race-intro and post-race playlists are constructed individually.
// ----------------------------------------------------------------------------
void SharedPlaylists::Construct()
{
    for (u32 luPause = 0; luPause < KU_NUM_PAUSE_PLAYLISTS; ++luPause)
    {
        maPausePlaylists[luPause].Construct();
    }
    mRaceIntroPlaylist.Construct();
    mPostRacePlaylist.Construct();

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

    // TEMPORARY p0-wave boot witness -- proves the shared playlists really are seeded now
    // that this TU is in the link (they were all empty while the scaffold body stood in).
    // Not console behaviour; remove once the wave is signed off.
    {
        static bool sbLoggedOnce = false;
        if (!sbLoggedOnce)
        {
            sbLoggedOnce = true;
            const s32       liFirstIntroSlot = mRaceIntroPlaylist.mMoviePoolIndicies.GetItem(0);
            const IceMovie& lrFirstIntro     = mRaceIntroPlaylist.mMoviePool[liFirstIntroSlot];
            *CgsDev::Log::gpDebugPrint
                << "[p0-icemovie] SharedPlaylists::Construct raceIntro="
                << mRaceIntroPlaylist.GetMovieCount()
                << " postRace=" << mPostRacePlaylist.GetMovieCount()
                << " pause0=" << maPausePlaylists[0].GetMovieCount()
                << " pause1=" << maPausePlaylists[1].GetMovieCount()
                << " pause2=" << maPausePlaylists[2].GetMovieCount()
                << " firstIntroMovie: group " << static_cast<s32>(lrFirstIntro.meGroup)
                << " take " << lrFirstIntro.muTakeIndex << "\n";
        }
    }
}

// ----------------------------------------------------------------------------
// BrnDirector::SharedPlaylists::GetPausePlaylist
//
// The pause playlist currently selected by muCurrentPausePlaylist. The console reaches it
// by (index + 2) * sizeof(ICEMoviePlaylist) from the object base -- the two skipped
// playlists being mRaceIntroPlaylist (+0x0000) and mPostRacePlaylist (+0x04E8), so
// maPausePlaylists[0] sits at +0x09D0 and each pause slot is one playlist further on.
// Reached BY NAME here.
// ----------------------------------------------------------------------------
const ICEMoviePlaylist&
SharedPlaylists::GetPausePlaylist() const
{
    CGS_ASSERT(muCurrentPausePlaylist < KU_NUM_PAUSE_PLAYLISTS,
               "muCurrentPausePlaylist < KI_NUM_PAUSE_PLAYLISTS");
    return maPausePlaylists[muCurrentPausePlaylist];
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
// BrnDirector::ICEMoviePlayer::ApplyFlashHookToCamera  (private helper)
//
// Fire the "2dFlash" start hook on this player's own camera, at full blend.
//
// NO STANDALONE SYMBOL: the recorded code folds this into ICEMoviePlayer::Update, which
// emits the same three stores at each of its three flash sites -- the start-hook name
// wrapper's Set against the camera's effects block (camera +0x68), the blend amount
// (effects +0x80) and the has-start-hook flag (effects +0xB7). That is exactly
// CameraEffects::SetStartHookName(name, blend). Both operands are hoisted once into the
// function prologue of the enclosing loop: the name is the literal "2dFlash" and the blend
// is the shared 1.0f constant, so all three sites are identical and unconditional -- the
// only guard is the caller's own GetShouldFlash() test on the movie entry.
// ----------------------------------------------------------------------------
void ICEMoviePlayer::ApplyFlashHookToCamera()
{
    mCamera.GetEffects().SetStartHookName("2dFlash", 1.0f);
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
        // The blend's produced camera lives on the pool HELPER the handle names, not on the
        // behaviour -- the console reads it through the handle here (and the behaviour only
        // for the finished flag on the next line).
        mCamera = mOutInterpolator.GetProducedCamera();
        Camera::BehaviourInterpolate* lpOut = mOutInterpolator.GetBehaviour();
        if (lpOut->HasFinished())
        {
            mbHasReachedEnd = true;
        }
    }
    else if (mInInterpolator.IsAllocated())
    {
        mCamera = mInInterpolator.GetProducedCamera();
        Camera::BehaviourInterpolate* lpIn = mInInterpolator.GetBehaviour();
        if (lpIn->HasFinished())
        {
            mbInterpolateIn = false;
            mInInterpolator.GetManager()->ReleaseBehaviour(mInInterpolator);
        }
    }
    else
    {
        // ICEWrapper::GetCamera returns a POINTER (DWARF ICEWrapper.hpp:198; the console
        // null-checks the returned address at CameraReference::GetCamera @0x8223EB58).
        mCamera = *mpICEWrapper->GetCamera();
    }

    if (mbInterpolateOutNow && !mOutInterpolator.IsAllocated())
    {
        lrBehaviourManager.NewBehaviourInterpolate(mOutInterpolator, 0, 0, 1);

        Camera::BehaviourInterpolate* lpOut = mOutInterpolator.GetBehaviour();
        lpOut->SetInterpolationMode(mbInterpolateOutUpdatesDuringPause ? 2 : 0);
        lpOut->SetParameters(mpInterpolateOutParams);
        lpOut->SetupDuration(mfInterpolateOutDuration);
        lpOut->SetupCameraAFromCamera(*mpICEWrapper->GetCamera());
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
        // Camera B is the ICE wrapper's live take camera. (SETTLED 2026-08-01: the old FLAG
        // here -- "the source passes a pointer to the wrapper" -- was reading the console's
        // POINTER-returning ICEWrapper::GetCamera, DWARF ICEWrapper.hpp:198; the accessor is
        // now typed and bodied that way, so the deref is explicit and the FLAG is retired.)
        lpInterpolate->SetupCameraBFromCamera(*mpICEWrapper->GetCamera());
    }

    CGS_ASSERT(mInInterpolator.IsAllocated(), "IsAllocated()");
    mInInterpolator.GetBehaviour()->Setup();

    mbInterpolateIn = true;
}

} // namespace BrnDirector
