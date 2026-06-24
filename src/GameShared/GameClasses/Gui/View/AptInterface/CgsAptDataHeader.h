#pragma once

#include "types.hpp"

// CgsGui::AptDataHeader -- the serialised header of an APT (gui/Flash movie) resource.
// Member set, order and types are from the DecFIGS DWARF (CgsAptDataHeader.h): a movie
// name, the movie/character/import data block, a const-data block, the gui-geometry
// object, the header size, and the load state. FixUp/FixDown relocate the four leading
// pointer fields (FixUp adds the load base, FixDown subtracts it back to offsets) and
// descend into the geometry object's own relocation.
//
// On disk every "pointer" is a 32-bit offset-from-base (the X360/PS3 serialised pointer
// width), so the pointer fields are modelled as u32 and converted on use -- the project's
// resource-FixUp idiom (see CgsGuiGeometryObjects.h / GuiGeometryObject). This keeps the
// on-disk layout exact (mpGeomStruct at +12, matching the X360 spine) while still naming
// every field instead of poking raw offsets.

namespace CgsResource { struct GuiGeometryObject; }

namespace CgsGui
{
    struct AptDataHeader
    {
        // CgsAptDataHeader.h:37 - DWARF enum values.
        enum EAptDataState
        {
            E_APTDATASTATE_LOADING = 0,
            E_APTDATASTATE_LOADED  = 1,
            E_APTDATASTATE_ACTIVE  = 2,
        };

        u32 mpacMovieName;   // +0   const char*               (serialised 32-bit pointer)
        u32 mpAptData;       // +4   void*  movie/char/imports  (serialised 32-bit pointer)
        u32 mpConstData;     // +8   void*  const data block    (serialised 32-bit pointer)
        u32 mpGeomStruct;    // +12  CgsResource::GuiGeometryObject* (serialised 32-bit pointer)
        u32 muSizeOfHeader;  // +16
        u32 meCurrentState;  // +20  EAptDataState

        // Load-time relocation: add the load base to the four pointer fields, then descend
        // into the geometry object. FixDown is the inverse (descend first, then subtract).
        // luDelta is the rw::Resource load base; lbEndianSwap is carried for resource-
        // interface parity (the geometry walk does not consult it).
        void FixUp(u32 luDelta);
        void FixDown(u32 luDelta, bool lbEndianSwap);
    };
}
