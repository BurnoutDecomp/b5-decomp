// ============================================================================
// GameSource/Director/Utils/BrnICEMoviePlayerSerialise.cpp
//
// The SERIALISATION + dev-menu half of BrnICEMoviePlayer.cpp: IceMovie::Serialise<S>,
// ICEMoviePlaylist::Serialise<S>, SharedPlaylists::Serialise<S> -- one body each, with
// their explicit instantiation sets -- and ICEMoviePlaylist::DebugMenuNewMovie.
//
// WHY THIS IS A FILE OF ITS OWN. These bodies' declared home is
// GameSource/Director/Utils/BrnICEMoviePlayer.cpp. Splitting them off is what lets that
// TU -- the ICE movie PLAYER, which the crash-navigation arbitrator state drives -- join
// the link: the explicit instantiations open the three serialisers' scalar and
// nested-block Serialise overloads, whose home Camera serialiser TUs are not
// reconstructed, and DebugMenuNewMovie opens the dev-tools menu registration callee
// below. Neither cost is anything the player half needs. Same file-split pattern as
// BrnDirectorICEWrapperPrepare.cpp: give the code that can land today its own TU.
//
// NOT MOUNTED.
// DELETE-WHEN: the Camera serialiser TUs (TextFileWriteSerialiser /
// TextFileReadSerialiser / DebugMenuSerialiser) and the dev-menu registration callee are
// reconstructed -- then move these bodies back into BrnICEMoviePlayer.cpp.
// ============================================================================

#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"    // CgsCore::SPrintf
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"  // Camera::TextFileWriteSerialiser
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"   // Camera::TextFileReadSerialiser
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h" // Camera::DebugMenuSerialiser

