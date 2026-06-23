#include "BrnInterpolator.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGui::Interpolator<float>::GetCurrentValue @ 0x82448898
//
// Interpolator<T> is a header-defined template (the body must be visible to every
// using-TU on the X360, where it is instantiated per-renderer). This TU is the
// definition home for the float instantiation: it forces Interpolator<f32> so the
// GetCurrentValue body is emitted and the compile gate exercises it. The X360 build
// emits one instance per using-TU (dup ledger addrs, one .cpp covers all).

namespace BrnGui
{
// Force the float instantiation (the only in-scope keyed type; @0x82448898).
template class Interpolator<f32>;
}
