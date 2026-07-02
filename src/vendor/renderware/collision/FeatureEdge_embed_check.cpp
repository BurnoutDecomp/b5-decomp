// Tiny embed/compile check for the rw::collision::FeatureEdge home: includes
// the header and touches each reconstructed entry point so the gate exercises
// the full TU.
#include "vendor/renderware/collision/FeatureEdge.hpp"

namespace
{
void FeatureEdgeEmbedCheck()
{
    using rw::collision::FeatureEdge;
    using rw::collision::Vec4;

    Vec4 lA{0.0f, 0.0f, 0.0f, 0.0f};
    Vec4 lB{1.0f, 0.0f, 0.0f, 0.0f};
    FeatureEdge lEdge(lA, lB);

    Vec4 lPoint{0.5f, 0.0f, 0.0f, 0.0f};
    u32 luRegion = lEdge.constrain_point(lPoint);
    (void)luRegion;
}
} // namespace
