// ============================================================================
// b5-decomp/src/GameSource/Sound/Module/SharedIO/BrnSoundRootSharedIO.h
//
// Canonical (DWARF) home for the BrnSound::Module::Io sound-root shared-IO types
// (BrnSoundRootSharedIO.h). This is a MINIMAL-COMPLETE slice: it currently homes
// only SoundWorldLoadEvent -- the element type of the
// BaseEventQueue<SoundWorldLoadEvent> instantiation whose AddEvent/Append the X360
// emitted out-of-line (0x822C9B88 / 0x823C4238). The other BrnSoundRootSharedIO.h
// types (UpdateInfo, AudioEffectsMessageQueue, PreUpdateOutput, RootInputBuffer,
// RootOutputBuffer, RootPreUpdateOutputBuffer, the queue typedefs) are intentionally
// NOT reproduced here yet; their own TUs grow this header ADDITIVELY when they land.
//
// Member names/types and the eLoadEvent enumerators are from the DecFIGS DWARF
// (BrnSoundRootSharedIO.h:45..92), X360-gated. Layout (DWARF-faithful):
//   +0  eLoadEvent meEvent   (s32)
//   +4  u16        mu16Zone
// => sizeof 8 (4B enum + 2B u16 + 2B trailing pad to the enum's 4-byte alignment).
// The 8-byte stride is confirmed by the X360 BaseEventQueue<SoundWorldLoadEvent>
// bodies: AddEvent copies two dwords (`*v12=*a2; v12[1]=a2[1]`) at `8*miLength`, and
// Append XMemCpy's `8 * srcLen` bytes.
#pragma once

#include "types.hpp"   // s32, u16

namespace BrnSound
{
namespace Module
{
namespace Io
{
    // BrnSoundRootSharedIO.h:45 -- published when the sound module is told the
    // world's pass-by audio map has finished loading / is about to unload, tagged
    // with the world zone the map belongs to.
    struct SoundWorldLoadEvent
    {
        // BrnSoundRootSharedIO.h:53
        enum eLoadEvent : s32
        {
            E_LOAD_EVENT_INVALID            = 0,
            E_LOAD_EVENT_PASSBY_MAP_LOADED  = 1,
            E_LOAD_EVENT_PASSBY_MAP_UNLOAD  = 2,
        };

        // BrnSoundRootSharedIO.h:71 -- own TU (declared-only here).
        void Construct(eLoadEvent leEvent, u16 lu16Zone);
        // BrnSoundRootSharedIO.h:78 -- own TU.
        eLoadEvent GetEvent() const;
        // BrnSoundRootSharedIO.h:85 -- own TU.
        u16 GetZone() const;

    private:
        eLoadEvent meEvent;   // :91
        u16        mu16Zone;  // :92
    };
}
}
}
