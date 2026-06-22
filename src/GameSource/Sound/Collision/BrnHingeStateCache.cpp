#include "GameSource/Sound/Collision/BrnHingeStateCache.h"

// =============================================================================
// BrnSound::Logic::Collision::HingeStateCache — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnHingeStateCache.h for the
// CacheNode layout, the JointedPartStateEvent placeholder FLAG, and the
// X360-32-bit-vs-host-64-bit offset note.
//
// This TU's recon'd function set is exactly ONE entry:
//   HingeStateCache::Update  @ 0x826830D8
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

// ---------------------------------------------------------------------------
// HingeStateCache::Update(f32 lfTime)  @ 0x826830D8
//
//   v2 = this + 16;  v3 = 8;            ; node cursor + loop count (4 nodes/iter)
//   do {
//     if (*v2)        *v2      = (lfTime - *(v2+4))  < 0.1;  ; node[k+0]
//     if (*(v2+28))   *(v2+28) = (lfTime - *(v2+32)) < 0.1;  ; node[k+1]
//     if (*(v2+56))   *(v2+56) = (lfTime - *(v2+60)) < 0.1;  ; node[k+2]
//     if (*(v2+84))   *(v2+84) = (lfTime - *(v2+88)) < 0.1;  ; node[k+3]
//     v2 += 112; --v3;
//   } while (v3);                       ; 4 nodes x 8 iters = 32 = KU_CACHE_SIZE
//
// For every node whose validity flag is currently set, the flag stays set only
// while the node was seen within the last 0.1 seconds; otherwise it is cleared.
// This expires stale hinge-state cache entries each frame. Bodied BY NAME over
// maEvents[i] (touching mbValid / mfTimeLastSeen), not as a raw offset walk; the
// 4-nodes-per-iteration unroll is a pure X360 codegen detail and collapses to the
// flat 32-entry loop below.
// ---------------------------------------------------------------------------
void HingeStateCache::Update(f32 lfTime)
{
    for (u32 luIndex = 0; luIndex < KU_CACHE_SIZE; ++luIndex)
    {
        CacheNode& lrNode = maEvents[luIndex];
        if (lrNode.mbValid)
        {
            lrNode.mbValid = (lfTime - lrNode.mfTimeLastSeen) < 0.1f;
        }
    }
}

} // namespace Collision
} // namespace Logic
} // namespace BrnSound
