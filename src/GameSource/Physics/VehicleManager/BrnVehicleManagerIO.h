// ============================================================================
// b5-decomp/src/GameSource/Physics/VehicleManager/BrnVehicleManagerIO.h
//
// Canonical (DWARF) home for the BrnPhysics::Vehicle vehicle-manager IO buffer
// (assert strings .../VehicleManager/BrnVehicleManagerIO.h:60/61). MINIMAL-COMPLETE
// slice covering ONLY the two X360-emitted lock-guarded accessors owned by this group:
//
//   GetVehicleManagerOutputInterface() const  @ 0x825A0FB0  read  (bit 4) -> +16  (:60)
//   GetVehicleManagerOutputInterface()        @ 0x825A1058  write (bit 3) -> +16  (:61)
//
// Both return the address of the buffer's first payload member (this + 16), which sits
// immediately after the 16-byte CgsModule::IOBuffer base (1-byte status FlagSet padded
// to 16). The const (read) handle tests the read-lock bit (((*a1 >> 4) & 1)); the
// non-const (write) handle tests the write-lock bit (((*a1 >> 3) & 1)) -- matching
// CgsModule::IOBuffer::IsBufferLockedForReading()/IsBufferLockedForWriting(). The asserts
// are non-gating tripwires; each returns the member's address unconditionally.
//
// FLAG (foreign type): VehicleManagerOutputInterface has its own owning home
// (BrnVehicleOutputInterface.h, currently a 256-byte NOMINAL blob); the full member set
// is that type's own ledger TU. Here it is an opaque payload at +16 so the X360-pinned
// return offset is exact. Adopt the named type additively when its full layout lands.
#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"  // CgsModule::IOBuffer

namespace BrnPhysics
{
namespace Vehicle
{
    // The vehicle-manager output-bridge IO buffer. Pushed/popped on the physics IO stack
    // and locked for read/write while PhysicsModule bridges the vehicle manager to the
    // simulation (callers: BridgeVehicleManagerToSimulation_PostScene / _PostPhysics /
    // PostSceneUpdate / Update).
    class VehicleManagerIO : public CgsModule::IOBuffer
    {
    public:
        // Opaque foreign-type payload (see FLAG above): a 1-byte placeholder pinned to +16
        // by the explicit padding member below.
        struct VehicleManagerOutputInterfaceStorage { unsigned char maBytes[1]; };

        // ---- accessors owned/bodied by this group --------------------------------------
        const VehicleManagerOutputInterfaceStorage* GetVehicleManagerOutputInterface() const;  // +16, read
        VehicleManagerOutputInterfaceStorage*       GetVehicleManagerOutputInterface();         // +16, write

        static void _AssertLayout();

    private:
        u8                                   maStatusPad[15];                // +1..+15 (force +16)
        VehicleManagerOutputInterfaceStorage mVehicleManagerOutputInterface; // +16
    };
}
}
