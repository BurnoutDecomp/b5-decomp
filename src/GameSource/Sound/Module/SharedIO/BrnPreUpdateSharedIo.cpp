// ===================================================================================
// BrnSound::Module::Io::EaTraxHelper -- the EA-Trax song-metadata reader.
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   GetNumSongs    @0x82697A20   GetSongRefSpec @0x82697AB8
//   GetSongName    @0x826B0380   GetArtistName  @0x826B03D0   GetAlbumName @0x826B0420
//
// ⭐ WHY THIS FILE EXISTS. These five were DECLARATION-LESS callees: five X360 symbols
// with no type home anywhere in b5-decomp/src. Every consumer had to route around them --
// BrnGuiAlwaysAvailableComponentsManager.cpp FLAGs its two in-game EATrax cases (502/503)
// as deferrals for exactly this reason, and the EA-Trax menu screen could not be
// reconstructed at all without them (EATraxMenuComponent::Initialize asks GetNumSongs how
// many rows to build, and SendDrawInformationToApt asks for each visible row's three
// strings). The X360 file is
// "d:\p4\b5_main\burnout\main\code\gamesource\unity\../Sound/Module/SharedIO/
//  BrnPreUpdateSharedIo.cpp", which is this path.
//
// HOW THE COLLECTION IS RESOLVED. The console holds the BurnoutGlobalData collection key
// in the file static mGlobalDataKey (.data @0x82FFB820, written by the unnamed
// SoundLogicModule body at 0x826C9330 with `std r3, -0x47E0(r11)` @0x826C9788) and hands
// that 64-bit key straight to the generated burnoutglobaldata constructor, whose
// Attrib::Instance base resolves it. This code uses burnoutglobaldata's existing
// ResolveLoadedCollection path and passes the original RefSpec to the generated
// songlist and song constructors. An unresolved collection uses the default layout.
// ===================================================================================

#include "GameSource/Sound/Module/SharedIO/BrnPreUpdateSharedIo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameSource/AttribSys/Generated/classes/burnoutglobaldata.h"   // SongList()
#include "GameSource/AttribSys/Generated/classes/songlist.h"            // Songs / Num_Songs
#include "GameSource/AttribSys/Generated/classes/song.h"                // Name/Artist/Album

namespace BrnSound
{
namespace Module
{
namespace Io
{
    // The X360 .data word at 0x82FFB820. Zero until the sound logic module publishes the
    // BurnoutGlobalData collection key, which is what every accessor below asserts on.
    u64 EaTraxHelper::mGlobalDataKey = 0;

    // ---- GetNumSongs @0x82697A20 --------------------------------------------------
    s32 EaTraxHelper::GetNumSongs() const
    {
        CGS_ASSERT(mGlobalDataKey != 0, "0 != mGlobalDataKey");   // cpp:135

        Attrib::Gen::burnoutglobaldata lGlobalData;
        lGlobalData.ResolveLoadedCollection();
        Attrib::Gen::songlist lSongList(lGlobalData.SongList(), 0);

        // `lwz r31, 0x968(r11)` on the songlist's attribute data == songlist::Num_Songs.
        return lSongList.Num_Songs();
    }

    // ---- GetSongRefSpec @0x82697AB8 -----------------------------------------------
    // songlist::Songs with the console's own bounds assert in front of it. The console
    // inlines the accessor (the `GetLength` / `idx*0x18 + 8` sequence at 0x82697B50 is
    // songlist::Songs verbatim, default block 0x18 bytes), so the call is spelled out
    // here as the accessor it is.
    const Attrib::RefSpec& EaTraxHelper::GetSongRefSpec(s32 liSongIndex) const
    {
        CGS_ASSERT(mGlobalDataKey != 0, "0 != mGlobalDataKey");   // cpp:181

        Attrib::Gen::burnoutglobaldata lGlobalData;
        lGlobalData.ResolveLoadedCollection();
        Attrib::Gen::songlist lSongList(lGlobalData.SongList(), 0);

        CGS_ASSERT(liSongIndex < lSongList.Num_Songs(),
                   "liSongIndex < lSongList.NumSongs()");         // cpp:190

        // Never null: out of range falls back to the songlist's 0x18-byte default block.
        return lSongList.Songs(static_cast<u32>(liSongIndex));
    }

    // ---- the three display strings ------------------------------------------------
    // The console passes each selected track's RefSpec to the song constructor.
    const char* EaTraxHelper::GetSongName(s32 liSongIndex) const
    {
        const Attrib::RefSpec& lrRefSpec = GetSongRefSpec(liSongIndex);
        Attrib::Gen::song lSong(lrRefSpec);
        return lSong.Name();
    }

    const char* EaTraxHelper::GetArtistName(s32 liSongIndex) const
    {
        const Attrib::RefSpec& lrRefSpec = GetSongRefSpec(liSongIndex);
        Attrib::Gen::song lSong(lrRefSpec);
        return lSong.Artist();
    }

    const char* EaTraxHelper::GetAlbumName(s32 liSongIndex) const
    {
        const Attrib::RefSpec& lrRefSpec = GetSongRefSpec(liSongIndex);
        Attrib::Gen::song lSong(lrRefSpec);
        return lSong.Album();
    }
}
}
}
