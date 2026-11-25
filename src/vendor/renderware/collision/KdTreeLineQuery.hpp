#pragma once

#include "vendor/renderware/collision/AALineClipper.hpp"
#include "vendor/renderware/collision/KdTreeBBoxQuery.hpp"

namespace rw
{
namespace collision
{

// DecFIGS kdtreelinequerybase.h:46-76 and kdtreelinequery.h:87-100.
struct KdTreeLineQueryBase
{
    // ARTIST 82BB0430-82BB0444 copies each element as two aligned words.
    struct CGS_ALIGN(8) StackElement
    {
        KdTree::NodeRef m_nodeRef;
        f32 m_pa;
        f32 m_pb;
    };

    KdTreeLineQueryBase(const KdTree* lpKdTree, math::vpu::Vector3 lStart,
                       math::vpu::Vector3 lEnd, f32 lfFatness);

    const KdTree* m_kdtree;
    AALineClipper m_lineClipper;
    StackElement m_stack[32];
    u32 m_top;
    u32 m_leafCount;
    u32 m_nextEntry;
};

struct KdTreeLineQuery : public KdTreeLineQueryBase
{
    KdTreeLineQuery(const KdTree* lpKdTree, math::vpu::Vector3 lStart,
                   math::vpu::Vector3 lEnd, f32 lfFatness);
    void ClipEnd(f32 lfFarParam);
};

CGS_STATIC_ASSERT((sizeof(KdTreeLineQuery) == 0x260), "KDTree line query size");
CGS_STATIC_ASSERT((sizeof(KdTreeLineQueryBase::StackElement) == 0x10), "KDTree line stack element");
CGS_STATIC_ASSERT((offsetof(KdTreeLineQueryBase, m_lineClipper) == 0x10), "KDTree line clipper");
CGS_STATIC_ASSERT((offsetof(KdTreeLineQueryBase, m_stack) == 0x50), "KDTree line query stack");
CGS_STATIC_ASSERT((offsetof(KdTreeLineQueryBase, m_top) == 0x250), "KDTree line query top");
CGS_STATIC_ASSERT((offsetof(KdTreeLineQueryBase, m_leafCount) == 0x254), "KDTree line query leaf count");
CGS_STATIC_ASSERT((offsetof(KdTreeLineQueryBase, m_nextEntry) == 0x258), "KDTree line query next entry");

} // namespace collision
} // namespace rw
