// Tiny embed/compile check for the rw::math::vpu::MaskScalar home (declared inline in
// rw/math/vpu/types.h, sibling to the Vector*/Matrix* aggregates). types.h is a
// header-only type-vocabulary file, so this TU provides the single compiled translation
// unit that exercises the reconstructed entry point (MaskScalar::GetBool @ 0x821F0D78)
// for the gate.
#include "rw/math/vpu/types.h"

namespace
{
void MaskScalarEmbedCheck()
{
    using namespace rw::math::vpu;

    sizeof(MaskScalar);

    // All four lanes 0.0 -> vcmpeqfp. all-equal -> GetBool() == false (mask empty).
    MaskScalar lAllZero = { 0.0f, 0.0f, 0.0f, 0.0f };
    (void)lAllZero.GetBool();

    // Any non-zero lane -> not-all-equal -> GetBool() == true (mask non-empty).
    MaskScalar lOneSet = { 0.0f, 0.0f, 1.0f, 0.0f };
    (void)lOneSet.GetBool();
}
} // namespace
