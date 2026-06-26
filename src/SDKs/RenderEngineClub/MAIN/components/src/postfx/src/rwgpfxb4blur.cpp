#include "types.hpp"

#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxb4blur.h"   // B4Blur::State

#include <cmath>   // std::pow

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   rw::graphics::postfx::B4Blur::State::State            @ 0x823F52E0
//   rw::graphics::postfx::B4Blur::State::SetBlendSharpness @ 0x823F53A8

namespace rw
{
namespace graphics
{
namespace postfx
{
    // X360 0x823F52E0.
    // The ctor builds four {x, y, 0, 0} vectors on the stack and writes them with stvx128 at
    // +0x00/+0x10/+0x20/+0x30, then writes the six trailing scalars with stfs. The .rdata floats
    // it loads are flt_82001C98 == 1.0, flt_82001DA0 == 0.5, flt_82001CC0 == 0.0 and
    // flt_8200CE04 == 0.005 (the decompiler shows the last as 0.0049999999, the single-precision
    // image of 0.005). The vector lanes z/w are the 0.0 the SIMD slot carries.
    B4Blur::State::State()
    {
        m_blendAmount.x = 1.0f;  m_blendAmount.y = 1.0f;  m_blendAmount.z = 0.0f;  m_blendAmount.w = 0.0f;
        m_blurAmount.x  = 1.0f;  m_blurAmount.y  = 1.0f;  m_blurAmount.z  = 0.0f;  m_blurAmount.w  = 0.0f;
        m_blendCenter.x = 0.5f;  m_blendCenter.y = 0.5f;  m_blendCenter.z = 0.0f;  m_blendCenter.w = 0.0f;
        m_blurCenter.x  = 0.5f;  m_blurCenter.y  = 0.5f;  m_blurCenter.z  = 0.0f;  m_blurCenter.w  = 0.0f;

        m_blurOpacity   = 1.0f;   // +0x40
        m_blurVelocity  = 1.0f;   // +0x44
        m_blendSharpMUL = 0.0f;   // +0x48
        m_blendSharpADD = 0.0f;   // +0x4C
        m_blendNoise    = 0.005f; // +0x50
        m_blendAngle    = 0.0f;   // +0x54
    }

    // X360 0x823F53A8.
    // f13 = (lfSharpness + 1.0) * 0.5            ; remap [-1,1] -> [0,1] (computed in double)
    // f1  = pow(f13, 16.0)                       ; bl sub_82C09970 (the CRT pow(double,double))
    // f13 = frsp(f1)                             ; round the pow result down to single
    // f0  = f13 * 500.0                          ; single-precision scale (flt_8200A034 == 500.0)
    // m_blendSharpMUL = f0 + 1.0                 ; stfs 0x48  (flt_82001C98 == 1.0)
    // m_blendSharpADD = -f0                      ; stfs 0x4C
    // The eight phantom integer "args" Hex-Rays shows on the call are leftover GPRs from the merged
    // pow signature, not real parameters; pow consumes only the two doubles (the value and 16.0).
    void B4Blur::State::SetBlendSharpness(f32 lfSharpness)
    {
        const f32 lfPower = static_cast<f32>(
            std::pow((static_cast<double>(lfSharpness) + 1.0) * 0.5, 16.0));   // frsp of pow(...)
        const f32 lfScaled = lfPower * 500.0f;

        m_blendSharpMUL = lfScaled + 1.0f;   // +0x48
        m_blendSharpADD = -lfScaled;         // +0x4C
    }
}
}
}
