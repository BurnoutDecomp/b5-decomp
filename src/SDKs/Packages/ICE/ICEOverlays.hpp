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
        // ICEOverlays.hpp:57 / :60 (DWARF). ⭐ BODIED 2026-09-27 (OWNERLIST lane L5) as the one-word
        // stores the console inlines through ICE::ICECamera::SetOverlay / ::ClearOverlay into
        // ICECameraMover::UpdateOverlay @0x8252E4A8: `stw r3, 0(camera)` (0x8252E508, the new id) and
        // `stw r7(=0), 0(camera)` (0x8252E4FC) -- mICEOverlay is the ICE camera's first member.
        void SetOverlay(s32 liOverlay) { muOverlay = static_cast<u32>(liOverlay); }
        void UnSetOverlay()            { muOverlay = 0; }

        u32 muOverlay;   // ICEOverlays.hpp:69
    };
}

#endif // SDKS_PACKAGES_ICE_ICEOVERLAYS_HPP
