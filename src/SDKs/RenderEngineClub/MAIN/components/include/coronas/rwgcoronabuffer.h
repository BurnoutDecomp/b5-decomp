#pragma once

#include "types.hpp"

namespace rw
{
typedef u32 RGBA;

namespace math
{
struct Vector2
{
    float mfX;
    float mfY;
};

struct Vector3
{
    float mfX;
    float mfY;
    float mfZ;
};
}
}

namespace renderengine
{
struct ResourceDescriptor5
{
    struct Entry
    {
        u32 muSize;
        u32 muAlignment;
    };

    Entry maEntries[5];
};

struct Corona
{
    float mafPosition[4];
    float mafDirection[4];
    float mafSize[2];
    u64   mu64ColourAndDistance;
    float mfDistance;
    int   miTextureID;
    u32   muUnknown56;
    u32   muUnknown60;
};

static_assert(sizeof(Corona) == 64, "Corona must match runtime buffer stride");

class CoronaBuffer
{
public:
    struct Parameters
    {
        void SetNumCoronas(int liNumCoronas);

        int miNumCoronas;
    };

    struct Iterator
    {
        void Write(const Corona& rCorona);
        void Write(
            rw::math::Vector3 lPosition,
            rw::math::Vector3 lDirection,
            rw::math::Vector2 lSize,
            float lfDistance,
            rw::RGBA lColour,
            int liTextureID);
        void SetPosition(u32 luIndex);
        Corona& operator*();
        void operator++();

        Corona* mpData;
        u32     muIndex;
        u32     muNumCoronas;
    };

    static ResourceDescriptor5* GetResourceDescriptor(ResourceDescriptor5* pDescriptor, Parameters* pParameters);
    static CoronaBuffer* Initialize(CoronaBuffer** ppBuffer, Parameters* pParameters);

    void Release();
    u32 GetNumCoronas() const;
    const Corona* GetCoronas() const;
    void Lock(Iterator& rIterator);
    void Unlock();

    u32     muNumCoronas;
    Corona* mpData;
};

class CoronaRenderer
{
public:
    static void Initialize(void* pResourceAllocator);
};
}

class BrnCoronaManager
{
public:
    renderengine::CoronaBuffer* Construct(void* pResourceAllocator);

private:
    u32                         muConstructed;
    u32                         muPad4;
    u32                         muActiveCoronaCount;
    renderengine::CoronaBuffer* mpPrimaryBuffer;
    renderengine::CoronaBuffer* mpSecondaryBuffer;
    u8                          mPad20[20];
    renderengine::CoronaBuffer* mapPrimaryHandles[5];
    renderengine::CoronaBuffer* mapSecondaryHandles[5];
    renderengine::CoronaBuffer* mpPrimaryBufferMirror;
    u8                          mPad88[108];
    renderengine::CoronaBuffer* mpSecondaryBufferMirror;
    u8                          mPad200[108];
    u32                         muUnknown304;
};

namespace
{
class ResourceAllocator
{
public:
    virtual void* Create(
        void* pOut,
        ResourceAllocator* pAllocator,
        renderengine::ResourceDescriptor5* pDescriptor,
        int liFlags) = 0;
};

renderengine::CoronaBuffer* gpCoronaBuffer0;
renderengine::CoronaBuffer* gpCoronaBuffer1;
renderengine::CoronaBuffer* gpCoronaBuffer2;
u32 guCoronaFlag0;
u32 guCoronaFlag1;
void* gpCoronaVTable;
extern u8 gCoronaVTable_82FAFC10;

renderengine::Corona MakeDefaultCorona()
{
    renderengine::Corona lCorona = {};

    lCorona.mafDirection[2] = 1.0f;
    lCorona.mafSize[0] = 1.0f;
    lCorona.mafSize[1] = 1.0f;
    lCorona.mfDistance = 1.0f;
    lCorona.miTextureID = -1;
    return lCorona;
}

void FillDefaultCoronas(renderengine::CoronaBuffer* pBuffer, const renderengine::Corona& lCorona)
{
    for (u32 luIndex = 0; luIndex < pBuffer->muNumCoronas; ++luIndex)
        pBuffer->mpData[luIndex] = lCorona;
}

void AllocateCoronaBufferHandles(
    ResourceAllocator* pAllocator,
    renderengine::CoronaBuffer** papHandles,
    renderengine::ResourceDescriptor5* pDescriptor)
{
    renderengine::CoronaBuffer* lapAllocatedHandles[5] = {};
    pAllocator->Create(lapAllocatedHandles, pAllocator, pDescriptor, 0);

    for (int liHandle = 0; liHandle < 5; ++liHandle)
        papHandles[liHandle] = lapAllocatedHandles[liHandle];
}
}

