// ============================================================================
// GameSource/Director/Utils/BrnICEMoviePlayer_wP0_01.cpp
//
// The ICE movie PLAYLIST half of BrnICEMoviePlayer.cpp: everything the boot path
// actually runs -- ICEMoviePlaylist::Construct / InsertMovieBefore / GetMovieCount and
// SharedPlaylists::Construct / GetPausePlaylist.
//
// WHY THIS IS A FILE OF ITS OWN. These bodies' declared home is
// GameSource/Director/Utils/BrnICEMoviePlayer.cpp, but that TU also carries the
// ICEMoviePlayer half and the three Serialise<> instantiation sets, and mounting the whole
// file costs the link 24 unresolved externals it does not have today (the camera
// BehaviourInterpolate / BehaviourManager handle overloads, ICEWrapper::PlayMovie and
// ::IsPlayingMovie, the three camera-tunings serialisers' scalar + nested-block overloads,
// and ICEMoviePlayer::ApplyFlashHookToCamera, whose body needs the camera effects layer
// modelled first). The playlist half needs NONE of them. Same file-split pattern as
// BrnDirectorICEWrapperPrepare.cpp: give the code that can land today its own TU and leave
// the rest of the class family's home file where it is.
// DELETE-WHEN: BrnICEMoviePlayer.cpp joins the link -- then move these bodies back into it.
//
// WHAT IT UNBLOCKS. Until 2026-09-08 SharedPlaylists::Construct was an empty body in
// DirectorLinkStubs.cpp. It runs at boot (the arbitrator's state container Constructs a
// SharedPlaylists by value), so every director-owned playlist -- race intro, post race and
// the three pause-camera playlists -- was left with a zero movie count AND with its movie
// pool never Clear()ed, i.e. the free queue and the occupancy bits were whatever the
// surrounding memory held. Anything that later walked a shared playlist saw an empty list.
// ============================================================================

#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"          // CgsDev::Log::gpDebugPrint (boot witness)

namespace BrnDirector
{

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

} // namespace BrnDirector
