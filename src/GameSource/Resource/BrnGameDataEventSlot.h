#ifndef GAMESOURCE_RESOURCE_BRNGAMEDATAEVENTSLOT_H
#define GAMESOURCE_RESOURCE_BRNGAMEDATAEVENTSLOT_H

#include "types.hpp"
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"   // GameDataIO::GameDataAssetEvent (captured request)

// ============================================================================
// GameSource/Resource/BrnGameDataEventSlot.h
//
// BrnResource::GameDataEventSlot -- one entry in the GameDataModule's event-slot
// IndexedPool (the pool GameDataModule::GetGameDataEventSlot and the many
// ProcessInternal*Response handlers index into). Each slot tracks an in-flight
// game-data resource request/response.
//
// GROWN (this batch): the per-field layout is now X360-attested by
// GameDataModule::GetGameDataEventSlot @ 0x826664A0 (the store sequence into a
// fresh/reused slot) plus the request/response dispatchers:
//   +0x00  meStage            -- Process{Load,Get,Unload}GameDataEvent store 0/1/2
//                                (E_LOADING/E_AQUIRING/E_UNLOADING) right after the
//                                slot fetch; DWARF names the enum (BrnGameDataModule.h:112)
//   +0x08  mEvent             -- a captured GameDataAssetEvent image of the request:
//                                GetGameDataEventSlot copies miEventId(+8), the
//                                receiver-queue pointer(+12), miPoolId(+16), mId(+24)
//                                and meType(+32) from the incoming event and clears
//                                the fail flag(+36). The response handlers pass
//                                `slot+8` straight back into the ProcessXxxRequest
//                                handlers as the event pointer (e.g.
//                                ProcessInternalLoadBundleResponse @ 0x82672630
//                                `ProcessGetVehicleRequest(a1, a2, slot + 2, 50, ...)`),
//                                proving the +8 block IS a GameDataAssetEvent.
//   +0x28  miResponseEventId  -- the response event id the servicing handler will
//                                post back to the requester (each ProcessXxxRequest
//                                stores its dispatch id here: `slot->+40 = a4`);
//                                GetGameDataEventSlot seeds 69 on a fresh allocation.
// The X360 element stride is 0x30 (48) -- IndexedPool<GameDataEventSlot,short>::
// operator[] @ 0x82663010 returns `48 * index + base`. On the x64 host the struct
// is wider (pointer members + the x64-grown GameDataAssetEvent); semantic parity
// is by named members per the x64 gate, the stride follows sizeof(T) generically.
// ============================================================================

namespace BrnResource
{

struct GameDataEventSlot
{
    // DWARF BrnGameDataModule.h:112 -- the slot's service stage. (The DWARF spelling
    // "E_AQUIRING" [sic] is kept verbatim.)
    enum EEventStage
    {
        E_LOADING   = 0,
        E_AQUIRING  = 1,
        E_UNLOADING = 2,
        E_DONE      = 3
    };

    EEventStage                       meStage;            // +0x00 (X360)
    GameDataIO::GameDataAssetEvent    mEvent;             // +0x08 (X360) captured request event
    s32                               miResponseEventId;  // +0x28 (X360) response id (69 = fresh sentinel)
};

} // namespace BrnResource

#endif // GAMESOURCE_RESOURCE_BRNGAMEDATAEVENTSLOT_H
