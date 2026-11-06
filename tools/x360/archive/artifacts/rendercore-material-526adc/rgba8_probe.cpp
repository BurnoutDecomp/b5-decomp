#include "SDKs/EATech/include/cmn/renderengine/types.h"
#include "GameSource/Effects/Particles/Native/BrnLionBlendVertex.h"
#include "GameSource/Effects/Particles/Native/BrnLionBlendRenderer.h"
#include <new>

struct NativeColorPlacement
{
    u8 lead;
    renderengine::RGBA8 color;
    u8 tail;
};

CGS_STATIC_ASSERT((offsetof(NativeColorPlacement, color) == 4), "native word alignment");
CGS_STATIC_ASSERT((offsetof(NativeColorPlacement, tail) == 8), "native four-byte storage");
CGS_STATIC_ASSERT((sizeof(NativeColorPlacement) == 12), "native containing stride");

extern "C" void NativeRGBA8Construct(renderengine::RGBA8* result, u8 r, u8 g, u8 b, u8 a)
{
    new (result) renderengine::RGBA8(r, g, b, a);
}

extern "C" void NativeRGBA8Copy(renderengine::RGBA8* result, const renderengine::RGBA8* source)
{
    *result = *source;
}
