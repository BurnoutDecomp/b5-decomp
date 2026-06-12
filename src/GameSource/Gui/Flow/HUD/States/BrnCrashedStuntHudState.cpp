#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82508510
//   (BrnGui::CrashedStuntHudState::GetResourcesToLoad)
//
// Behaviour-faithful to the X360 pseudocode:
//     *result = &unk_82F26488;     // out: pointer to the resource-tuple table
//     *a2     = dword_82F264A8;    // out: number of resources
//     return result;              // (Hex-Rays __fastcall artifact; effectively void)
//
// Hands the crashed-stunt HUD state's static resource list to the loader. The table
// and its count live in .rdata; the IDA export carries no values for them, so they
// are declared here (original addresses noted) and resolved at link time.

namespace BrnGui
{
    struct CrashedStuntHudState
    {
        struct ResourceTuple;                            // opaque .rdata record
        static const ResourceTuple maResourcesToLoad[];  // @ 0x82F26488 (unk_82F26488)
        static const u32           muNumResourcesToLoad; // @ 0x82F264A8 (dword_82F264A8)

        void GetResourcesToLoad(const ResourceTuple** lppResourceTuples,
                                u32* lpuNumberOfResources) const;
    };

    void CrashedStuntHudState::GetResourcesToLoad(const ResourceTuple** lppResourceTuples,
                                                  u32* lpuNumberOfResources) const
    {
        *lppResourceTuples    = maResourcesToLoad;
        *lpuNumberOfResources = muNumResourcesToLoad;
    }
}
