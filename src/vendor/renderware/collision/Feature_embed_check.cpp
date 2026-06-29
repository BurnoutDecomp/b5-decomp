// Tiny embed/compile check for the rw::collision::Feature home: includes the
// header, pins the two offsets BuildEdgePlanes actually uses (count @ +0x230,
// record stride 0x40 with the edge dir at record+0x00 and the plane normal at
// record+0x10), and touches the entry point so the gate exercises the TU.
#include "vendor/renderware/collision/Feature.hpp"

#include <cstddef>

namespace
{
using rw::collision::Feature;

// offsetof pins the asm-proven layout without any runtime cost (never called).
static_assert(offsetof(Feature, mEdgePlanes) == 0x20,
              "Feature edge-record array must start at +0x20");
static_assert(sizeof(Feature::EdgePlaneRecord) == 0x40,
              "Feature edge record stride must be 0x40");
static_assert(offsetof(Feature::EdgePlaneRecord, mPlaneNormal) == 0x10,
              "plane normal must sit at record+0x10");
static_assert(offsetof(Feature, mi32EdgePlaneCount) == 0x230,
              "Feature edge count must sit at +0x230");

void FeatureEmbedCheck()
{
    Feature lFeature{};
    lFeature.mi32EdgePlaneCount = 1;
    lFeature.mEdgePlanes[0].mEdgeDir = Feature::Vec4{1.0f, 0.0f, 0.0f, 0.0f};

    Feature::Vec4 lRefNormal{0.0f, 0.0f, 1.0f, 0.0f};
    Feature* lpResult = lFeature.BuildEdgePlanes(1, &lRefNormal);
    (void)lpResult;
}
} // namespace
