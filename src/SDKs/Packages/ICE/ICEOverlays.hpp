#ifndef SDKS_PACKAGES_ICE_ICEOVERLAYS_HPP
#define SDKS_PACKAGES_ICE_ICEOVERLAYS_HPP

// ============================================================================
// SDKs/Packages/ICE/ICEOverlays.hpp
//
// MINIMAL SLICE of ICE::ICEOverlay -- the ICE overlay-id holder embedded by value
// in ICE::ICECamera (mICEOverlay @+0x00). DWARF home
// references/DecFIGS/dwarfdump/SDKs/Packages/ICE/ICEOverlays.hpp:
//   struct ICE::ICEOverlay { uint32_t muOverlay;
//       void Construct(); BrnDirector::OverlayEnums::EOverlay GetOverlay();
//       void SetOverlay(int32_t); void UnSetOverlay(); };
//
// None of ICEOverlay's methods are attested in the X360 ledger (progress/
// identity.json has no ICEOverlay symbols), so they are PS3-only / inlined on the
// X360 spine and are NOT declared here -- only the single u32 member the DWARF
// confirms, which is all ICECamera's layout needs (4 bytes at the camera's +0x00).
// Grow this home with the real method set when an X360-attested ICEOverlay TU is
// worked.
// FLAG: minimal layout-only slice (1 member, no methods); X360 ledger attests none.
// ============================================================================

#include "types.hpp"

namespace ICE
{
    // ICEOverlays.hpp:51 (DWARF). Holds the active overlay id; behaviour lives in
    // the (PS3-only here) method set.
    struct ICEOverlay
    {
        u32 muOverlay;   // ICEOverlays.hpp:69
    };
}

#endif // SDKS_PACKAGES_ICE_ICEOVERLAYS_HPP
