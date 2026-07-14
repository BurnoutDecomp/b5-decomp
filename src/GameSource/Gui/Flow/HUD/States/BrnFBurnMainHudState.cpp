#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. Self-contained per-class reconstruction
// (the freeburn main-HUD GUI state). Functions recovered here:
//   FBurnMainHudState::FBurnMainHudState()        @ 0x82508388  (ctor)
//   FBurnMainHudState::GetResourcesToLoad(...)    @ 0x825084D0
//
// The object is large (last ctor store at +0x5390 -> size 0x5394). The ctor stores the
// most-derived vtable at +0x000 then ~50 embedded sub-component vtables/fields at fixed
// offsets across the body (off_820753B8, off_82072F6C, off_82071688, the repeated
// &off_82071638 / &off_82071660 / off_82071690 component vtables, ...). Those stores are
// the compiler's own initialisation of the state's embedded sub-objects (HUD components),
// whose individual types are not recoverable from this ctor alone; the recovered
// observable effect is "construct the freeburn-main-HUD state with its components
// default-initialised". The reserved body below carries that layout; access is by name.
// Mirrors the committed sibling BrnGui::RaceMainHudState (ctor @ 0x82508110).

namespace BrnGui
{
    struct FBurnMainHudState
    {
        struct ResourceTuple;                            // opaque .rdata record
        static const ResourceTuple maResourcesToLoad[];  // @ 0x82F26230 (unk_82F26230)
        static const u32           muNumResourcesToLoad; // @ 0x82F2622C (dword_82F2622C)

        FBurnMainHudState();                             // @ 0x82508388

        void GetResourcesToLoad(const ResourceTuple** lppResourceTuples,
                                u32* lpuNumberOfResources) const;

        // --- recovered layout (guest 32-bit offsets) -------------------------------------
        // Most-derived vtable @ +0x000 + embedded HUD sub-object vtables/fields across the
        // body; individual component types are not recoverable from the ctor alone.
        u8 maReserved[0x5394];                           // +0x000..+0x5393
    };

    // @ 0x82508388 -- construct the freeburn-main-HUD state. The X360 writes the object's
    // vtable + every embedded HUD sub-component vtable; the observable post-state is a
    // default-constructed state, reproduced here by zeroing the reserved body.
    FBurnMainHudState::FBurnMainHudState()
    {
        for (u32 lu = 0; lu < sizeof(maReserved); ++lu) maReserved[lu] = 0;
    }

    // @ 0x825084D0 -- hands the freeburn main-HUD state's static resource list to the
    // loader:
    //     *result = &maResourcesToLoad;   // out: pointer to the resource-tuple table
    //     *a2     = muNumResourcesToLoad; // out: number of resources
    // The table and its count live in .rdata; the IDA export carries no values for them,
    // so they are declared here (original addresses noted) and resolved at link time.
    void FBurnMainHudState::GetResourcesToLoad(const ResourceTuple** lppResourceTuples,
                                               u32* lpuNumberOfResources) const
    {
        *lppResourceTuples    = maResourcesToLoad;
        *lpuNumberOfResources = muNumResourcesToLoad;
    }
}
