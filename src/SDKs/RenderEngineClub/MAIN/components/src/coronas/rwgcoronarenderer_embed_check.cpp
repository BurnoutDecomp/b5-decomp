// Tiny embed check: force ODR use of the reconstructed CoronaRenderer entry points so the compile
// gate exercises their signatures end to end. Compile-only (cl /c); the bodies live in
// rwgcoronarenderer.cpp.
//
// REWRITTEN 2026-08-17 (coronas step 1). It used to RE-DECLARE the class --
//     namespace renderengine { class CoronaRenderer { static void Initialize(void*); }; }
// -- because "the committed declaration in coronas/rwgcoronabuffer.h is outside the gate's include
// root". That reason was never true (the gate's include root is b5-decomp/src, which contains that
// header), and the local re-declaration was a SECOND, DIVERGENT declaration of a class that now has
// a real home: its `void*` parameter mangles differently from the real
// `rw::IResourceAllocator&`, so this file's UNDEF could never be satisfied by the real body -- the
// embed check would have "passed" while checking a function that does not exist.
// It now includes the real header, which is what an embed check is for.
#include "SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronarenderer.h"

void rwgcoronarenderer_embed_check(rw::IResourceAllocator& lrAllocator,
                                   const renderengine::CoronaRenderer::RenderParameters& lrRenderParams,
                                   const renderengine::CoronaRenderer::BatchParameters& lrBatchParams,
                                   renderengine::CoronaBuffer* lpBuffer)
{
    renderengine::CoronaRenderer::Initialize(lrAllocator);
    renderengine::CoronaRenderer::Begin(lrRenderParams);
    renderengine::CoronaRenderer::Dispatch(lrBatchParams, lpBuffer);
    renderengine::CoronaRenderer::End();
}
