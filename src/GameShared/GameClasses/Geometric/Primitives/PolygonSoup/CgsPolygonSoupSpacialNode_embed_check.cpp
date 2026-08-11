#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupSpacialNode.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox4.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupList.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.h"

#include <cstddef>

// ============================================================================
// Layout gate for the spatial-partition types (2026-08-10 spatial-partition wave).
//
// ⭐ MOUNTED FROM DAY ONE. An unmounted embed_check has never run -- the previous
// wave mounted one that had been submit-time-only for weeks and it failed in a
// cascade on the first real build ([[gates-are-stale-not-dead]]). This one goes on
// the link with the code it guards.
//
// ⭐ Each assert below is classified by the DECIDING TEST -- is the struct
// serialised? -- because that is what says whether a console byte offset may be
// pinned at all:
//
//   SERIALISED (FixUp'd / porter-emitted)  -> the byte layout IS the contract,
//                                             pin it. The witness is the porter.
//   CARVED AT RUNTIME                      -> pointer slots widen 4->8, so pin
//                                             only what is platform-independent
//                                             OR independently load-bearing.
// ============================================================================

namespace
{
    using namespace CgsGeometric;

    // ------------------------------------------------------------------------
    // AxisAlignedBox / AxisAlignedBox4 -- SERIALISED, and pointer-free, so every
    // console offset is genuinely pinnable. Witness: world_support_transcode.py
    // keeps BOX4_SIZE = 112 ("7 rows x 4 u32 lanes") on both platforms.
    // ------------------------------------------------------------------------
    static_assert(sizeof(AxisAlignedBox) == 32, "AxisAlignedBox is two 16-byte corners");
    static_assert(offsetof(AxisAlignedBox, mMin) == 0x00, "AxisAlignedBox::mMin @+0x00");
    static_assert(offsetof(AxisAlignedBox, mMax) == 0x10, "AxisAlignedBox::mMax @+0x10");

    static_assert(sizeof(AxisAlignedBox4) == 112, "AxisAlignedBox4 stride 0x70 (caller's mulli 0x70)");
    static_assert(offsetof(AxisAlignedBox4, mafMinX) == 0x00, "AxisAlignedBox4 min X row");
    static_assert(offsetof(AxisAlignedBox4, mafMinY) == 0x10, "AxisAlignedBox4 min Y row");
    static_assert(offsetof(AxisAlignedBox4, mafMinZ) == 0x20, "AxisAlignedBox4 min Z row");
    static_assert(offsetof(AxisAlignedBox4, mafMaxX) == 0x30, "AxisAlignedBox4 max X row");
    static_assert(offsetof(AxisAlignedBox4, mafMaxY) == 0x40, "AxisAlignedBox4 max Y row");
    static_assert(offsetof(AxisAlignedBox4, mafMaxZ) == 0x50, "AxisAlignedBox4 max Z row");
    static_assert(offsetof(AxisAlignedBox4, mafUnread) == 0x60, "AxisAlignedBox4 seventh row");

    // ------------------------------------------------------------------------
    // PolygonSoupList / PolygonSoup -- SERIALISED, and the PC porter emits the
    // WIDENED form, so the pinned numbers are the porter's, not the console's.
    // Witness: world_support_transcode.py PSL_X64_HDR = 0x38, SOUP_X64_HDR = 0x28,
    // '<QQiI' @0x20, '<QQ' @soup+0x10, 'H' @soup+0x20.
    // ⚠️ These four are the ones that would silently mis-read WORLDCOL.BIN if the
    // porter and the struct ever drift apart, which is exactly the failure mode
    // that has no symptom until geometry is subtly in the wrong place.
    // ------------------------------------------------------------------------
    static_assert(sizeof(PolygonSoupList) == 0x38, "PolygonSoupList x64 header is 0x38 (porter PSL_X64_HDR)");
    static_assert(offsetof(PolygonSoupList, mpapPolySoups)    == 0x20, "PolygonSoupList soup table @+0x20");
    static_assert(offsetof(PolygonSoupList, mpaPolySoupBoxes) == 0x28, "PolygonSoupList box array @+0x28");
    static_assert(offsetof(PolygonSoupList, miNumPolySoups)   == 0x30, "PolygonSoupList count @+0x30");
    static_assert(offsetof(PolygonSoupList, miDataSize)       == 0x34, "PolygonSoupList size @+0x34");
    // The soup pointer TABLE is u64 per entry on x64 ('<%dQ'); BuildSpacialPartition
    // indexes it as PolygonSoup* const*, so the host pointer must match the porter.
    static_assert(sizeof(const PolygonSoup*) == 8, "the ported soup table is u64 per entry");

