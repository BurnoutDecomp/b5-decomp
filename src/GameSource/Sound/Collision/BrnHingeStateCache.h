#ifndef BRN_SOUND_LOGIC_COLLISION_BRN_HINGE_STATE_CACHE_H
#define BRN_SOUND_LOGIC_COLLISION_BRN_HINGE_STATE_CACHE_H

#include "types.hpp"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"   // JointedPartStateEvent (CacheNode::mEvent)

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
// This TU's recon'd function set: Update @ 0x826830D8, Insert @ 0x826831A8, and FindInCache
// (inlined by its only caller, CollisionStateManager::UpdateHingingBodyParts @0x826D44D0 -- the
// collision manager's mHingeCache, DWARF h:791, X360 +0x12F0).
// The X360 Update body (0x826830D8) walks all 32 nodes (4 nodes unrolled x 8 loop
// iterations, 28-byte node stride on the 4-byte-pointer ABI) and, for each node
// whose validity flag is currently set, KEEPS it set only while
// (currentTime - timeLastSeen) < 0.1; otherwise clears it:
//   do { if (node.valid) node.valid = (a2 - node.timeLastSeen) < 0.1f; ... } x32
// i.e. it expires cache entries not seen within the last 0.1 seconds. The `a2`
// argument is the current sound-logic time (DWARF: Update(float32_t); the X360
// pseudocode widens it to a double FP register, but the declared parameter is f32).
//
// CacheNode::mEvent is the real BrnPhysics::Deformation::JointedPartStateEvent
// (BrnDeformationEvents.h): +0x00 mVehicleId, +0x04 meType, +0x08 mfCurrentOrientation,
// +0x0C mfHingeVelocity; then mbValid +0x10, mfTimeLastSeen +0x14, mbHingeOpen +0x18,
// mbHingeClose +0x19 (28-byte node, Insert / UpdateHingingBodyParts stores).
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
        // DWARF BrnSoundLogicSharedIO.h:68 -- the deformation output's hinge event (vehicle id,
        // part type, orientation, hinge velocity: 16 bytes, the node's +0x00..+0x0F that Insert
        // and UpdateHingingBodyParts copy word for word).
        typedef BrnPhysics::Deformation::JointedPartStateEvent JointedPartStateEvent;

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

    // h:197 (DWARF). The node already holding this part's state: the first VALID node with the
    // same part type (`cmpw`) and vehicle (`cmplw`), else null. The console inlines it
    // (UpdateHingingBodyParts 0x826D4614..0x826D4654); bodied in this TU.
    CacheNode* FindInCache(const CacheNode::JointedPartStateEvent& lrEvent);

    // h:214 (DWARF), ARTIST 0x826831A8. The first free node takes the event (valid, mEvent =
    // the event's four words); null when all 32 are in use. Bodied in this TU.
    CacheNode* Insert(const CacheNode::JointedPartStateEvent& lrEvent);

    // BrnCollisionStateManager.h:228 (DWARF).
    CacheNode maEvents[KU_CACHE_SIZE];
};

} // namespace Collision
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_COLLISION_BRN_HINGE_STATE_CACHE_H
