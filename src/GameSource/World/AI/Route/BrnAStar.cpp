#include "GameSource/World/AI/Route/BrnAStar.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnAI::AStarNodePool -- the .cpp home for the pool members declared in BrnAStar.h.
// Only GetNode (@0x82765530) is bodied here; the remaining pool members
// (Construct/NewNode/ExtractBestOpenNode/FindNode/GetNodeCount) live in their own TUs.

namespace BrnAI
{
// BrnAI::AStarNodePool::GetNode @0x82765530.
//
// Returns a pointer to the luNodeIndex'th node in the flat maNodes[] array. The pool
// partitions its 1024-node capacity into KU_PARTITION_COUNT buckets of
// KU_MAX_PARTITION_NODES (256) nodes each; mauNodeCount[partition] tracks how many
// nodes are live in each bucket. The X360 build asserts:
//   - luNodeIndex < KU_MAX_NODES                                    (BrnAStar.h:483)
//   - luNodeIndex % KU_MAX_PARTITION_NODES
//        < mauNodeCount[luNodeIndex / KU_MAX_PARTITION_NODES]       (BrnAStar.h:484)
// then returns &maNodes[luNodeIndex] (asm: r3 = 24 * luNodeIndex + this, i.e. the
// 24-byte AStarNode stride). The baked d:\p4 file/line are dropped in favour of
// __FILE__/__LINE__ by CGS_ASSERT. Called by ExtractBestOpenNode and FindNode.
AStarNode* AStarNodePool::GetNode(u16 luNodeIndex)
{
    CGS_ASSERT(luNodeIndex < KU_MAX_NODES, "luIndex < KU_MAX_NODES");
    CGS_ASSERT(luNodeIndex % KU_MAX_PARTITION_NODES
                   < mauNodeCount[luNodeIndex / KU_MAX_PARTITION_NODES],
               "luIndex % KU_MAX_PARTITION_NODES < mauNodeCount[luIndex / KU_MAX_PARTITION_NODES]");
    return &maNodes[luNodeIndex];
}
}
