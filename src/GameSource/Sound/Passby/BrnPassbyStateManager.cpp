#include "GameSource/Sound/Passby/BrnPassbyStateManager.h"

// =============================================================================
// BrnSound::Logic::Passby::PassbyStateManager::DynamicPropByCache -- out-of-line
// body for the single ledger function owned by this TU:
//   DynamicPropByCache::Update  @ 0x82683360
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The cache holds KU_DYNAMIC_PROP_CACHE_SIZE recent dynamic-prop passbys so the
// same prop is not re-triggered every frame. Update() ages every active entry:
// once an entry has been live for >= 5 seconds it is cleared so the prop becomes
// eligible to trigger again.
//
// SIGNATURE NOTE: the committed declaration is `void Update(f32 lfTimeStep)`, but
// the X360 asm at 0x82683360 (and its sole call site inside UpdateDynamicPropBys
// @ 0x826A0E48..0x826A0E58, which loads `lfs f28, 4(rThis)` -- the manager's
// running game clock -- and passes it as the argument) shows the argument is the
// CURRENT game time, NOT a per-frame delta: each entry's expiry test is
// `(currentTime - entry.mfTimeStamp) >= 5.0`. The parameter is therefore named
// lfCurrentTime here. FLAG: the committed header parameter name `lfTimeStep` is
// misleading; rename it to `lfCurrentTime` when that home is next grown (the type
// f32 already matches the single-precision load at the call site).
//
// LAYOUT (recovered from the asm): maItems is processed at a 12-byte stride
// (Item = bool mbActive @+0 padded to 4, f32 mfTimeStamp @+4, EntityId mId @+8),
// four items per unrolled loop iteration over 8 iterations == 32 entries. Bodied
// by NAME (range loop over maItems) -- no absolute offsets are asserted.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Passby
{

// Expiry window: an active cache entry is cleared once it has been live for at
// least this many seconds. Recovered from the X360 rodata literal compared by
// every fcmpu in the loop (flt_820ABCD8 == 5.0f).
static const f32 KF_PROP_BY_CACHE_LIFETIME = 5.0f;

// ---------------------------------------------------------------------------
// DynamicPropByCache::Update  @ 0x82683360
//   For every entry: if it is active and has aged past KF_PROP_BY_CACHE_LIFETIME
//   relative to the supplied current time, deactivate it. Inactive entries and
//   still-fresh entries are left as-is.
//
//   Boolean parity with the asm: mbActive becomes
//     mbActive && ( (lfCurrentTime - mfTimeStamp) < KF_PROP_BY_CACHE_LIFETIME )
// ---------------------------------------------------------------------------
void PassbyStateManager::DynamicPropByCache::Update( f32 lfCurrentTime )
{
    for( u32 luIndex = 0; luIndex < PassbyStateManager::KU_DYNAMIC_PROP_CACHE_SIZE; ++luIndex )
    {
        Item& lrItem = maItems[ luIndex ];

        if( lrItem.mbActive
            && ( ( lfCurrentTime - lrItem.mfTimeStamp ) >= KF_PROP_BY_CACHE_LIFETIME ) )
        {
            lrItem.mbActive = false;
        }
    }
}

} // namespace Passby
} // namespace Logic
} // namespace BrnSound
