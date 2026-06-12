#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x825084D0
//   (BrnGui::FBurnMainHudState::GetResourcesToLoad)
//
// Behaviour-faithful to the X360 pseudocode:
//     *result = &unk_82F26230;     // out: pointer to the resource-tuple table
//     *a2     = dword_82F2622C;    // out: number of resources
//     return result;              // (Hex-Rays __fastcall artifact; effectively void)
//
// Hands the freeburn main-HUD state's static resource list to the loader. The table
// and its count live in .rdata; the IDA export carries no values for them, so they
// are declared here (original addresses noted) and resolved at link time.

namespace BrnGui
{
    struct FBurnMainHudState
    {
        struct ResourceTuple;                            // opaque .rdata record
        static const ResourceTuple maResourcesToLoad[];  // @ 0x82F26230 (unk_82F26230)
        static const u32           muNumResourcesToLoad; // @ 0x82F2622C (dword_82F2622C)

        void GetResourcesToLoad(const ResourceTuple** lppResourceTuples,
                                u32* lpuNumberOfResources) const;
    };

    void FBurnMainHudState::GetResourcesToLoad(const ResourceTuple** lppResourceTuples,
                                               u32* lpuNumberOfResources) const
    {
        *lppResourceTuples    = maResourcesToLoad;
        *lpuNumberOfResources = muNumResourcesToLoad;
    }
}
