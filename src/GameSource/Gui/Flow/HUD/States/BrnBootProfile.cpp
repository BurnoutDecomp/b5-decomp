#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x825080F0
//   (BrnGui::BootProfile::GetResourcesToLoad)
//
// Behaviour-faithful to the X360 pseudocode:
//     *result = &unk_82F25F78;     // out: pointer to the resource-tuple table
//     *a2     = dword_82F25F80;    // out: number of resources
//     return result;              // (Hex-Rays __fastcall artifact; effectively void)
//
// Hands the boot-profile screen's static resource list to the loader. The table and
// its count live in .rdata; the IDA export carries no values for them, so they are
// declared here (original addresses noted) and resolved at link time.

namespace BrnGui
{
    struct BootProfile
    {
        struct ResourceTuple;                            // opaque .rdata record
        static const ResourceTuple maResourcesToLoad[];  // @ 0x82F25F78 (unk_82F25F78)
        static const u32           muNumResourcesToLoad; // @ 0x82F25F80 (dword_82F25F80)

        void GetResourcesToLoad(const ResourceTuple** lppResourceTuples,
                                u32* lpuNumberOfResources) const;
    };

    void BootProfile::GetResourcesToLoad(const ResourceTuple** lppResourceTuples,
                                         u32* lpuNumberOfResources) const
    {
        *lppResourceTuples    = maResourcesToLoad;
        *lpuNumberOfResources = muNumResourcesToLoad;
    }
}
