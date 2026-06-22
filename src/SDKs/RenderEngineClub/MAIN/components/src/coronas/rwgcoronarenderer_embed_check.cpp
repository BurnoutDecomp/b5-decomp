// Tiny embed check: re-declare the CoronaRenderer home surface and force ODR use of the
// reconstructed Initialize so the compile gate exercises its signature end to end. Compile-only
// (cl /c); the real body + the local ProgramBuffer / VertexDescriptor call surface live in
// rwgcoronarenderer.cpp. (Mirrors the committed declaration in coronas/rwgcoronabuffer.h, which is
// outside the gate's include root, so it is re-declared here per the vendor-TU embed-check pattern.)
#include "types.hpp"

namespace renderengine
{
    class CoronaRenderer
    {
    public:
        static void Initialize(void* pResourceAllocator);
    };
}

void rwgcoronarenderer_embed_check()
{
    renderengine::CoronaRenderer::Initialize(nullptr);
}
