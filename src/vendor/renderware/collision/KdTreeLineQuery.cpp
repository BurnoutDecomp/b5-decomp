#include "vendor/renderware/collision/KdTreeLineQuery.hpp"

namespace rw
{
namespace collision
{

// The original outer constructor inlines the SDK traversal base.
__forceinline KdTreeLineQueryBase::KdTreeLineQueryBase(const KdTree* lpKdTree, math::vpu::Vector3 lStart,
                                               math::vpu::Vector3 lEnd, f32 lfFatness)
    : m_kdtree(lpKdTree)
    , m_lineClipper(lStart, lEnd, math::vpu::Vector3(lfFatness, lfFatness, lfFatness), lpKdTree->mBBox)
    , m_leafCount(0)
    , m_nextEntry(0)
{
    m_stack[0].m_pa = 0.0f;
    m_stack[0].m_pb = 1.0f;
    if (!m_lineClipper.ClipToAABBox(m_stack[0].m_pa, m_stack[0].m_pb, m_kdtree->mBBox))
    {
        m_top = 0;
    }
    else if (lpKdTree->muNumBranchNodes > 0)
    {
        m_stack[0].m_nodeRef.muContent = 0xFFFFFFFFu;
        m_stack[0].m_nodeRef.muIndex = 0;
        m_top = 1;
    }
    else
    {
        m_leafCount = lpKdTree->muNumEntries;
        m_nextEntry = 0;
        m_top = 0;
    }
}

KdTreeLineQuery::KdTreeLineQuery(const KdTree* lpKdTree, math::vpu::Vector3 lStart,
                               math::vpu::Vector3 lEnd, f32 lfFatness)
    : KdTreeLineQueryBase(lpKdTree, lStart, lEnd, lfFatness)
{
}

void KdTreeLineQuery::ClipEnd(f32 lfFarParam)
{
    // ARTIST skips only keys strictly greater than the new endpoint.
    u32 luIndex = 0;
    u32 luSurvivors = 0;
    for (; luIndex < m_top; ++luIndex)
    {
        if (!(m_stack[luIndex].m_pa > lfFarParam))
        {
            m_stack[luSurvivors] = m_stack[luIndex];
            f32& lrfEnd = m_stack[luSurvivors].m_pb;
            lrfEnd = static_cast<f32>(__fsel(lrfEnd - lfFarParam, lfFarParam, lrfEnd));
            ++luSurvivors;
        }
    }
    m_top = luSurvivors;
}

} // namespace collision
} // namespace rw
