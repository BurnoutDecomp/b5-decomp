// Embed check for BrnSound::Logic::Passby::PassbyStateManager::DynamicPropByCache.
// Exercises the single ledger function bodied by this TU:
//   DynamicPropByCache::Update  @ 0x82683360
//
// The cache embeds an Item[] whose Item() ctor is declared-only in the committed
// home (not owned by this TU), so we never default-construct a cache here -- the
// bodied Update is driven through a caller-provided reference, mirroring the X360
// call site inside PassbyStateManager::UpdateDynamicPropBys.

#include "GameSource/Sound/Passby/BrnPassbyStateManager.h"

using namespace BrnSound::Logic::Passby;

typedef PassbyStateManager::DynamicPropByCache Cache;

// Layout parity sanity: 32 entries of 12 bytes each (bool padded to 4 + f32 + EntityId).
static_assert( PassbyStateManager::KU_DYNAMIC_PROP_CACHE_SIZE == 32,
               "dynamic-prop cache size parity" );
static_assert( sizeof( Cache::Item ) == 12, "Item is 12 bytes (asm 12-byte stride)" );
static_assert( sizeof( Cache ) == 12 * 32, "cache is a dense Item[32]" );

// Drive the bodied Update: age the cache against a current game-time value.
// Returns the number of entries still active afterwards.
u32 brn_passby_dynpropcache_update_check( Cache& rCache, f32 lfCurrentTime )
{
    rCache.Update( lfCurrentTime );

    u32 luActive = 0;
    for( u32 luIndex = 0; luIndex < PassbyStateManager::KU_DYNAMIC_PROP_CACHE_SIZE; ++luIndex )
    {
        if( rCache.maItems[ luIndex ].mbActive )
        {
            ++luActive;
        }
    }
    return luActive;
}
