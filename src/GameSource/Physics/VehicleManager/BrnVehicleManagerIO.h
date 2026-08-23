// ============================================================================
// b5-decomp/src/GameSource/Physics/VehicleManager/BrnVehicleManagerIO.h
//
// Canonical (DWARF) home for BrnPhysics::Vehicle::VehicleManagerOutputBuffer
// (references/DecFIGS/dwarfdump/.../BrnVehicleManagerIO.h:47):
//
//     struct VehicleManagerOutputBuffer : public IOBuffer {
//         VehicleOutputRequestInterface mVehicleOutputRequestInterface;   // :65
//         void Construct();                                               // :52
//         void Destruct();                                                // :56
//         const VehicleOutputRequestInterface* GetVehicleOutputRequestInterface() const; // :60
//         VehicleOutputRequestInterface*       GetVehicleOutputRequestInterface();       // :61
//     };
//
// The two accessors are the X360 pair this file always pinned:
//     @0x825A0FB0  const  (read-lock bit 4: extrwi r11,r11,1,27)  -> this + 16
//     @0x825A1058  write  (write-lock bit 3: extrwi r11,r11,1,28) -> this + 16
// Their assert paths cite ".../VehicleManager/BrnVehicleManagerIO.h:60/61".
//
// Until this pass the file declared a
// class named `VehicleManagerIO` whose +16 member was a 1-byte
// `VehicleManagerOutputInterfaceStorage` returned by accessors named
// `GetVehicleManagerOutputInterface`. ALL THREE NAMES WERE INVENTED: the DWARF
// names the class VehicleManagerOutputBuffer, the member
// mVehicleOutputRequestInterface, and the accessors
// GetVehicleOutputRequestInterface -- and the PS3 DecFIGS build carries the
// mangled proofs out of line (`_ZN[K]10BrnPhysics7Vehicle26VehicleManagerOutput
// Buffer32GetVehicleOutputRequestInterfaceEv`, plus Construct/Destruct).
// PhysicsModule::Update @0x825B0640 settles the SEMANTIC type too: it feeds
// @0x825A1058's result to DoCrashPrediction / UpdateDrivers / ProcessResetEvents
// (each declared with a VehicleOutputRequestInterface* in that seat) and
// @0x825A0FB0's result to BridgeVehicleManagerRequestsToSimulation
// (`const VehicleOutputRequestInterface*`) and to the request-queue Append run.
// The payload is the REAL committed BrnPhysics::Vehicle::VehicleOutputRequestInterface
// (BrnVehicleOutputInterface.h, 41,936 bytes both targets -- pointer-free
// queues), so the console +16 seat is exact on the host too.
// ============================================================================
#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"  // CgsModule::IOBuffer
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h" // Vehicle::VehicleOutputRequestInterface

namespace BrnPhysics
{
namespace Vehicle
{
    // The vehicle-manager request-channel IO buffer. Pushed/popped on the physics
    // output IO stack once per PhysicsModule::Update ("VehManager") and locked for
    // write while the vehicle manager fills its simulation requests; the module
    // drains it through the two bridge functions and the request Append run.
    struct VehicleManagerOutputBuffer : public CgsModule::IOBuffer
    {
        // DWARF :52. The X360 CreateIOBuffer<VehicleManagerOutputBuffer>
        // @0x8259DAF0 runs this after the stack alloc (PS3 keeps it out of line:
        // VehicleManagerOutputBuffer::Construct): raise the buffer status, then
        // construct the six request queues. The PC template does the same --
        // CreateIOBuffer<T> runs T::Construct (2026-08-15).
        void Construct();

        // DWARF :56 (PS3 out-of-line sibling of Construct).
        void Destruct();

        // ---- the X360-emitted accessor pair ------------------------------------
        const VehicleOutputRequestInterface* GetVehicleOutputRequestInterface() const;  // @0x825A0FB0, read  -> +16
        VehicleOutputRequestInterface*       GetVehicleOutputRequestInterface();        // @0x825A1058, write -> +16

        static void _AssertLayout();

    private:
        u8                            maStatusPad[15];                 // +1..+15 (force +16)
        VehicleOutputRequestInterface mVehicleOutputRequestInterface;  // +16  (DWARF :65)
    };
}
}
