#pragma once

// MINIMAL SLICE for the RaceCarEntityModuleIO IO-buffer unlock; full layout
// reconstructed by BrnDirectorVehicleInputInterface's own TU (DWARF home
// GameSource/Director/SharedIO/BrnDirectorVehicleInputInterface.h:46). Size 256
// (NOMINAL -- not byte-verified, grown by own TU).
//
// Opaque sub-interface payload embedded BY VALUE in RaceCarEntityModuleIO::
// OutputBuffer_PostPhysics (mDirectorVehicleInputInterface, IO header :597; the
// buffer typedefs it as DirectorVehicleInputInterface). The IO header only returns
// &member (Get/GetDirectorVehicleInputInterface), so a complete sized blob suffices.
//
// DWARF (BrnDirectorVehicleInputInterface.h:46) gives the full member list as a
// single embedded queue:
//   typedef EventQueue<BrnDirector::NewVehicleEvent,50> NewVehicleEventQueue;
//   NewVehicleEventQueue mNewVehicleQueue;   // :67
// reconstructed by this type's own ledger TU (pulls in the real EventQueue generic
// + NewVehicleEvent element cascade -- NOT instantiated here).
//
// alignas(16): the DWARF shows the member is an EventQueue instantiation.

#include "types.hpp"   // reserved blob

namespace BrnDirector
{
    // DWARF: BrnDirectorVehicleInputInterface.h:46
    // (struct BrnDirector::BrnDirectorVehicleInputInterface). Wraps a
    // CgsModule::EventQueue<NewVehicleEvent,50> new-vehicle event queue.
    struct alignas(16) BrnDirectorVehicleInputInterface
    {
        unsigned char maReserved[256]; // NOMINAL -- full layout grown by own TU
    };
}
