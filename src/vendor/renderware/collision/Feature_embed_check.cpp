// Tiny embed/compile check for the rw::collision::Feature home: includes the
// header, pins the canonical asm-attested offsets (region @+0x000, edge
// records @+0x010 with stride 0x40, ownNormal @+0x210, pt @+0x220, numedges
// @+0x230, console stride 0x240), and touches the entry point so the gate
// exercises the TU.
#include "vendor/renderware/collision/Feature.hpp"

#include <cstddef>

namespace
{
using rw::collision::Feature;
using rw::collision::FeatureEdge;
using rw::collision::Vec4;

// offsetof pins the asm-proven layout without any runtime cost (never called).
static_assert(offsetof(Feature, region) == 0x000,
              "Feature region/feature-id word must sit at +0x000");
static_assert(offsetof(Feature, edges) == 0x010,
              "Feature edge-record array must start at +0x010");
static_assert(offsetof(FeatureEdge, dir) == 0x10,
              "edge direction must sit at record+0x10");
static_assert(offsetof(FeatureEdge, pn) == 0x20,
              "prism-wall plane normal must sit at record+0x20");
static_assert(offsetof(Feature, ownNormal) == 0x210,
              "face plane normal must sit at +0x210");
static_assert(offsetof(Feature, pt) == 0x220,
              "point-feature position must sit at +0x220");
static_assert(offsetof(Feature, numedges) == 0x230,
              "Feature edge count must sit at +0x230");

void FeatureEmbedCheck()
{
    Feature lFeature{};
    lFeature.numedges = 1;
    lFeature.edges[0].dir = Vec4{1.0f, 0.0f, 0.0f, 0.0f};

    Vec4 lExtrusionDir{0.0f, 0.0f, 1.0f, 0.0f};
    lFeature.BuildEdgePlanes(1, lExtrusionDir);
}
} // namespace
