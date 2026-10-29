#include "SDKs/RenderEngineClub/MAIN/components/src/vertexbuffer.h"
#include "GameSource/Effects/Particles/EffectsVertexBufferManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h" // CgsResource::ResourceDescriptor
#include "rw/rwcore_structs.h"                                      // rw::IResourceAllocator, rw::Resource

// EffectsVertexBufferManager::GetVertexBuffer  X360 0x82278320
// Asserts the manager is not locked for write, then returns the vertex buffer
// for the current double-buffer slot: mapVertexBuffer[muCurrentBuffer].
// Asm: lbz r11,0x15 (mbLocked) -> assert; lbz r11,0x14 (muCurrentBuffer);
//      rotlwi r11,r11,2 (index*4); lwzx r3,r11,r31 -> mapVertexBuffer[index].
renderengine::VertexBuffer* EffectsVertexBufferManager::GetVertexBuffer() const
{
    CGS_ASSERT(!mbLocked, "!mbLocked");
    return mapVertexBuffer[muCurrentBuffer];
}

// EffectsVertexBufferManager::Construct  X360 0x822807D0
// Allocate KU_NUM_BUFFERS vertex buffers of luVertexBufferSize bytes through the
// resource allocator, capture each buffer's GPU write address (lock/unlock once),
// and zero the double-buffer bookkeeping plus the embedded EffectsVertexBuffer.
//
// The DWARF source drives this through renderengine::VertexBuffer::Parameters +
// a BatchIterator; the X360 image inlines those helpers, so the reconstruction is
// store-for-store against the inlined asm:
//   - Parameters {format=0, length=size}     -> v12 = {0, a3}
//   - GetResourceDescriptor(&desc, &params)  -> renderengine::VertexBuffer::GetResourceDescriptor
//   - allocator->DoAllocate(desc, 0)         -> (*(*a2 + 16))(&res, a2, &desc, 0)
//   - Initialize(res-as-wrapper, &params)    -> renderengine::VertexBuffer::Initialize
//   - GetLockedBuffer inlined                -> read muSize, Xbox2UnSet, D3DVertexBuffer_Lock,
//                                               assert size, capture address, D3DVertexBuffer_Unlock
// lbPs3CreateInMainMemory is a PS3-only parameter; the X360 image never reads it.
void EffectsVertexBufferManager::Construct(rw::IResourceAllocator* lpAllocator,
                                           u32 luVertexBufferSize,
                                           bool lbPs3CreateInMainMemory)
{
    (void)lbPs3CreateInMainMemory;   // unused on X360 (PS3-only main-memory hint)

    CGS_ASSERT(lpAllocator, "lpAllocator");

    // Parameters block Initialize/GetResourceDescriptor read: {format=0, length}.
    renderengine::VertexBuffer::Parameters lVbParams;
    lVbParams.type = 0u;
    lVbParams.bufferSize = luVertexBufferSize;

    rw::ResourceDescriptor lVbResDesc = renderengine::VertexBuffer::GetResourceDescriptor(lVbParams);

    for (u32 luCount = 0u; luCount < KU_NUM_BUFFERS; ++luCount)
    {
        // Allocate a GPU buffer through the resource allocator (vtable DoAllocate).
        rw::Resource lResource = lpAllocator->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>(lVbResDesc), 0);

        renderengine::VertexBuffer* const lpBuffer =
            renderengine::VertexBuffer::Initialize(lResource, lVbParams);
        mapVertexBuffer[luCount] = lpBuffer;
        CGS_ASSERT(mapVertexBuffer[luCount], "mapVertexBuffer[luCount]");

        renderengine::VertexBuffer::BatchIterator lIterator;
        lpBuffer->Lock(0, lIterator);
        CGS_ASSERT(lIterator.GetLockedBufferSize() == luVertexBufferSize,
                   "lBatchIterator.GetLockedBufferSize() == luVertexBufferSize");
        mapVertexBufferAddress[luCount] = lIterator.GetLockedBuffer();
        CGS_ASSERT(mapVertexBufferAddress[luCount], "mapVertexBufferAddress[luCount]");
        lpBuffer->Unlock(lIterator);
    }

    muVertexBufferSize = luVertexBufferSize;
    muCurrentBuffer    = 0u;
    mbLocked           = false;
    mCurrentBuffer     = EffectsVertexBuffer();   // zero base/current/top/mxFlags
}

// EffectsVertexBufferManager::Lock  X360 0x82279B18
// Open a write window over the current double-buffer slot: seed the embedded
// EffectsVertexBuffer over [address, address + size), then Lock() it and hand back
// the locked view. The X360 image inlines EffectsVertexBuffer::Construct + Lock;
// the reconstruction calls them by name (the DWARF source's construct-then-lock).
EffectsVertexBufferLocked& EffectsVertexBufferManager::Lock()
{
    CGS_ASSERT(!mbLocked, "!mbLocked");

    mbLocked = true;
    mCurrentBuffer.Construct(mapVertexBufferAddress[muCurrentBuffer], muVertexBufferSize);
    return mCurrentBuffer.Lock();
}

// EffectsVertexBufferManager::UnLock  X360 0x82279BA8
// Close the write window on the current slot (EffectsVertexBuffer::UnLock inlined by
// the X360 image -- its own asserts at EffectsVertexBuffer.cpp:122/78 live inside that
// call), then clear the manager's locked flag.
void EffectsVertexBufferManager::UnLock()
{
    CGS_ASSERT(mbLocked, "mbLocked");

    mCurrentBuffer.UnLock();
    mbLocked = false;
}
