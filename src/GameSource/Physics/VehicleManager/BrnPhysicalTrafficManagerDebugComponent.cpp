#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManagerDebugComponent.h"

#include <cstddef>   // offsetof (layout asserts)

// BrnPhysics::Vehicle::PhysicalTrafficManagerDebugComponent - the leaf ledger function homed by
// the Vehicle-physics group (GetName @0x827DBA50) plus Construct, recovered from its inline
// expansion at the tail of PhysicalTrafficManager::Construct @0x82636CA8. The rest of the
// component's render API is owned by a separate dev-UI pass.

namespace BrnPhysics
{
namespace Vehicle
{
    // ========================================================================================
    // PhysicalTrafficManagerDebugComponent::Construct
    // DWARF: BrnPhysicalTrafficManagerDebugComponent.cpp:54  `void Construct(PhysicalTrafficManager *)`
    //
    // The X360 build INLINED this into PhysicalTrafficManager::Construct, so no out-of-line
    // symbol survives; the store set below is transcribed from that expansion
    // (0x82636DF8..0x82636E28, r29 == the component base). The base's own Construct survives as
    // a real call there -- `bl 0x8284CB38`, which is a single `blr`: the ICF representative for
    // every empty function in the image (IDA labels it BaseCollisionGenerator::Destruct).
    //
    // The one non-zero default is mfAirRamLengthScale = 5.0f (flt_8200426C, read out of the
    // image as 40 A0 00 00). It is a plain scalar-pool constant, not one of the static-init-
    // filled slots that read zero in the image.
    // ========================================================================================
    void PhysicalTrafficManagerDebugComponent::Construct(PhysicalTrafficManager* lpTrafficManager)
    {
        DebugComponent::Construct();                    // bl <empty> @0x82636DF8

        mpTrafficManager           = lpTrafficManager;  // stw  r31,   0x0C(r29)
        mbDrawTrafficAirRams       = false;             // stb  0,     0x10(r29)
        mfAirRamLengthScale        = 5.0f;              // stfs 5.0f,  0x14(r29)   flt_8200426C
        mbDrawDriverControls       = false;             // stb  0,     0x18(r29)
        mbDrawPhysicalTraffic      = false;             // stb  0,     0x19(r29)
        mbDrawJoints               = false;             // stb  0,     0x1A(r29)
        mbDrawSimpleTraffic        = false;             // stb  0,     0x1B(r29)
        mbDrawTrafficUsingBoxWorld = false;             // stb  0,     0x1C(r29)
        mbDrawCatchupTargets       = false;             // stb  0,     0x1D(r29)
        mbDrawResetOnWaterHeight   = false;             // stb  0,     0x1E(r29)
    }

    // ========================================================================================
    // Layout pins. Never called -- exists only so offsetof() can see the private members.
    // X360-RELATIVE: the base's vtable pointer and mpTrafficManager both widen on x64, so the
    // absolute X360 offsets (+12 .. +30) cannot be asserted; every delta below can, and each one
    // is a gap between two of the asm's own store targets.
    // ========================================================================================
    void PhysicalTrafficManagerDebugComponent::_AssertLayout()
    {
        typedef PhysicalTrafficManagerDebugComponent D;
        static_assert(offsetof(D, mbDrawTrafficAirRams) - offsetof(D, mpTrafficManager) == sizeof(void*),
                      "mbDrawTrafficAirRams follows the manager pointer (X360 +12 -> +16)");
        static_assert(offsetof(D, mfAirRamLengthScale) - offsetof(D, mbDrawTrafficAirRams) == 4,
                      "mfAirRamLengthScale is one word past the air-ram flag (X360 +16 -> +20)");
        static_assert(offsetof(D, mbDrawDriverControls)       - offsetof(D, mfAirRamLengthScale)  == 4, "X360 +20 -> +24");
        static_assert(offsetof(D, mbDrawPhysicalTraffic)      - offsetof(D, mbDrawDriverControls) == 1, "X360 +25");
        static_assert(offsetof(D, mbDrawJoints)               - offsetof(D, mbDrawDriverControls) == 2, "X360 +26");
        static_assert(offsetof(D, mbDrawSimpleTraffic)        - offsetof(D, mbDrawDriverControls) == 3, "X360 +27");
        static_assert(offsetof(D, mbDrawTrafficUsingBoxWorld) - offsetof(D, mbDrawDriverControls) == 4, "X360 +28");
        static_assert(offsetof(D, mbDrawCatchupTargets)       - offsetof(D, mbDrawDriverControls) == 5, "X360 +29");
        static_assert(offsetof(D, mbDrawResetOnWaterHeight)   - offsetof(D, mbDrawDriverControls) == 6, "X360 +30");
    }

    // @0x827DBA50  BrnPhysics::Vehicle::PhysicalTrafficManagerDebugComponent::GetName
    //   lis r11,aPhysicalTraffi@ha ; addi r3,r11,aPhysicalTraffi@l "Physical traffic" ; blr
    const char* PhysicalTrafficManagerDebugComponent::GetName() const
    {
        return "Physical traffic";
    }
}
}
