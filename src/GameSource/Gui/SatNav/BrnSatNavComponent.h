#pragma once

// ===================================================================================
// BrnGui::SatNavComponent  -- owning header
//   b5-decomp/src/GameSource/Gui/SatNav/BrnSatNavComponent.h
//
// The sat-nav HUD component. SetCachePointer (@0x82473638) installs the pointer to the
// shared GUI cache the component reads its map/landmark data from.
//
// No prior reconstruction carries this component's full member layout, and there is no
// DecFIGS DWARF for this TU. The recovered shape comes from the X360 access in
// SetCachePointer: a single pointer stored at object +0x25C (stw). The cache pointee type
// is not determined by this accessor (the pointer is only stored, never dereferenced
// here), so it is modeled as an opaque void* of the X360 4-byte pointer width. The
// unrecovered head ahead of the field is a reserved byte-span so mpCache lands at its
// binary offset without raw-offset casting; all access is by name.
// (assert site: BrnSatNav.h:342.)
// ===================================================================================

#include "types.hpp"

namespace BrnGui
{
    class SatNavComponent
    {
    public:
        // @ 0x82473638 - install the GUI cache pointer the component reads from. lpCache must
        // be non-null. Called from RaceMainHudState/FBurnMainHudState::UpdateSetupState.
        void SetCachePointer(void* lpCache);

    private:
        // Reserved storage for the unrecovered component head that precedes the cache
        // pointer (object +0x25C). Layout-recovery padding only.
        u8    maHeadReserved[0x25C];

        // Pointer to the shared GUI cache (X360 stw @ +0x25C). Opaque pointee (only stored
        // by this accessor).
        void* mpCache;   // +0x25C
    };
}
