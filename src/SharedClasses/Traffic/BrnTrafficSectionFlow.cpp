// =============================================================================
// BrnTrafficSectionFlow.cpp -- owning .cpp for BrnTraffic::SectionFlow. A
// two-halfword serialised value type; the only out-of-line content is the
// never-called layout pin. PARK: EndianSwap (DWARF :53) is declared-only -- no
// ARTIST symbol, no rw::EndianSwap in this tree, and the shipped PC payload is
// already little-endian (BrnTrafficStaticTraffic.cpp).
// =============================================================================

#include "SharedClasses/Traffic/BrnTrafficSectionFlow.h"
#include <cstddef>   // offsetof

namespace BrnTraffic
{
    void SectionFlow::_AssertLayout()
    {
        static_assert(offsetof(SectionFlow, muFlowTypeId)        == 0x00, "SectionFlow::muFlowTypeId");
        static_assert(offsetof(SectionFlow, muVehiclesPerMinute) == 0x02, "SectionFlow::muVehiclesPerMinute");

        // Pointer-free record: identical stride on console and host. Attested by the
        // shipped data (hull 4: 4 sections, 16-byte mpaSectionFlows block).
        static_assert(sizeof(SectionFlow) == 4, "SectionFlow stride");
    }
}