    static_assert(sizeof(PolygonSoup)  == 0x28, "PolygonSoup x64 header is 0x28 (porter SOUP_X64_HDR)");
    static_assert(offsetof(PolygonSoup, mpPolygons)     == 0x10, "PolygonSoup polys ptr @+0x10");
    static_assert(offsetof(PolygonSoup, mpVertices)     == 0x18, "PolygonSoup verts ptr @+0x18");
    static_assert(offsetof(PolygonSoup, mu16SoupSize)   == 0x20, "PolygonSoup size @+0x20 (porter shdr-8)");
    static_assert(offsetof(PolygonSoup, mu8NumPolygons) == 0x22, "PolygonSoup npoly @+0x22 (porter shdr-6)");
    static_assert(offsetof(PolygonSoup, mu8NumQuads)    == 0x23, "PolygonSoup nquad @+0x23 (porter shdr-5)");
    static_assert(offsetof(PolygonSoup, mu8NumVertices) == 0x24, "PolygonSoup nvert @+0x24 (porter shdr-4)");

    // ------------------------------------------------------------------------
    // The two node types -- CARVED AT RUNTIME by BuildSpacialPartition, so their
    // pointer slots widen and most console offsets are NOT pinnable.
    //
    // ⭐ The size, however, IS pinned, and deliberately: it survives the widening at
    // 48 only because Vector4's alignas(16) absorbs the extra four bytes, and
    // PolygonSoupListSpatialMap::GetPolygonSoup @0x8280FFD0 hard-codes a 0x30 element
    // step that is load-bearing on ANY platform. If a member is ever added, the
    // coincidence breaks and GetPolygonSoup starts returning misaligned nodes with no
    // other symptom. That is what this gate is for.
    // ------------------------------------------------------------------------
    static_assert(sizeof(PolygonSoupSpacialNode) == 0x30,
                  "PolygonSoupSpacialNode must stay 48 bytes -- GetPolygonSoup's 0x30 step");
    static_assert(sizeof(PolygonSoupLeafNode) == 0x30,
                  "PolygonSoupLeafNode must stay 48 bytes -- GetPolygonSoup's 0x30 step");
    static_assert(sizeof(PolygonSoupSpacialNode) == sizeof(PolygonSoupLeafNode),
                  "both node kinds share one stride (the console walks both with addi 0x30)");

    // Platform-independent: the box leads the node, so a node is castable to its box
    // and the console's `lvx128 v, r0, rNode` / `lvx128 v, rNode, 0x10` pair works.
    static_assert(offsetof(PolygonSoupSpacialNode, mBox) == 0, "the box leads the parent node");
    static_assert(offsetof(PolygonSoupLeafNode, mBox) == 0, "the box leads the leaf node");
    static_assert(offsetof(PolygonSoupSpacialNode, mpaIndices) == sizeof(AxisAlignedBox),
                  "the parent node's pointer follows the box immediately");
    static_assert(offsetof(PolygonSoupLeafNode, mpPolygonSoup) == sizeof(AxisAlignedBox),
                  "the leaf node's soup pointer follows the box immediately");
    // The count sits immediately after the (host-width) pointer in both.
    static_assert(offsetof(PolygonSoupSpacialNode, mu16NumIndices) ==
                  offsetof(PolygonSoupSpacialNode, mpaIndices) + sizeof(u16*),
                  "the parent node's count follows its pointer");
    static_assert(offsetof(PolygonSoupLeafNode, mu16PolygonSoupSize) ==
                  offsetof(PolygonSoupLeafNode, mpPolygonSoup) + sizeof(const PolygonSoup*),
                  "the leaf node's size follows its pointer");

    // ------------------------------------------------------------------------
    // The child-index block BuildSpacialPartition carves per parent node is a fixed
    // 8 bytes on the console (`li r4, 8` before the Malloc); it must stay four u16
    // on the host or the four index stores run past the allocation.
    // ------------------------------------------------------------------------
    static_assert(4u * sizeof(u16) == 8u, "a parent node's child block is four u16 == the console's 8");
}
