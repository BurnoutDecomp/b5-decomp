#ifndef RW_GPFX_HELPER_H
#define RW_GPFX_HELPER_H

#include "types.hpp"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"  // renderengine::ProgramBufferData

// rw::graphics::postfx::PfxHelper -- the post-fx singleton helper. It owns the shared post-fx
// render state (opaque blend state, the full-screen quad geometry, the work render targets) and
// the program factory the individual effects (DepthOfField, Tint, RenderTargetDebugger, ...) call
// to compile their shaders. Only the declaration surface the effects compile against is provided
// here; the helper's own bodies live in their own TU (rw::graphics::postfx::PfxHelper::*).
//
// CreateProgram is X360-attested (the post-fx constructors call it to build their vertex/pixel
// programs from embedded microcode). The remaining helper API (GetInstance / GetOpaqueBlendState /
// GetState) is declared as the effects reach for it, gated on the X360 ledger; bodies are separate.
namespace rw
{
namespace graphics
{
namespace postfx
{
    class PfxHelper
    {
    public:
        // Compile a shader program from embedded microcode and return its runtime buffer.
        //   leType     0 -> vertex program, 1 -> pixel program (the renderengine shader-kind flag)
        //   lpMicrocode  pointer to the embedded compiled-shader microcode blob (platform data)
        //   luSize       microcode byte size
        //   lpReserved   always 0 at the X360 call sites
        static renderengine::ProgramBufferData* CreateProgram(s32 leType, const void* lpMicrocode,
                                                              u32 luSize, void* lpReserved);
    };
}
}
}

#endif // RW_GPFX_HELPER_H
