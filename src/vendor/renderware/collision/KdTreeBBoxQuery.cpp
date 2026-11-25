#include "vendor/renderware/collision/KdTreeBBoxQuery.hpp"

namespace rw
{
namespace collision
{

inline KdTreeBBoxQueryBase::KdTreeBBoxQueryBase(const KdTree* lpKdTree, const AABBox& lrBox)
    : m_kdtree(lpKdTree), m_bbox(lrBox)
{
    m_resultCount = 0;
    m_nextEntry = 0;
    if (lpKdTree->muNumBranchNodes > 0)
    {
        m_stack[0] = 0;
        m_top = 1;
    }
    else
    {
        m_resultCount = lpKdTree->muNumEntries;
        m_nextEntry = 0;
        m_top = 0;
    }
}

KdTreeBBoxQuery::KdTreeBBoxQuery(const KdTree* lpKdTree, const AABBox& lrBox)
    : KdTreeBBoxQueryBase(lpKdTree, lrBox)
{
}

} // namespace collision
} // namespace rw
