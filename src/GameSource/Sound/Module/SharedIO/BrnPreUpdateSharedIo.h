#pragma once

// ============================================================================
// b5-decomp/src/GameSource/Sound/Module/SharedIO/BrnPreUpdateSharedIo.h
//
// Canonical (DWARF) home for the BrnSound::Module::Io pre-update shared-IO types
// (BrnPreUpdateSharedIo.h). MINIMAL slice: it currently homes only
//   * AudioEffectsMessageQueue -- DWARF BrnPreUpdateSharedIo.h:56, verbatim an
//     empty subclass of CgsModule::VariableEventQueue<128,16>. Pointer-free
//     (inline buffer + s32 cursors), so its host sizeof equals the X360 0x90.
//   * EaTraxHelper -- DWARF BrnPreUpdateSharedIo.h:230, the EA-Trax song-metadata
//     reader the music menu and the in-game "now playing" chyron both go through.
//     Added 2026-09-16 because it was a DECLARATION-LESS callee: five X360 symbols
//     (GetNumSongs / GetSongRefSpec / GetSongName / GetArtistName / GetAlbumName)
//     with no type home anywhere in the tree, which is why
//     BrnGuiAlwaysAvailableComponentsManager.cpp had to FLAG its two in-game EATrax
//     cases as deferrals. Bodies in BrnPreUpdateSharedIo.cpp.
// PreUpdateOutput (DWARF :149) is currently sliced in
// GameSource/Sound/Module/BrnRootSoundModuleIo.h (which documents this file as
// the canonical home); it migrates here when its own TU lands -- do NOT define a
// second copy.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::VariableEventQueue<128,16>
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"  // Attrib::RefSpec

namespace BrnSound
{
namespace Module
{
namespace Io
{
    enum eEffectsMessageTypes
    {
        E_EFFECTS_MESSAGE_TYPES_INVALID = 0,
        E_EFFECTS_MESSAGE_TYPES_POP = 1,
        E_EFFECTS_MESSAGE_TYPES_VOICEOVER_FINISHED = 2,
        E_EFFECTS_MESSAGE_TYPES_COUNT = 3
    };

    template <eEffectsMessageTypes teEventType>
    struct AudioEffectsMessageEvent : public CgsModule::Event
    {
        eEffectsMessageTypes GetEventType() const { return teEventType; }
    };

    struct PopEffectsMessage
        : public AudioEffectsMessageEvent<E_EFFECTS_MESSAGE_TYPES_POP>
    {
        PopEffectsMessage() : muRaceCarID(0), mfIntensity(0.0f) {}
        void Construct(u8 auRaceCarID, f32 afIntensity)
        {
            muRaceCarID = auRaceCarID;
            mfIntensity = afIntensity;
        }

        u8 muRaceCarID;
        f32 mfIntensity;
    };

    // DWARF BrnPreUpdateSharedIo.h:56 -- verbatim empty subclass.
    struct AudioEffectsMessageQueue : public CgsModule::VariableEventQueue<128, 16>
    {
    };

    // ------------------------------------------------------------------------
    // BrnSound::Module::Io::EaTraxHelper  -- DWARF BrnPreUpdateSharedIo.h:230
    //
    // The EA-Trax song-metadata reader. STATELESS: its one data member is the file
    // static mGlobalDataKey (X360 @0x82FFB820), so the object itself is empty --
    // which is why EATraxMenuComponent's mEATraxHelper occupies exactly the four
    // bytes between +0x8C and +0x90.
    //
    // Every accessor opens by asserting the key is set ("0 != mGlobalDataKey",
    // BrnPreUpdateSharedIo.cpp:135 / :181), builds a burnoutglobaldata from it,
    // takes its SongList() RefSpec (the generated accessor at global data +0x548)
    // and reads through the resulting songlist.
    // ------------------------------------------------------------------------
    struct EaTraxHelper
    {
        // @0x826B0380 / @0x826B03D0 / @0x826B0420 -- the three display strings. Each
        // builds an Attrib::Gen::song on GetSongRefSpec(liSongIndex) and reads one
        // word of its attribute data (+0x00 / +0x08 / +0x0C).
        const char* GetSongName(s32 liSongIndex) const;
        const char* GetArtistName(s32 liSongIndex) const;
        const char* GetAlbumName(s32 liSongIndex) const;

        // @0x82697A20 -- the song list's own element count (songlist::Num_Songs).
        s32 GetNumSongs() const;

        // The X360 has no out-of-line symbol for this: the one writer inlines the
        // store (`std r3, -0x47E0(r11)` @0x826C9788, inside the unnamed
        // SoundLogicModule body at 0x826C9330).
        void SetGlobalDataKey(u64 luGlobalDataKey) { mGlobalDataKey = luGlobalDataKey; }

    private:
        // @0x82697AB8 -- the RefSpec of song liSongIndex, i.e. songlist::Songs with
        // the console's extra "liSongIndex < lSongList.NumSongs()" assert in front
        // (BrnPreUpdateSharedIo.cpp:190). Returns the songlist's own 0x18-byte
        // default block when the index is out of range, so the result is never null.
        const Attrib::RefSpec& GetSongRefSpec(s32 liSongIndex) const;

        // DWARF :289. `extern` in the dwarfdump == a class static, and it IS one:
        // the X360 keeps it in .data at 0x82FFB820 and every accessor loads it with
        // a 64-bit `ld`, so it is u64 here and not the SDK's later `typedef u32 Key`.
        static u64 mGlobalDataKey;
    };
}
}
}
