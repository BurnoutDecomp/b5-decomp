#ifndef BRN_SOUND_LOGIC_COLLISION_BRN_HINGE_STATE_CACHE_H
#define BRN_SOUND_LOGIC_COLLISION_BRN_HINGE_STATE_CACHE_H

#include "types.hpp"

// =============================================================================
// BrnSound::Logic::Collision::HingeStateCache
//   GameSource/Sound/Collision/BrnCollisionStateManager.h (DWARF home).
//   Minimal OWNING slice materialised here as BrnHingeStateCache.h/.cpp because
//   no committed BrnCollisionStateManager home exists yet and this leaf TU needs
//   only HingeStateCache + its CacheNode.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// HingeStateCache is a fixed 32-entry cache of recently-seen jointed-part (hinge)
// state events; CollisionStateManager::UpdateHingingBodyParts ticks it each frame
// via HingeStateCache::Update. DWARF (BrnCollisionStateManager.h:165):
//   struct HingeStateCache {
//       static const uint32_t KU_CACHE_SIZE = 32;   // h:166
//       CacheNode maEvents[32];                      // h:228
//       void Update(float32_t);                      // h:181
//       CacheNode* FindInCache(const CacheNode::JointedPartStateEvent&); // h:197
//       CacheNode* Insert(const CacheNode::JointedPartStateEvent&);      // h:214
//   };
// and CacheNode (BrnCollisionStateManager.h:169):
//   struct CacheNode {
//       JointedPartStateEvent mEvent;   // h:172 (BrnPhysics::Deformation type)
//       bool                  mbValid;  // h:173
//       float32_t             mfTimeLastSeen; // h:174
//       bool                  mbHingeOpen;    // h:175
//       bool                  mbHingeClose;   // h:176
//       CacheNode();                          // h:170
//   };
//
// This TU's recon'd function set is exactly ONE entry:
//   HingeStateCache::Update  @ 0x826830D8
// The X360 body (0x826830D8) walks all 32 nodes (4 nodes unrolled x 8 loop
// iterations, 28-byte node stride on the 4-byte-pointer ABI) and, for each node
// whose validity flag is currently set, KEEPS it set only while
// (currentTime - timeLastSeen) < 0.1; otherwise clears it:
//   do { if (node.valid) node.valid = (a2 - node.timeLastSeen) < 0.1f; ... } x32
// i.e. it expires cache entries not seen within the last 0.1 seconds. The `a2`
// argument is the current sound-logic time (DWARF: Update(float32_t); the X360
// pseudocode widens it to a double FP register, but the declared parameter is f32).
//
// FLAG (un-homed dependency, NAME-only placeholder): CacheNode::mEvent is
// BrnPhysics::Deformation::JointedPartStateEvent, whose full layout lives in the
// (un-homed) physics deformation subsystem (DWARF
// GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h). Update
// NEVER touches mEvent (only mbValid + mfTimeLastSeen are load-bearing), so it is
// modelled here as a NAME-only opaque placeholder of UNVERIFIED size so CacheNode
// stays a complete, instantiable type. Replace with the real JointedPartStateEvent
// when that physics home lands. FindInCache/Insert (which DO read mEvent) are
// DEFERRED to their own recon slice.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 walk uses a 28-byte node
// stride and a +16 base skew into the containing object; Update is bodied as a
// BY-NAME loop over maEvents[i] touching mbValid / mfTimeLastSeen, NOT a raw
// offset walk, and no absolute offsets are asserted across the 32/64 boundary.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

// BrnCollisionStateManager.h:165 (DWARF). Fixed 32-entry hinge-state cache.
struct HingeStateCache
{
    // BrnCollisionStateManager.h:166 (DWARF).
    static const u32 KU_CACHE_SIZE = 32;

    // BrnCollisionStateManager.h:169 (DWARF). One cached jointed-part state event.
    struct CacheNode
    {
        // BrnCollisionStateManager.h:172 (DWARF):
        //   BrnPhysics::Deformation::JointedPartStateEvent mEvent;
        // FLAG: NAME-only opaque placeholder for the un-homed physics deformation
        // event type. Update never touches it. Size is UNVERIFIED (not an X360
        // fact). Replace with the real JointedPartStateEvent when its home lands.
        struct JointedPartStateEvent { u8 mOpaque[1]; };

        JointedPartStateEvent mEvent;         // h:172
        bool                  mbValid;        // h:173
        f32                   mfTimeLastSeen; // h:174
        bool                  mbHingeOpen;    // h:175
        bool                  mbHingeClose;   // h:176

        // BrnCollisionStateManager.h:170 (DWARF).
        CacheNode()
            : mbValid(false)
            , mfTimeLastSeen(0.0f)
            , mbHingeOpen(false)
            , mbHingeClose(false)
        {
        }
    };

    // BrnCollisionStateManager.h:181 (DWARF). @ 0x826830D8 — bodied in this TU.
    void Update(f32 lfTime);

    // h:197 / h:214 (DWARF) — declared for home completeness; DEFERRED (they read
    // CacheNode::mEvent, which is the NAME-only placeholder above).
    CacheNode* FindInCache(const CacheNode::JointedPartStateEvent& lrEvent);
    CacheNode* Insert(const CacheNode::JointedPartStateEvent& lrEvent);

    // BrnCollisionStateManager.h:228 (DWARF).
    CacheNode maEvents[KU_CACHE_SIZE];
};

} // namespace Collision
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_COLLISION_BRN_HINGE_STATE_CACHE_H
