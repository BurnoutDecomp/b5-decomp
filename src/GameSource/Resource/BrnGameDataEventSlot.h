#ifndef GAMESOURCE_RESOURCE_BRNGAMEDATAEVENTSLOT_H
#define GAMESOURCE_RESOURCE_BRNGAMEDATAEVENTSLOT_H

#include "types.hpp"

// ============================================================================
// GameSource/Resource/BrnGameDataEventSlot.h
//
// BrnResource::GameDataEventSlot -- one entry in the GameDataModule's event-slot
// IndexedPool (the pool GameDataModule::GetGameDataEventSlot and the many
// ProcessInternal*Response handlers index into). Each slot tracks an in-flight
// game-data resource request/response.
//
// MINIMAL SLICE / FLAG: the only X360 fact this batch fixes is the element STRIDE.
// The pool accessor CgsContainers::IndexedPool<GameDataEventSlot, N>::operator[]
// @ 0x82663010 returns `48 * index + base`, proving sizeof(GameDataEventSlot) == 0x30
// (48). The per-field layout is reconstructed when GameDataModule's request/response
// handlers land; for now the slot is an honest 48-byte opaque record so the pool
// instantiation has the correct element width. Do NOT treat the field breakdown as an
// X360 fact -- only the 0x30 size is attested here. GROW this header (replace the
// opaque storage with named fields) when those handler TUs are reconstructed.
// ============================================================================

namespace BrnResource
{

struct GameDataEventSlot
{
    // 48-byte (0x30) opaque slot payload. Only the size is X360-attested (the pool
    // operator[] element stride); the internal fields are deferred.
    u8 maOpaque[0x30];
};

} // namespace BrnResource

#endif // GAMESOURCE_RESOURCE_BRNGAMEDATAEVENTSLOT_H
