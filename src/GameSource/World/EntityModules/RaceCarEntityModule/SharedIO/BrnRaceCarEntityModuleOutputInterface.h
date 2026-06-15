#pragma once

// Race-car entity-module output interface (boot-path subset). Member names/types
// and the EMessageType enum are from the DecFIGS DWARF
// (BrnRaceCarEntityModuleOutputInterface.h), X360-gated. This header currently
// holds only the AudioCarDataLoadedEvent payload that the boot-path
// AudioCarLoadedDataQueue (EventQueue<AudioCarDataLoadedEvent, 16>) embeds; extend
// it with the rest of the output interface as those TUs are reconstructed.
#include "BrnCommonTypes.h"                                         // CgsID
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"    // CgsModule::Event

namespace BrnResource { struct VehicleListEntry; }

namespace BrnWorld
{
namespace RaceCarEntityModuleIO
{
    // Output event: a race car's audio data has finished (un)loading, or a (un)load
    // has been requested. The owning queue is 16-capacity; the event carries no SIMD
    // members so it takes its natural 8-byte alignment (CgsID is 64-bit).
    struct AudioCarDataLoadedEvent : public CgsModule::Event
    {
        enum EMessageType : s32
        {
            E_NONE                = 0,
            E_REQUEST_LOAD_DATA   = 1,
            E_DATA_IS_LOADED      = 2,
            E_REQUEST_UNLOAD_DATA = 3,
            E_DATA_IS_UNLOADED    = 4
        };

        EMessageType                         meMessageType;
        const BrnResource::VehicleListEntry* mpVehicleListEntry;
        CgsID                                mAssetID;
        u8                                   miActiveRaceCarIndex;
        bool                                 mbIsPlayer;
    };
}
}
