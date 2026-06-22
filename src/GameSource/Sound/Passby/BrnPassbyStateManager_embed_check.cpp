// Embed check for GameSource/Sound/Passby/BrnPassbyStateManager.h
// Exercises the single ledger function bodied by this TU:
//   BrnSound::Logic::Passby::PassbyStateManager::PostPassby  @ 0x82683488
//
// PassbyStateManager embeds a CgsSound::Playback::Content (no default ctor, has
// reference members), so we never default-construct one here -- the bodied
// PostPassby is exercised through a caller-provided reference, matching the X360
// call sites (TrafficControl / EnclosureControl / PhysicsControl / StaticPassbyControl).

#include "GameSource/Sound/Passby/BrnPassbyStateManager.h"

using namespace BrnSound::Logic::Passby;

// Passby payload shape parity: the X360 copy moves the whole record.
static_assert( static_cast<int>( AttribSys::Enums::ePassbyTypes::Tunnel ) == 9, "ePassbyTypes parity" );
static_assert( static_cast<int>( AttribSys::Enums::ePassbyTypes::MaxPassbyTypes ) == 19, "ePassbyTypes tail" );

// PostPassby returns bool.
static_assert( sizeof( decltype( ( (PassbyStateManager*)nullptr )->PostPassby(
                   *(const PassbyStateManager::Passby*)nullptr ) ) ) == sizeof( bool ),
               "PostPassby returns bool" );

// Build a Passby value by member assignment (the two real ctors are bodied in the
// .cpp; here we only need a valid value to feed the bodied PostPassby).
bool brn_passby_post_check( PassbyStateManager& rManager,
                            const CgsSound::Logic::Cgs3dEffectControl* lp3dControl )
{
    PassbyStateManager::Passby lPassby;
    lPassby.mp3dControl                 = lp3dControl;
    lPassby.mfRelativeVelocityMagnitude = 12.5f;
    lPassby.meType                      = AttribSys::Enums::ePassbyTypes::TrafficMedium;
    lPassby.mfVolumeModifier            = 1.0f;
    lPassby.mbSuppressBoostBys          = false;

    // Drive the bodied function (append + count advance, or reject when full).
    return rManager.PostPassby( lPassby );
}
