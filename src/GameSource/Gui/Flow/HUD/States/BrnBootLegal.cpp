#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82508090
//   (BrnGui::BootLegal::GetResourcesToLoad)
//
// Behaviour-faithful to the X360 pseudocode:
//     *result = &unk_82F25CEC;     // out: pointer to the resource-tuple table
//     *a2     = dword_82F25CF4;    // out: number of resources
//     return result;              // (Hex-Rays __fastcall artifact; effectively void)
//
// Hands the boot-legal screen's static resource list to the loader. The table and
// its count live in .rdata; the IDA export carries no values for them, so they are
// declared here (original addresses noted) and resolved at link time.

namespace BrnGui
{
    struct BootLegal
    {
        struct ResourceTuple;                            // opaque .rdata record
        static const ResourceTuple maResourcesToLoad[];  // @ 0x82F25CEC (unk_82F25CEC)
        static const u32           muNumResourcesToLoad; // @ 0x82F25CF4 (dword_82F25CF4)

        void GetResourcesToLoad(const ResourceTuple** lppResourceTuples,
                                u32* lpuNumberOfResources) const;
    };

    void BootLegal::GetResourcesToLoad(const ResourceTuple** lppResourceTuples,
                                       u32* lpuNumberOfResources) const
    {
        *lppResourceTuples   = maResourcesToLoad;
        *lpuNumberOfResources = muNumResourcesToLoad;
    }
}
