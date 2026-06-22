// Tiny embed/compile check for the rw::collision::FeatureEdge home: includes
// the header and touches each reconstructed entry point so the gate exercises
// the full TU.
#include "vendor/renderware/collision/FeatureEdge.hpp"

namespace
{
void FeatureEdgeEmbedCheck()
{
    using rw::collision::FeatureEdge;

    FeatureEdge::Vec4 lA{0.0f, 0.0f, 0.0f, 0.0f};
    FeatureEdge::Vec4 lB{1.0f, 0.0f, 0.0f, 0.0f};
    FeatureEdge lEdge(&lA, &lB);

    FeatureEdge::Vec4 lPoint{0.5f, 0.0f, 0.0f, 0.0f};
    int liRegion = lEdge.constrain_point(&lPoint);
    (void)liRegion;
}
} // namespace
