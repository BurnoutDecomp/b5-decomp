#pragma once

#include "types.hpp"
// ⚠️ FIXED 2026-08-03: this was `#include "DebugSystem/Core/CgsDebugComponent.h"`, which resolves
// against NO -I directory in either the per-TU gate or build_game_exe.bat -- i.e. this header had
// never been compiled by anything. The real path is below. (B5PhysicsHandlingDebugComponent.h
// carried the identical broken include and is fixed the same way.)
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"   // CgsDev::DebugComponent (real base)

// BrnPhysics::Vehicle::PhysicalTrafficManagerDebugComponent - the in-game debug menu/overlay for
// the physical traffic manager (air-ram / driver-control / joint / catchup / reset-on-water
// visualisations). Derives from the real CgsDev::DebugComponent. The many RenderWorld/DrawXxx
// render helpers declared in the DecFIGS DWARF are owned by its own dev-UI pass. Incremental:
// this TU implements the leaf name getter (GetName @0x827DBA50) and Construct.
//
// ⭐ THE DATA LAYOUT IS NOW REAL (2026-08-03). It was an empty slice here and, separately, an
// opaque `{ void* mpVTable; u8 [60]; }` FORK of the same fully-qualified name inside
// BrnPhysicalTrafficManager.h. The ten members below come verbatim from the DecFIGS DWARF
// (BrnPhysicalTrafficManagerDebugComponent.h:137-146) and are confirmed one-for-one by the
// inlined Construct at the tail of PhysicalTrafficManager::Construct
// (0x82636DF8..0x82636E28), which writes exactly ten slots in exactly this order off the
// component base (X360 offsets; sizeof(CgsDev::DebugComponent) == 12 there):
//     +12 stw r31   -> mpTrafficManager   (the owning manager)
//     +16 stb 0     -> mbDrawTrafficAirRams
//     +20 stfs 5.0f -> mfAirRamLengthScale        (flt_8200426C; image-read 40 A0 00 00 == 5.0)
//     +24 stb 0     -> mbDrawDriverControls
//     +25 stb 0     -> mbDrawPhysicalTraffic
//     +26 stb 0     -> mbDrawJoints
//     +27 stb 0     -> mbDrawSimpleTraffic
//     +28 stb 0     -> mbDrawTrafficUsingBoxWorld
//     +29 stb 0     -> mbDrawCatchupTargets
//     +30 stb 0     -> mbDrawResetOnWaterHeight
// The seven consecutive `stb` at 0x18..0x1E are what pin the seven-bool run.

namespace BrnPhysics
{
namespace Vehicle
{
    class PhysicalTrafficManager;   // owner; pointer-only (Construct only stores it)

    class PhysicalTrafficManagerDebugComponent : public CgsDev::DebugComponent
    {
    public:
        // DWARF BrnPhysicalTrafficManagerDebugComponent.cpp:54. Inlined into
        // PhysicalTrafficManager::Construct @0x82636CA8 (no out-of-line symbol survives in the
        // ARTIST image), but DWARF-named with its own .cpp line, so it is homed here and called
        // by name rather than being re-expanded at the call site.
        void Construct(PhysicalTrafficManager* lpTrafficManager);

    protected:
        // @0x827DBA50: the debug-menu display name for this component.
        //   asm: lis r11,aPhysicalTraffi@ha ; addi r3,r11,aPhysicalTraffi@l "Physical traffic" ; blr
        const char* GetName() const override;

    private:
        // Never called -- exists only so offsetof() can see the private members below (offsetof
        // on a private member needs member-function context). The gate FAILS if any pin moves.
        static void _AssertLayout();

        PhysicalTrafficManager* mpTrafficManager;           // :137  X360 +12
        bool                    mbDrawTrafficAirRams;       // :138  X360 +16
        f32                     mfAirRamLengthScale;        // :139  X360 +20
        bool                    mbDrawDriverControls;       // :140  X360 +24
        bool                    mbDrawPhysicalTraffic;      // :141  X360 +25
        bool                    mbDrawJoints;               // :142  X360 +26
        bool                    mbDrawSimpleTraffic;        // :143  X360 +27
        bool                    mbDrawTrafficUsingBoxWorld; // :144  X360 +28
        bool                    mbDrawCatchupTargets;       // :145  X360 +29
        bool                    mbDrawResetOnWaterHeight;   // :146  X360 +30
    };
}
}