inline void renderengine::CoronaBuffer::Parameters::SetNumCoronas(int liNumCoronas)
{
    miNumCoronas = liNumCoronas;
}

inline void renderengine::CoronaBuffer::Iterator::Write(const Corona& rCorona)
{
    mpData[muIndex] = rCorona;
    ++muIndex;
}

inline void renderengine::CoronaBuffer::Iterator::Write(
    rw::math::Vector3 lPosition,
    rw::math::Vector3 lDirection,
    rw::math::Vector2 lSize,
    float lfDistance,
    rw::RGBA lColour,
    int liTextureID)
{
    Corona& rCorona = mpData[muIndex];
    rCorona.mafPosition[0] = lPosition.mfX;
    rCorona.mafPosition[1] = lPosition.mfY;
    rCorona.mafPosition[2] = lPosition.mfZ;
    rCorona.mafDirection[0] = lDirection.mfX;
    rCorona.mafDirection[1] = lDirection.mfY;
    rCorona.mafDirection[2] = lDirection.mfZ;
    rCorona.mafSize[0] = lSize.mfX;
    rCorona.mafSize[1] = lSize.mfY;
    rCorona.mfDistance = lfDistance;
    rCorona.mu64ColourAndDistance = lColour;
    rCorona.miTextureID = liTextureID;
    ++muIndex;
}

inline void renderengine::CoronaBuffer::Iterator::SetPosition(u32 luIndex)
{
    muIndex = luIndex;
}

inline renderengine::Corona& renderengine::CoronaBuffer::Iterator::operator*()
{
    return mpData[muIndex];
}

inline void renderengine::CoronaBuffer::Iterator::operator++()
{
    ++muIndex;
}

inline void renderengine::CoronaBuffer::Release()
{
}

inline u32 renderengine::CoronaBuffer::GetNumCoronas() const
{
    return muNumCoronas;
}

inline const renderengine::Corona* renderengine::CoronaBuffer::GetCoronas() const
{
    return mpData;
}

inline void renderengine::CoronaBuffer::Lock(Iterator& rIterator)
{
    rIterator.mpData = mpData;
    rIterator.muIndex = 0;
    rIterator.muNumCoronas = muNumCoronas;
}

inline void renderengine::CoronaBuffer::Unlock()
{
}

inline renderengine::CoronaBuffer* BrnCoronaManager::Construct(void* pResourceAllocator)
{
    ResourceAllocator* lpAllocator = static_cast<ResourceAllocator*>(pResourceAllocator);
    renderengine::ResourceDescriptor5 lDescriptor;
    renderengine::CoronaBuffer::Parameters lParameters;

    muConstructed = 1;
    renderengine::CoronaRenderer::Initialize(pResourceAllocator);

    gpCoronaBuffer0 = gpCoronaBuffer1;
    gpCoronaBuffer1 = gpCoronaBuffer2;
    gpCoronaBuffer2 = nullptr;
    guCoronaFlag0 = 1;
    gpCoronaVTable = &gCoronaVTable_82FAFC10;

    lParameters.SetNumCoronas(512);
    renderengine::CoronaBuffer::GetResourceDescriptor(&lDescriptor, &lParameters);

    AllocateCoronaBufferHandles(lpAllocator, mapPrimaryHandles, &lDescriptor);
    AllocateCoronaBufferHandles(lpAllocator, mapSecondaryHandles, &lDescriptor);

    mpPrimaryBuffer = renderengine::CoronaBuffer::Initialize(mapPrimaryHandles, &lParameters);
    mpSecondaryBuffer = renderengine::CoronaBuffer::Initialize(mapSecondaryHandles, &lParameters);

    const renderengine::Corona lDefaultCorona = MakeDefaultCorona();
    FillDefaultCoronas(mpPrimaryBuffer, lDefaultCorona);
    FillDefaultCoronas(mpSecondaryBuffer, lDefaultCorona);

    muUnknown304 = 0;
    muActiveCoronaCount = 0;
    mpPrimaryBufferMirror = mpPrimaryBuffer;
    mpSecondaryBufferMirror = mpSecondaryBuffer;

    return mpSecondaryBuffer;
}
