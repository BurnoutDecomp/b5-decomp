// Tiny embed/compile check for the CgsGraphics::PackedOobb home: includes the
// header and touches the reconstructed entry point so the gate exercises the TU.
#include "GameShared/GameClasses/Graphics/Dispatch/CgsPackedOobb.h"

namespace
{
void CgsPackedOobbEmbedCheck()
{
    CgsGraphics::PackedOobb lOobb{};
    rw::math::vpu::Matrix44 lMatrix;
    lOobb.ToMatrix(lMatrix);
    (void)lMatrix;
}
} // namespace