namespace BrnDirector
{


// FLAG: the dev-menu registration callee that (de)registers a playlist's per-movie
// remove-data entry with the debug component. Not-yet-reconstructed dev-tools TU; this is
// a DECLARATION-ONLY external callee (its body links later -- the per-TU `cl /c` gate
// only needs the declaration). The descriptor is built by the caller and passed by
// address; the trailing playlist + name identify the menu owner.
void IceMovieDebugMenuRegisterRemoveEntry(const void* lpDescriptor,
                                          const char* lpcDebugName,
                                          ICEMoviePlaylist* lpPlaylist);


// ----------------------------------------------------------------------------
// BrnDirector::IceMovie::Serialise<S> -- the ONE per-movie field-walk visitor body.
//   <TextFileReadSerialiser>
//   <TextFileWriteSerialiser>
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

// Explicit instantiations -- one per text serialiser the movie is saved/loaded through. Each
// leaf resolves to that serialiser's scalar Serialise(const char*, s32&/u32&/bool&) overload
// (declaration-only on the serialiser side; their bodies link with the serialiser TUs), so this
// TU forces no inner-template instantiation of its own.
template void IceMovie::Serialise<Camera::TextFileWriteSerialiser>(Camera::TextFileWriteSerialiser&);
template void IceMovie::Serialise<Camera::TextFileReadSerialiser>(Camera::TextFileReadSerialiser&);

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
// BrnDirector::ICEMoviePlaylist::Serialise<S> -- the ONE playlist movie-list visitor body.
//   <DebugMenuSerialiser>
//   <TextFileWriteSerialiser>
//   <TextFileReadSerialiser>
//
// Unlike the SharedPlaylists field-walk below, the playlist visitor drives the movie
// ObjectPool/Array machinery: serialise the live movie count through the throwaway "Ignore
// this" scratch member, resize the list to match (Construct() to reset when the incoming
// count is smaller; InsertMovieBefore() a fresh default IceMovie when it is larger), then
// hand each movie to the serialiser as a nested "MovieN" block (S recurses into
// IceMovie::Serialise for the text passes; registers the movie's fields under the path for
// the debug-menu pass). The body is uniform across S; only S's inlined leaf/nested-block
// helpers differ (Write: FormatName+fprintf; Read: fscanf; DebugMenu: Process/AddToPath).
//
// Offsets verified against the asm: mMoviePoolIndicies at playlist +0x380 (its live-count
// word at +0x3D0), and the "Ignore this" scratch slot at +0x4DC == miDebugSize (the pool
// mDebugMenuRemoveData @+0x3D8 is 0x100 bytes, so the trailing s32 pair sits at +0x4D8
// (miDebugMenuNewMovieIndex) / +0x4DC (miDebugSize), pinned by sizeof(ICEMoviePlaylist)
// == 0x4E8 from SharedPlaylists::GetPausePlaylist). The default grow-movie is the asm's
// partial inline init: refType INVALID, startPos 0, vehicleType E_PLAYER_CAR(0), flash true
// (v20[0]=-1, v20[6]=0.0, v20[7]=0, v21=1); its other fields are left uninitialised exactly
// as the console leaves them. The per-movie label is the "Movie1".."Movie20" name the console
// reads from a 10-byte-stride rodata table (only "Movie1" symbolised); reconstructed here as
// the equivalent deterministic "Movie<1-based index>" format.
// ----------------------------------------------------------------------------
template<class TSerialiser>
void ICEMoviePlaylist::Serialise(TSerialiser& lrSerialiser)
{
    // Serialise the movie count through the throwaway "Ignore this" scratch member
    // (miDebugSize): on a write pass it carries the live count out; on a read pass it is
    // overwritten with the count parsed from the file.
    miDebugSize = mMoviePoolIndicies.GetCount();   // GetCount asserts Construct/Clear was called
    lrSerialiser.Serialise("Ignore this", miDebugSize);

    // If the incoming count is smaller than the live list, reset the list so the read pass
    // rebuilds exactly miDebugSize movies from scratch.
    if (miDebugSize < mMoviePoolIndicies.GetCount())
    {
        Construct();
    }

    for (s32 liMovie = 0; liMovie < miDebugSize; ++liMovie)
    {
        // Grow the list with a fresh default movie whenever the target count runs past the
        // live list (append it before the current end).
        if (mMoviePoolIndicies.GetCount() <= liMovie)
        {
            IceMovie lNewMovie;
            lNewMovie.meRefType         = IceMovie::E_REF_TYPE_INVALID;
            lNewMovie.mfStartPosition01 = 0.0f;
            lNewMovie.meVehicleType     = VehicleRef::E_PLAYER_CAR;
            lNewMovie.mbPlayFlash       = true;
            InsertMovieBefore(mMoviePoolIndicies.GetCount(), lNewMovie);
        }

        const s32 liPoolIndex = mMoviePoolIndicies.GetItem(liMovie);
        IceMovie&  lrMovie     = mMoviePool[liPoolIndex];
        CGS_ASSERT(mMoviePoolIndicies.GetItem(liMovie) <= KI_CAPACITY,
                   "mMoviePoolIndicies[liMovie] <= 20");

        // Label the movie "Movie1".."Movie20" and hand it to the serialiser as a nested block.
        char lacName[16];
        CgsCore::SPrintf(lacName, sizeof(lacName), "Movie%i", liMovie + 1);
        lrSerialiser.Serialise(lacName, lrMovie);
    }
}

// Explicit instantiations -- one per serialiser the playlist is saved/loaded/menu-registered
// through. Each drives the count scalar overload + the nested-block Serialise<IceMovie> per
// movie (both declaration-only on the serialiser side; their bodies link with the serialiser
// TUs), so this TU forces no inner-template instantiation of its own.
template void ICEMoviePlaylist::Serialise<Camera::DebugMenuSerialiser>(Camera::DebugMenuSerialiser&);
template void ICEMoviePlaylist::Serialise<Camera::TextFileWriteSerialiser>(Camera::TextFileWriteSerialiser&);
template void ICEMoviePlaylist::Serialise<Camera::TextFileReadSerialiser>(Camera::TextFileReadSerialiser&);

// ----------------------------------------------------------------------------
// BrnDirector::SharedPlaylists::Serialise<S> -- the ONE shared-playlists field-walk visitor body.
//
// Serialises only the three pause-camera playlists and the current-pause-playlist index (the race
// intro / post-race playlists are NOT part of the debug save). Hands each of the three pause
// playlists to the serialiser as a nested block (S recurses into ICEMoviePlaylist::Serialise), then
// the current-playlist index as a scalar u32. The body is uniform across S; only S's inlined
// helpers differ:
//   - TextFileWriteSerialiser: three Serialise<ICEMoviePlaylist> section writes, then
//     FormatName + fprintf "%s : %d\n" for the index.
//   - TextFileReadSerialiser : three nested-block reads (consume header line + recurse),
//     then fscanf "%s : %d\n" for the index.
//   - DebugMenuSerialiser    : three nested-block Serialise<ICEMoviePlaylist> registrations
//     (the un-homed DebugMenuSerialiser::Serialise<ICEMoviePlaylist> helper), then the
//     index leaf which inlines to Process<unsigned int> -- i.e. the u32 Serialise overload's body.
//
// Playlist offsets verified against the asm: maPausePlaylists[0/1/2] at +0x09D0/+0x0EB8/+0x13A0
// (0x4E8-byte stride) and muCurrentPausePlaylist at +0x1888. The DebugMenuSerialiser
// instance walks the same "Playlists/Pause playlist {0,1,2}" + "Playlists/Current playlist"
// names at the same field offsets, confirming the shared source body.
// ----------------------------------------------------------------------------
template<class TSerialiser>
void SharedPlaylists::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Playlists/Pause playlist 0", maPausePlaylists[0]);
    lrSerialiser.Serialise("Playlists/Pause playlist 1", maPausePlaylists[1]);
    lrSerialiser.Serialise("Playlists/Pause playlist 2", maPausePlaylists[2]);
    lrSerialiser.Serialise("Playlists/Current playlist", muCurrentPausePlaylist);
}

// Explicit instantiations -- one per serialiser the shared playlists are saved/loaded/menu-registered
// through. Each ICEMoviePlaylist leaf resolves to the serialiser's nested-block Serialise<ICEMoviePlaylist>
// overload; the u32 index leaf resolves to its scalar Serialise(const char*, u32&) overload.
template void SharedPlaylists::Serialise<Camera::TextFileWriteSerialiser>(Camera::TextFileWriteSerialiser&);
template void SharedPlaylists::Serialise<Camera::TextFileReadSerialiser>(Camera::TextFileReadSerialiser&);
template void SharedPlaylists::Serialise<Camera::DebugMenuSerialiser>(Camera::DebugMenuSerialiser&);

} // namespace BrnDirector
