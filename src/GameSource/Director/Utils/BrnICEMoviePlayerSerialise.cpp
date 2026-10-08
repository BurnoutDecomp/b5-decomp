// ============================================================================
// GameSource/Director/Utils/BrnICEMoviePlayerSerialise.cpp
//
// The serialisation + dev-menu half of BrnICEMoviePlayer.cpp: IceMovie::Serialise<S>,
// ICEMoviePlaylist::Serialise<S>, SharedPlaylists::Serialise<S> (one body each, with their
// explicit instantiation sets), the playlist's dev-menu callbacks and accessors, and the
// debug-menu serialiser's playlist overload, DebugMenuSerialiser::Serialise(ICEMoviePlaylist&).
// ============================================================================

#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"  // Camera::TextFileWriteSerialiser
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"   // Camera::TextFileReadSerialiser
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h" // Camera::DebugMenuSerialiser

namespace BrnDirector
{

// ----------------------------------------------------------------------------
// BrnDirector::IceMovie::Serialise<S> -- the ONE per-movie field-walk visitor body.
//   <TextFileReadSerialiser>
//   <TextFileWriteSerialiser>
//   <DebugMenuSerialiser> (implicit; the playlist's debug-menu walk recurses into it)
//
// Recursed into from ICEMoviePlaylist::Serialise (via S's nested-block Serialise<IceMovie>)
// to (de)serialise one movie entry. A movie saved/loaded through the text passes is always a
// GROUP/TAKE reference anchored to the player car, so the body first forces meRefType ==
// E_REF_TYPE_GROUP_TAKE (asm: *this = 1) and meVehicleType == E_PLAYER_CAR (asm: *(this+0x1C)
// = 0), then walks the four editable fields as named scalar leaves through S:
//   "Group"         -> meGroup        (+0x10, the ICE-group enum word)
//   "Take"          -> muTakeIndex    (+0x14)
//   "Vehicle Index" -> muVehicleIndex (+0x20)
//   "Play Flash"    -> mbPlayFlash    (+0x24, the byte flag)
//
// The body is uniform across S; only S's inlined leaf helpers differ:
//   - Write: each leaf is FormatName + fprintf "%s : %d\n" of the field, guarded by
//     `if (mpFile)` (the four `if (*(a2+4))` checks -- mpFile is TextFileWriteSerialiser +0x04).
//   - Read : each leaf is fscanf "%s : %d\n" into the field, guarded by `if (mpFile)`
//     (the four `if (*a2)` checks -- mpFile is TextFileReadSerialiser +0x00). The read of the
//     "Play Flash" line parses an int then stores (value != 0), which the asm renders as the
//     cntlzw/extrwi/xori "!= 0" idiom -- exactly the bool& overload's `store (v != 0)`.
//
// meGroup is the signed EIceGroup enum (its storage IS the 4-byte word the asm reads/writes with
// "%d"); it is handed to S's s32 scalar overload through an aliasing reference so the exact same
// four bytes are read/written, without a separate enum overload (parity, not a value change).
// ----------------------------------------------------------------------------
template<class TSerialiser>
void IceMovie::Serialise(TSerialiser& lrSerialiser)
{
    // A text-serialised movie is always a group/take reference on the player car.
    meRefType     = E_REF_TYPE_GROUP_TAKE;   // *this      = 1
    meVehicleType = VehicleRef::E_PLAYER_CAR; // *(this+0x1C) = 0

    lrSerialiser.Serialise("Group", reinterpret_cast<s32&>(meGroup));
    lrSerialiser.Serialise("Take", muTakeIndex);
    lrSerialiser.Serialise("Vehicle Index", muVehicleIndex);
    lrSerialiser.Serialise("Play Flash", mbPlayFlash);
}

template void IceMovie::Serialise<Camera::TextFileWriteSerialiser>(Camera::TextFileWriteSerialiser&);
template void IceMovie::Serialise<Camera::TextFileReadSerialiser>(Camera::TextFileReadSerialiser&);

// ----------------------------------------------------------------------------
// The playlist's dev-menu accessors.
// ----------------------------------------------------------------------------
const char* ICEMoviePlaylist::DebugGetMovieName(s32 liMovie) const
{
    CGS_ASSERT(mMoviePoolIndicies[liMovie] <= KI_CAPACITY, "mMoviePoolIndicies[liMovie] <= 20");
    static const char laacMovieNames[KI_CAPACITY][10] = {
        "Movie1",  "Movie2",  "Movie3",  "Movie4",  "Movie5",  "Movie6",  "Movie7",
        "Movie8",  "Movie9",  "Movie10", "Movie11", "Movie12", "Movie13", "Movie14",
        "Movie15", "Movie16", "Movie17", "Movie18", "Movie19", "Movie20",
    };
    return laacMovieNames[liMovie];
}

ICEMoviePlaylist::DebugMenuRemoveData ICEMoviePlaylist::GetRemoveData(s32 liMovie)
{
    DebugMenuRemoveData lRemoveData;
    lRemoveData.mpThisPlaylist = this;
    lRemoveData.miIndex        = mMoviePoolIndicies.GetItem(liMovie);
    return lRemoveData;
}

void ICEMoviePlaylist::SetDebugComponent(DebugComponent* lpDebugComponent)
{
    mpDebugComponent = lpDebugComponent;
    CGS_ASSERT(mpDebugComponent != NULL, "mpDebugComponent != NULL");
}

void ICEMoviePlaylist::SetDebugName(const char* lpcDebugName)
{
    mpDebugName = lpcDebugName;
}

s32& ICEMoviePlaylist::GetDebugMenuNewMovieIndex()
{
    return miDebugMenuNewMovieIndex;
}

// ----------------------------------------------------------------------------
// BrnDirector::ICEMoviePlaylist::DebugMenuNewMovie / DebugMenuRemoveMovie -- the dev-menu
// actions. Each strips the playlist's menu entries, edits the movie list, then rebuilds the
// entries for the new layout. DebugMenuNewMovie's user data is the playlist; DebugMenuRemoveMovie's
// is the movie's slot in the remove-data pool.
// ----------------------------------------------------------------------------
void ICEMoviePlaylist::DebugMenuNewMovie(void* lpPlaylist)
{
    ICEMoviePlaylist* lpThis = static_cast<ICEMoviePlaylist*>(lpPlaylist);

    {
        Camera::DebugMenuSerialiser lSerialiser;
        lSerialiser.Construct(lpThis->mpDebugComponent, Camera::DebugMenuSerialiser::E_MODE_REMOVE_FROM_MENU);
        lSerialiser.Serialise(lpThis->mpDebugName, *lpThis);
    }

    IceMovie lNewMovie;
    lNewMovie.SetMovie(IceMovie::E_ICE_GROUP_GENERIC_ALL, 31u);
    lNewMovie.SetStartPosition(0.0f);
    lNewMovie.SetVehicle(VehicleRef::E_RACE_CAR, 0u);
    lNewMovie.SetShouldFlash(true);
    lpThis->InsertMovieBefore(lpThis->miDebugMenuNewMovieIndex - 1, lNewMovie);

    {
        Camera::DebugMenuSerialiser lSerialiser;
        lSerialiser.Construct(lpThis->mpDebugComponent, Camera::DebugMenuSerialiser::E_MODE_ADD_TO_MENU);
        lSerialiser.Serialise(lpThis->mpDebugName, *lpThis);
    }
}

void ICEMoviePlaylist::DebugMenuRemoveMovie(void* lpRemoveData)
{
    DebugMenuRemoveData* lpData     = static_cast<DebugMenuRemoveData*>(lpRemoveData);
    ICEMoviePlaylist*    lpPlaylist = lpData->mpThisPlaylist;

    {
        Camera::DebugMenuSerialiser lSerialiser;
        lSerialiser.Construct(lpPlaylist->mpDebugComponent, Camera::DebugMenuSerialiser::E_MODE_REMOVE_FROM_MENU);
        lSerialiser.Serialise(lpPlaylist->mpDebugName, *lpPlaylist);
    }

    lpPlaylist->mMoviePool.FreeObject(lpData->miIndex);
    lpPlaylist->mMoviePoolIndicies.EraseInstancesOf(lpData->miIndex);
    lpPlaylist->mDebugMenuRemoveData.FreeObject(lpPlaylist->mDebugMenuRemoveData.FindObject(*lpData));

    if (lpPlaylist->miDebugMenuNewMovieIndex > lpPlaylist->GetMovieCount() + 1)
    {
        lpPlaylist->miDebugMenuNewMovieIndex = lpPlaylist->GetMovieCount() + 1;
    }

    {
        Camera::DebugMenuSerialiser lSerialiser;
        lSerialiser.Construct(lpPlaylist->mpDebugComponent, Camera::DebugMenuSerialiser::E_MODE_ADD_TO_MENU);
        lSerialiser.Serialise(lpPlaylist->mpDebugName, *lpPlaylist);
    }
}

// ----------------------------------------------------------------------------
// BrnDirector::ICEMoviePlaylist::Serialise<S> -- the ONE playlist movie-list visitor body.
//   <DebugMenuSerialiser>
//   <TextFileWriteSerialiser>
//   <TextFileReadSerialiser>
//
// Serialise the live movie count through the throwaway "Ignore this" scratch member
// (miDebugSize), resize the list to match (Construct() to reset when the incoming count is
// smaller; InsertMovieBefore() a fresh default IceMovie when it is larger), then hand each
// movie to the serialiser as a nested "MovieN" block. The default grow-movie is the console's
// partial inline init: refType INVALID, startPos 0, vehicleType E_PLAYER_CAR(0), flash true;
// its other fields are left as the console leaves them.
// ----------------------------------------------------------------------------
template<class TSerialiser>
void ICEMoviePlaylist::Serialise(TSerialiser& lrSerialiser)
{
    miDebugSize = GetMovieCount();
    lrSerialiser.Serialise("Ignore this", miDebugSize);

    if (miDebugSize < GetMovieCount())
    {
        Construct();
    }

    for (s32 liMovie = 0; liMovie < miDebugSize; ++liMovie)
    {
        if (GetMovieCount() <= liMovie)
        {
            IceMovie lNewMovie;
            lNewMovie.meRefType         = IceMovie::E_REF_TYPE_INVALID;
            lNewMovie.mfStartPosition01 = 0.0f;
            lNewMovie.meVehicleType     = VehicleRef::E_PLAYER_CAR;
            lNewMovie.mbPlayFlash       = true;
            InsertMovieBefore(GetMovieCount(), lNewMovie);
        }

        IceMovie& lrMovie = mMoviePool[mMoviePoolIndicies.GetItem(liMovie)];
        lrSerialiser.Serialise(DebugGetMovieName(liMovie), lrMovie);
    }
}

template void ICEMoviePlaylist::Serialise<Camera::DebugMenuSerialiser>(Camera::DebugMenuSerialiser&);
template void ICEMoviePlaylist::Serialise<Camera::TextFileWriteSerialiser>(Camera::TextFileWriteSerialiser&);
template void ICEMoviePlaylist::Serialise<Camera::TextFileReadSerialiser>(Camera::TextFileReadSerialiser&);

// ----------------------------------------------------------------------------
// BrnDirector::SharedPlaylists::Serialise<S> -- the ONE shared-playlists field-walk visitor body.
//
// Serialises only the three pause-camera playlists and the current-pause-playlist index (the race
// intro / post-race playlists are NOT part of the debug save). Each pause playlist goes to the
// serialiser as a nested block (the debug-menu serialiser's own ICEMoviePlaylist overload for
// the menu pass), then the current-playlist index as a scalar u32.
// ----------------------------------------------------------------------------
template<class TSerialiser>
void SharedPlaylists::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Playlists/Pause playlist 0", maPausePlaylists[0]);
    lrSerialiser.Serialise("Playlists/Pause playlist 1", maPausePlaylists[1]);
    lrSerialiser.Serialise("Playlists/Pause playlist 2", maPausePlaylists[2]);
    lrSerialiser.Serialise("Playlists/Current playlist", muCurrentPausePlaylist);
}

