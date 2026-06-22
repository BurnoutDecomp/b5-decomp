// Tiny embed check: include the TextureState home's headers and force ODR use of both
// reconstructed functions so the compile gate exercises their signatures end to end.
// Compile-only (cl /c); SamplerState::Initialize resolves at link in samplerstate.cpp.
#include "types.hpp"
#include "rw/rwcore_structs.h"

namespace renderengine
{
    struct SamplerStateParameters;
    struct TextureStateParameters;
    struct TextureStateData;

    class TextureState
    {
    public:
        static rw::BaseResourceDescriptors<5>* GetResourceDescriptor(rw::BaseResourceDescriptors<5>* lpDescriptor);
        static TextureStateData* Initialize(TextureStateData** lppState, const TextureStateParameters* lpParams);
    };
}

void texturestate_embed_check()
{
    using namespace renderengine;
    rw::BaseResourceDescriptors<5> lDesc{};
    (void)TextureState::GetResourceDescriptor(&lDesc);
    TextureStateData* lpState = nullptr;
    TextureStateParameters* lpParams = nullptr;
    (void)TextureState::Initialize(&lpState, lpParams);
}
