#include "SDKs/EATech/include/Nicotine/IDynamicMixer.hpp"
#include <cstddef> // offsetof

// Compile-gate the IDynamicMixer bridge type (the CgsSound -> NFS mix seam).
static void IDynamicMixer_embed_check()
{
    (void)sizeof(Nicotine::IDynamicMixer);
    static_assert(sizeof(Nicotine::IDynamicMixer::MAP_CREATE_PARAMS) == 2 * sizeof(int) + sizeof(void*),
                  "MAP_CREATE_PARAMS = {int,int,allocator*}");
    static_assert(offsetof(Nicotine::IDynamicMixer, mMixMaster) < offsetof(Nicotine::IDynamicMixer, mLiveLink),
                  "mMixMaster precedes mLiveLink");
    static_assert(offsetof(Nicotine::IDynamicMixer, mCreateParams) < offsetof(Nicotine::IDynamicMixer, mpSnapshot),
                  "mCreateParams precedes mpSnapshot");
}
