#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82508070
//   (BrnGui::BootPreload::GetResourcesToLoad)
//
// Behaviour-faithful to the X360 pseudocode:
//     *lpaResourceTuples   = &BrnGui::BootPreload::maSecondPhaseResourcesToLoad;
//     *lpuNumberOfResources = BrnGui::BootPreload::muSecondPhaseNumResourcesToLoad;
//
// Hands the boot-preload screen's second-phase resource list to the loader. The
// table and its count are static members living in .rdata; the IDA export carries
// no values for them, so they are declared here and resolved at link time. These
// two symbols kept their recovered names in the X360 build.

namespace BrnGui
{
    struct BootPreload
    {
        struct ResourceTuple;                                       // opaque .rdata record
        static const ResourceTuple maSecondPhaseResourcesToLoad[];
        static const u32           muSecondPhaseNumResourcesToLoad;

        void GetResourcesToLoad(const ResourceTuple** lppResourceTuples,
                                u32* lpuNumberOfResources) const;
    };

    void BootPreload::GetResourcesToLoad(const ResourceTuple** lppResourceTuples,
                                         u32* lpuNumberOfResources) const
    {
        *lppResourceTuples    = maSecondPhaseResourcesToLoad;
        *lpuNumberOfResources = muSecondPhaseNumResourcesToLoad;
    }
}
