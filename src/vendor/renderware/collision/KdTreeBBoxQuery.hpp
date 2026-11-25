#pragma once

#include "types.hpp"
#include "vendor/renderware/collision/AABBox.hpp"

namespace rw
{
namespace collision
{

// DecFIGS kdtree.h:174-177. ARTIST reads the two counts and the in-place bounds.
struct KdTree
{
    struct NodeRef
    {
        u32 muContent;    // +0x00  m_content
        u32 muIndex;      // +0x04  m_index
    };

    struct BranchNode
    {
        u32     muParent;        // +0x00  m_parent
        u32     muAxis;          // +0x04  m_axis (0 x, 1 y, 2 z)
        NodeRef maChildRefs[2];  // +0x08  m_childRefs
        f32     mafExtents[2];   // +0x18  m_extents
    };

    BranchNode* mpBranchNodes;
    u32 muNumBranchNodes;
    u32 muNumEntries;
    AABBox mBBox;
};

CGS_STATIC_ASSERT((offsetof(KdTree,mpBranchNodes)==0x00), "KDTree branches");
CGS_STATIC_ASSERT((offsetof(KdTree,muNumBranchNodes)==0x04), "KDTree branch count");
CGS_STATIC_ASSERT((offsetof(KdTree,muNumEntries)==0x08), "KDTree entries");
CGS_STATIC_ASSERT((offsetof(KdTree,mBBox)==0x10), "KDTree box");
CGS_STATIC_ASSERT((sizeof(KdTree)==0x30), "KDTree size");

// DecFIGS kdtreebboxquerybase.h:29-44 and kdtreebboxquery.h:47-55.
struct KdTreeBBoxQueryBase
{
    KdTreeBBoxQueryBase(const KdTree* lpKdTree, const AABBox& lrBox);

    const KdTree* m_kdtree;
    AABBox m_bbox;
    u32 m_stack[32];
    u32 m_top;
    u32 m_resultCount;
    u32 m_nextEntry;
};

struct KdTreeBBoxQuery : public KdTreeBBoxQueryBase
{
    KdTreeBBoxQuery(const KdTree* lpKdTree, const AABBox& lrBox);
};

CGS_STATIC_ASSERT((sizeof(KdTreeBBoxQuery) == 0xC0), "KDTree box query size");
CGS_STATIC_ASSERT((offsetof(KdTreeBBoxQueryBase, m_bbox) == 0x10), "KDTree query box");
CGS_STATIC_ASSERT((offsetof(KdTreeBBoxQueryBase, m_stack) == 0x30), "KDTree box query stack");
CGS_STATIC_ASSERT((offsetof(KdTreeBBoxQueryBase, m_top) == 0xB0), "KDTree box query top");
CGS_STATIC_ASSERT((offsetof(KdTreeBBoxQueryBase, m_resultCount) == 0xB4), "KDTree box query result count");
CGS_STATIC_ASSERT((offsetof(KdTreeBBoxQueryBase, m_nextEntry) == 0xB8), "KDTree box query next entry");

} // namespace collision
} // namespace rw
