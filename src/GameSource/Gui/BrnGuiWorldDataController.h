#ifndef BRN_GUI_WORLD_DATA_CONTROLLER_H
#define BRN_GUI_WORLD_DATA_CONTROLLER_H

#include "types.hpp"

// BrnGui::WorldDataController -- the GUI-side world/progression data front-end. The cache
// owns one; the sat-nav renderer asks it for event records and the online-landmark count
// when building its on-map icon set.
//
// Reconstructed from references/DecFIGS/dwarfdump/.../BrnGuiWorldDataController.h, gated on
// the X360 ARTIST ledger. MINIMAL OWNING SLICE: only the methods the in-scope callers reach
// are declared (the full controller holds the trigger/progression/vehicle/colour/street
// resource pointers and the receiver queue -- all uncommitted and OMITTED here). The full
// member layout is not modelled; callers only use these accessors. Bodies link from the
// WorldDataController TU. FLAG: declaration-only boundary class.

namespace BrnProgression { struct RaceEventData; }

namespace BrnGui
{
    struct WorldDataController
    {
        // DWARF h:103 -- look up the serialised event record for an event id. The sat-nav
        // renderer reads the record's event-type / display fields off the result.
        const BrnProgression::RaceEventData* GetEventInfoFromEventId(u32 luEventId) const;

        // DWARF h:116 -- number of online landmarks currently available (GetNumIcons clamps
        // the online-landmark icon count to KU_MAX_SATNAV_ICONS against this).
        s32 GetTotalNumberOfOnlineLandmarks() const;
    };
}

#endif // BRN_GUI_WORLD_DATA_CONTROLLER_H
