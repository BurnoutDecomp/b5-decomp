#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"

#include <cstddef>   // offsetof, size_t

// Sanctioned gate driver for the CgsModule::EventReceiverQueue<N, 16> family (the
// templated event-receiver queue from the X360 CgsEventReceiverQueue.h -- re-homed to
// CgsBaseEventReceiverQueue.h, where BaseEventReceiverQueue's bodies live out-of-line).
//
// The derived EventReceiverQueue<BUFSIZE, ALIGN> is a thin inline wrapper: it adds the
// embedded backing buffer maBuffer[BUFSIZE] and a single inline Construct() that binds it
// into the base. All queue behaviour (Clear/AddEvent/GetFirstEvent/GetNextEvent, @0x821F1D50
// /0x82663928/0x82204690) is the non-template base, already committed in
// CgsBaseEventReceiverQueue.cpp. This TU exists only to:
//   (1) force-instantiate every <N, 16> the in-tree consumers use, proving the generic
//       Construct() compiles for each, and
//   (2) pin the layout the X360 inline bodies assume.
//
// Consumer instantiations (grep of b5-decomp/src):
//   <512,16>   CgsLiveUpdate / ResourceBundleLoader / ResourcePool / WorldMap (TriggerData)
//   <1024,16>  CgsModule::GuiEventReceiverQueue (CgsGuiShared.h typedef) + GuiResourceModule
//   <2048,16>  BrnGameData receiver
//   <4096,16>  BrnAI::AIModule map-data receiver
//   <5120,16>  scene add-entity receiver
//   <8192,16>  larger receiver consumer
//   <16384,16> largest receiver consumer
//
// Layout (LLP64 host): BaseEventReceiverQueue = u8* mpBuffer(8) + 5*s32(20) -> padded to 32
// (8-byte struct alignment from the pointer member). maBuffer[N] (u8, align 1) is appended;
// every consumer N is a multiple of 16 (so already 8-aligned) -> sizeof == 32 + N exactly.
namespace
{
    // Pin the base size: u8* mpBuffer (8) then the five s32 fields (write/count/start/cap/align,
    // [1]..[5]) contiguously -> 28 bytes, padded to 32 by the 8-byte pointer's struct alignment.
    static_assert(sizeof(CgsModule::BaseEventReceiverQueue) == 32,
                  "BaseEventReceiverQueue must pad to 32 (u8* + 5*s32, 8-aligned)");

    template <s32 BUFSIZE>
    struct ErqLayoutCheck
    {
        static_assert(sizeof(CgsModule::EventReceiverQueue<BUFSIZE, 16>) == (size_t)(BUFSIZE + 32),
                      "EventReceiverQueue<N,16> sizeof must be N + 32 (32-byte base + char[N])");
    };

    // Force instantiation of every <N, 16> the consumers use; each pulls in the generic
    // Construct() and the layout check above.
    template struct ErqLayoutCheck<512>;
    template struct ErqLayoutCheck<1024>;
    template struct ErqLayoutCheck<2048>;
    template struct ErqLayoutCheck<4096>;
    template struct ErqLayoutCheck<5120>;
    template struct ErqLayoutCheck<8192>;
    template struct ErqLayoutCheck<16384>;
}

// Explicit class instantiation of the representative typedef'd queue (GuiEventReceiverQueue =
// EventReceiverQueue<1024,16>) so its inline Construct() is emitted out-of-line at least once.
template class CgsModule::EventReceiverQueue<1024, 16>;