template void SharedPlaylists::Serialise<Camera::TextFileWriteSerialiser>(Camera::TextFileWriteSerialiser&);
template void SharedPlaylists::Serialise<Camera::TextFileReadSerialiser>(Camera::TextFileReadSerialiser&);
template void SharedPlaylists::Serialise<Camera::DebugMenuSerialiser>(Camera::DebugMenuSerialiser&);

namespace Camera
{

// ----------------------------------------------------------------------------
// DebugMenuSerialiser::Serialise(ICEMoviePlaylist&) -- the playlist's menu section: bind the
// playlist to this menu, walk its movies' fields, then one "Remove Movie" action per movie, a
// "New Movie" action while the list has room, and the "New Movie Index" variable (range 1 to
// one past the movie count).
// ----------------------------------------------------------------------------
void DebugMenuSerialiser::Serialise(const char* lpcName, ICEMoviePlaylist& lrPlaylist)
{
    AddToPath(lpcName);
    lrPlaylist.SetDebugName(lpcName);
    lrPlaylist.SetDebugComponent(mpDebugComponent);
    lrPlaylist.Serialise(*this);

    for (s32 liMovie = 0; liMovie < lrPlaylist.GetMovieCount(); ++liMovie)
    {
        const ICEMoviePlaylist::DebugMenuRemoveData lRemoveData = lrPlaylist.GetRemoveData(liMovie);
        AddToPath(lrPlaylist.DebugGetMovieName(liMovie));
        const s32 liRemoveDataIndex = lrPlaylist.mDebugMenuRemoveData.FindObject(lRemoveData);
        ProcessFunction(&ICEMoviePlaylist::DebugMenuRemoveMovie,
                        &lrPlaylist.mDebugMenuRemoveData[liRemoveDataIndex], "Remove Movie");
        RemoveFromPath(lrPlaylist.DebugGetMovieName(liMovie));
    }

    if (lrPlaylist.GetMovieCount() < ICEMoviePlaylist::KI_CAPACITY)
    {
        ProcessFunction(&ICEMoviePlaylist::DebugMenuNewMovie, &lrPlaylist, "New Movie");
    }

    Serialise("New Movie Index", lrPlaylist.GetDebugMenuNewMovieIndex());
    mpDebugComponent->SetRange(&lrPlaylist.GetDebugMenuNewMovieIndex(), 1, lrPlaylist.GetMovieCount() + 1);

    RemoveFromPath(lpcName);
}

} // namespace Camera
} // namespace BrnDirector
