#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsHash.h"                  // CgsContainers::CgsHash::CalculateHash

// Reconstructed from BURNOUT_X360_ARTIST.XEX. Self-contained per-class reconstruction
// (the race main-HUD GUI state). Functions recovered here:
//   RaceMainHudState::RaceMainHudState()          @ 0x82508110  (ctor)
//   RaceMainHudState::SetExpectedComponent(const char*) @ 0x82473698
//   RaceMainHudState::GetResourcesToLoad(...)     @ 0x82508368
//
// The object is large (last ctor store at +0x790C -> size 0x7910). The ctor stores the
// most-derived vtable at +0x000 then ~90 embedded sub-component vtables/fields at fixed
// offsets across the body. Those stores are the compiler's own initialisation of the
// state's embedded sub-objects (HUD components), whose individual types are not
// recoverable from this ctor alone; the recovered observable effect is "construct the
// race-main-HUD state with its components default-initialised". The reserved body below
// carries the recovered named fields (the expected-APT-component hash table and its
// count) at their recovered offsets; everything else is layout-recovery storage.
// All access is by name.

namespace BrnGui
{
    struct RaceMainHudState
    {
        struct ResourceTuple;                            // opaque .rdata record
        static const ResourceTuple maResourcesToLoad[];  // @ 0x82F25F88 (unk_82F25F88)
        static const u32           muNumResourcesToLoad; // @ 0x82F25F84 (dword_82F25F84)

        static const u32 KU_MAX_EXPECTED_COMPONENTS = 64;   // SetExpectedComponent bound (0x40)

        RaceMainHudState();                                 // @ 0x82508110
        u32  SetExpectedComponent(const char* lpcName);     // @ 0x82473698
        void GetResourcesToLoad(const ResourceTuple** lppResourceTuples,
                                u32* lpuNumberOfResources) const;

        // --- recovered layout (guest 32-bit offsets) -------------------------------------
        u8  maHeadReserved[0x3C];                           // +0x000..+0x03B (vtable + base + early components)
        u32 maExpectedComponents[KU_MAX_EXPECTED_COMPONENTS]; // +0x03C (this[15 + i]) APT-component name hashes
        u32 muExpectedComponentCount;                       // +0x13C
        u8  maBodyReserved[0x7910 - 0x140];                 // +0x140..+0x790F (embedded HUD sub-objects)
    };

    // @ 0x82508110 -- construct the race-main-HUD state. The X360 writes the object's
    // vtable + every embedded HUD sub-component vtable; the observable post-state is a
    // default-constructed state, reproduced here by zeroing the reserved body. The
    // expected-component table starts empty.
    RaceMainHudState::RaceMainHudState()
    {
        for (u32 lu = 0; lu < sizeof(maHeadReserved); ++lu) maHeadReserved[lu] = 0;
        for (u32 li = 0; li < KU_MAX_EXPECTED_COMPONENTS; ++li) maExpectedComponents[li] = 0;
        muExpectedComponentCount = 0;
        for (u32 lu = 0; lu < sizeof(maBodyReserved); ++lu) maBodyReserved[lu] = 0;
    }

    // @ 0x82473698 -- append the hash of an expected APT-component name to the table.
    // Asserts there is room (count < 0x40; BrnRaceMainHudState.h:634), hashes the
    // NUL-terminated name (length = strlen), stores at this[15 + count] (byte +0x3C +
    // count*4 == maExpectedComponents[count]), and increments the count.
    u32 RaceMainHudState::SetExpectedComponent(const char* lpcName)
    {
        CGS_ASSERT(muExpectedComponentCount < KU_MAX_EXPECTED_COMPONENTS,
                   "No space for new expected component");

        const char* lpc = lpcName;
        while (*lpc) ++lpc;
        u32 luHash = CgsContainers::CgsHash::CalculateHash(
            const_cast<char*>(lpcName), static_cast<int>(lpc - lpcName));   // length excludes the NUL

        maExpectedComponents[muExpectedComponentCount] = luHash;
        ++muExpectedComponentCount;
        return luHash;
    }

    void RaceMainHudState::GetResourcesToLoad(const ResourceTuple** lppResourceTuples,
                                              u32* lpuNumberOfResources) const
    {
        *lppResourceTuples    = maResourcesToLoad;
        *lpuNumberOfResources = muNumResourcesToLoad;
    }
}
