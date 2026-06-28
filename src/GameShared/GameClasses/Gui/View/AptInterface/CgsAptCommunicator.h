#ifndef CGS_APT_COMMUNICATOR_H
#define CGS_APT_COMMUNICATOR_H

#include "types.hpp"

// ============================================================================
// GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h
//
// Minimal owning home for the APT-communicator value type CgsGui::KeyValue.
//
// The full CgsAptCommunicator translation unit (CgsGui::AptCommunicator and its
// many native-function methods) is not reconstructed here; this header only
// supplies the small data type that the X360 ARTIST build attests through the
// AptComponentList key-value pointer table (Get/SetKeyValue return/take a
// KeyValue*). The type's shape is taken verbatim from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/.../CgsAptCommunicator.h:95-98), which is the
// declared home of CgsGui::KeyValue.
// ============================================================================

namespace CgsGui
{
    // CgsAptCommunicator.h:95 (DWARF) - one stored key/value pair. The APT GUI
    // communicator keeps a pool of these and hands AptComponentList a KeyValue*
    // for each (component, key-slot) cell. mHashedKey is the hash of the key
    // string; macValue is the fixed-width value text.
    struct KeyValue
    {
        // CgsAptCommunicator.h:96 (DWARF) - value text capacity.
        static const s32 KI_MAX_VALUE_LENGTH = 128;

        u32  mHashedKey;                    // CgsAptCommunicator.h:97 (DWARF)
        char macValue[KI_MAX_VALUE_LENGTH]; // CgsAptCommunicator.h:98 (DWARF)
    };
}

#endif // CGS_APT_COMMUNICATOR_H
