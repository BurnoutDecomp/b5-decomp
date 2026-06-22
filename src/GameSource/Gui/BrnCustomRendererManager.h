#ifndef BRN_CUSTOM_RENDERER_MANAGER_H
#define BRN_CUSTOM_RENDERER_MANAGER_H

#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//   BrnGui::CustomRendererManager::SetReplaySerialiser  @ 0x82445648
//
// The manager owns the GUI custom-renderer set; the one in-scope function stores a
// replay-serialiser pointer into a member deep in the object:
//
//   0x82445648  lis  r11, 1
//   0x8244564C  ori  r11, r11, 0xBED0        ; r11 = 0x1BED0 (114384)
//   0x82445650  stwx r4, r3, r11             ; *(this + 0x1BED0) = a2
//   0x82445654  blr                          ; returns this (result == r3)
//
// MINIMAL OWNING SLICE: the real manager is ~114 KB of renderer state (the full custom
// renderer array, per-renderer texture/event state, etc. -- all uncommitted). This models
// ONLY the member the ledger function writes: the replay-serialiser pointer (guest
// +0x1BED0). FLAG: minimal-slice class -- every other manager member is intentionally
// OMITTED (uncommitted dependencies, none in scope). The exact guest offset is NOT
// reproduced (host is 64-bit; the offset is not load-bearing here).
//
// The serialiser parameter is an opaque pointer in scope (its type, a replay serialiser,
// is uncommitted): modelled as void* held BY NAME, faithful to the raw stwx store.

namespace BrnGui
{

class CustomRendererManager
{
public:
    // SetReplaySerialiser @ 0x82445648. Stores the serialiser pointer and returns this
    // (the X360 keeps the incoming object pointer in r3 as the result).
    CustomRendererManager* SetReplaySerialiser( void* lpReplaySerialiser );

private:
    // Guest +0x1BED0: the replay serialiser the manager hands to its renderers. Held BY
    // NAME (no raw offset cast). FLAG: only member modelled in this minimal slice.
    void* mpReplaySerialiser;
};

} // namespace BrnGui

#endif // BRN_CUSTOM_RENDERER_MANAGER_H
