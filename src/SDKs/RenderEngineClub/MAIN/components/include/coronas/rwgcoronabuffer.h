#pragma once

#include "SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcorona.h"
#include "rw/rwcore_structs.h"

namespace renderengine
{
class CoronaBuffer
{
public:
    struct Parameters
    {
        void SetNumCoronas(s32 liNumCoronas) { miNumCoronas = liNumCoronas; }
        s32 miNumCoronas;
    };

    struct Iterator
    {
        // ARTIST823F3350: v1/v2/v3, f1, ABGR colour r5, texture r6.
        void Write(Vector3 lvPosition, Vector3 lvDirection, Vector2 lvSize,
                   f32 lfDistance, rw::RGBA lColour, s32 liTextureID);
        void SetPosition(u32 luIndex) { muIndex = luIndex; }
        Corona& operator*() { return mpData[muIndex]; }
        void operator++() { ++muIndex; }

    protected:
        Corona* mpData;
        u32 muIndex;
        u32 muNumCoronas;
        friend class CoronaBuffer;
    };

    // ARTIST8228D6C0/822850D8; DecFIGS rwgcoronabuffer.h:42/43.
    static rw::ResourceDescriptor GetResourceDescriptor(Parameters& lrParameters);
    static CoronaBuffer* Initialize(const rw::Resource& lrResource, Parameters& lrParameters);
    void Release() {}
    u32 GetNumCoronas() const { return muNumCoronas; }
    const Corona* GetCoronas() const { return mpData; }
    void Lock(Iterator& lrIterator)
    {
        lrIterator.muIndex = 0;
        lrIterator.mpData = mpData;
        lrIterator.muNumCoronas = muNumCoronas;
    }
    void Unlock() {}

private:
    u32 muNumCoronas;
    Corona* mpData;
};
CGS_STATIC_ASSERT((sizeof(CoronaBuffer) == 8), "ARTIST corona buffer header");
CGS_STATIC_ASSERT((sizeof(CoronaBuffer::Iterator) == 12), "ARTIST corona iterator");
}
