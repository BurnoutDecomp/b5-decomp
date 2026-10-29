#ifndef GAMESOURCE_RESOURCE_BRNRESOURCEALLOCATOR_H
#define GAMESOURCE_RESOURCE_BRNRESOURCEALLOCATOR_H

#include "types.hpp"
#include "GameShared/GameClasses/Memory/CgsMemoryBank.h"
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"
#include "rw/rwcore_structs.h"   // rw::Resource, rw::ResourceDescriptor
#include "GameShared/GameClasses/Development/MessageSystem/CgsMessage.h"   // KX_FILTER_* (memory-check print)
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT (LinearResourceAllocator queries)

namespace rw { namespace core { struct GeneralResourceAllocator; } }

// Native ARTIST root resource allocators. Each wrapper retains its own backing
// allocator, name and original diagnostics; startup constructs all real heaps.

namespace BrnResource
{

// The Burnout heap-backed RenderWare resource allocator. Public Allocate() returns
// an rw::Resource carved from the underlying general allocator (the X360 build
// resolves this through the IResourceAllocator vtable's DoAllocate slot).
class HeapResourceAllocator : public rw::IResourceAllocator
{
public:
    // DecFIGS h1010; ARTIST construction sites store these three native fields.
    void Construct(rw::core::GeneralResourceAllocator* lpAllocator,
                   const char* lpcName, s32 liPrintLevel);
    // Allocate a resource matching lDescriptor (lpcName is a debug tag). The
    // returned rw::Resource's m_baseResources[0] is the memory-resource pointer.
    rw::Resource Allocate(const rw::ResourceDescriptor& lDescriptor, const char* lpcName = 0);

    // Original virtual allocation/free entries dispatch into the backing heap.
    rw::Resource DoAllocate(const rw::ResourceDescriptor& lDescriptor, const char* lpcName);
    void DoFree(const rw::Resource& lrResource);
    void DoFreeDisposable(rw::Resource& lrResource);

private:
    rw::core::GeneralResourceAllocator* mpGeneralAllocator;
    const char* mpcName;
    s32 miPrintLevel;
};

// The initial process-wide RenderWare resource allocator. ARTIST Construct @0x82661A60 and
// DoAllocate @0x82664800 traverse exactly five LinearMalloc records at the X360 0x1C stride;
// DecFIGS declares six, which is the PS3 shape and is not used by the X360 target. The native
// member array retains the five native records. DoFree and DoFreeDisposable
// preserve the original assertion-only behavior.
class DefaultLinearAllocator : public rw::IResourceAllocator
{
public:
    void Construct(u32 luPreAllocatedSize);
    void CheckRenderwareBudget();
    ~DefaultLinearAllocator() ;

protected:
    rw::Resource DoAllocate(const rw::ResourceDescriptor& lrDescriptor, const char* lpcName) ;
    void DoFree(const rw::Resource& lrResource) ;
    void DoFreeDisposable(rw::Resource& lrResource) ;

private:
    CgsMemory::LinearMalloc maAllocators[5];
};

// The named game-side wrapper over an rw::LinearResourceAllocator (the GlobalGraphics,
// GlobalResource, MemFile and EmbeddedTexture allocators of Allocators). The
// backing pointer and diagnostic name retain ARTIST's native declaration order.
class LinearResourceAllocator : public rw::IResourceAllocator
{
public:
    // ARTIST82666C48/4C inlines these two original member stores.
    void Construct(rw::LinearResourceAllocator* lpAllocator, const char* lpcName);

    rw::ResourceDescriptor GetCapacity() const;
    rw::ResourceDescriptor GetCurrentUsage() const;
    const rw::Resource&    GetLinearHeapBase() const;

protected:
    rw::Resource DoAllocate(const rw::ResourceDescriptor& lrDescriptor, const char* lpcName) ;
    void DoFree(const rw::Resource& lrResource) ;
    void DoFreeDisposable(rw::Resource& lrResource) ;

private:
    rw::LinearResourceAllocator* mpLinearAllocator;
    const char*                  mpcName;
};

inline rw::ResourceDescriptor LinearResourceAllocator::GetCapacity() const
{
    CGS_ASSERT(mpLinearAllocator, "No linear allocator\n");
    return mpLinearAllocator->GetCapacity();
}

inline rw::ResourceDescriptor LinearResourceAllocator::GetCurrentUsage() const
{
    CGS_ASSERT(mpLinearAllocator, "No linear allocator\n");
    return mpLinearAllocator->GetCurrentUsage();
}

inline const rw::Resource& LinearResourceAllocator::GetLinearHeapBase() const
{
    CGS_ASSERT(mpLinearAllocator, "No linear allocator\n");
    return mpLinearAllocator->GetLinearHeapBase();
}

// Accessor for the global debug allocator (asserts the backing allocator exists).
// ARTIST823A7CB8 returns &Allocators::mGlobalDebugAllocator without allocating.
HeapResourceAllocator* GetDebugAllocator();
// BrnResourceAllocator.h:381; ARTIST main allocates through mRuntimeAllocator
// at 82F2C854, independently of the debug allocator at 82F2C7DC.
HeapResourceAllocator* GetRuntimeAllocator();
// Genuine public wrapper; distinct from the retained native allocation helper.
LinearResourceAllocator* GetGraphicsAllocator();
// Native GlobalResource wrapper at ARTIST82F2C848, independent of GlobalDebug.
LinearResourceAllocator* GetGlobalResourceAllocator();
// DecFIGS h405; ARTIST loading-screen allocation uses 82F2C870.
LinearResourceAllocator* GetEmbeddedTextureAllocator();
void ClearResource(rw::Resource& lrResource, const rw::ResourceDescriptor& lrDescriptor);

// Logs the message (when given) and the platform memory status to the debug print, one line per
// figure in MB plus the in-use percentage. Each line is gated on the debug-print message filter.
void PrintConsoleMemory(const char* lpcMessage);

// The platform's free physical memory in bytes.
u32 GetAvailableMemory();

// Original process-wide startup allocator homes.
class Allocators
{
public:
    static rw::core::GeneralResourceAllocator* mpInternalDebugAllocator;

    // When set, a failed memory check (the available-memory comparisons in
    // BrnNetworkModule::ProcessAfterSimulation, the game module's DoUpdate and the network
    // managers) fires "Memory check failed". Never written by the game; false.
    static bool mbAssertOnMemoryChange;

    // The startup memory-map/initial-default-allocator chain (ARTIST 0x82664728).
    static void InitMemoryMap();
    // ARTIST8266AED8: original construction order and default-allocator switch.
    static void Construct();

    // Existing graphics callers receive the genuine GlobalGraphics wrapper.
    static rw::IResourceAllocator* GetGlobalGraphicsAllocator();

private:
    static void ConstructGlobalSystemMemory();
    static void ConstructGlobalDebugMemory();
    static void ConstructGlobalGraphicsMemory();
    static void ConstructGlobalResourceMemory(const CgsMemory::MemoryBank::Params* lpRootBankParams);
    static void ConstructChildAllocators();
    friend HeapResourceAllocator* GetDebugAllocator();
    static rw::Resource mGlobalSystemMemory;
    static rw::ResourceDescriptor mGlobalSystemMemoryDescriptor;
    static rw::core::GeneralResourceAllocator* mpInternalGlobalSystemAllocator;
    static HeapResourceAllocator mGlobalSystemAllocator;
    static rw::Resource mGlobalDebugMemory;
    static rw::ResourceDescriptor mGlobalDebugMemoryDescriptor;
    static HeapResourceAllocator mGlobalDebugAllocator;
    static rw::Resource mGlobalResourceMemory;
    static rw::ResourceDescriptor mGlobalResourceMemoryDescriptor;
    static rw::LinearResourceAllocator* mpInternalResourceAllocator;
    friend LinearResourceAllocator* GetGraphicsAllocator();
    friend LinearResourceAllocator* GetGlobalResourceAllocator();
    static LinearResourceAllocator mGlobalResourceAllocator;
    friend HeapResourceAllocator* GetRuntimeAllocator();
    friend LinearResourceAllocator* GetEmbeddedTextureAllocator();
    static HeapResourceAllocator mRuntimeAllocator;
    static LinearResourceAllocator mEmbeddedTextureAllocator;
    static rw::Resource mGlobalGraphicsMemory;
    static rw::ResourceDescriptor mGlobalGraphicsMemoryDescriptor;
    static rw::LinearResourceAllocator* mpInternalGraphicsAllocator;
    static LinearResourceAllocator mGlobalGraphicsAllocator;
    static DefaultLinearAllocator mInitialDefaultAllocator;
};

} // namespace BrnResource

// The inlined memory check: re-read the free memory, and when it differs from luMemory log the
// change (with this site's line and file), take the new reading, and assert when
// Allocators::mbAssertOnMemoryChange is set. The expansion site needs CgsLog.h and CgsAssert.h.
// FLAG: the macro's own name is not recovered; the expanded body is the shipped one, except that
// [PC] the print is additionally gated on the memory log category, which the PC default filter
// (CgsLog.cpp) leaves off: once the network module runs, dozens of these lines fire per frame.
#define BRN_RESOURCE_MEMORY_CHECK(luMemory)                                                                    \
    do                                                                                                         \
    {                                                                                                          \
        const u32 luMemoryNow = BrnResource::GetAvailableMemory();                                             \
        if ((luMemory) != luMemoryNow)                                                                         \
        {                                                                                                      \
            if ((CgsDev::Message::gxMessageFilterFlags & CgsDev::Message::KX_FILTER_GLOBAL) != 0 &&            \
                (CgsDev::Message::gxMessageFilterFlags & CgsDev::Message::KX_FILTER_GSMEMORY) != 0)            \
            {                                                                                                  \
                *CgsDev::Log::gpDebugPrint << "[" << static_cast<s32>(__LINE__) << ":" << __FILE__             \
                                           << "] Available memory changed from " << (luMemory)                 \
                                           << " to " << luMemoryNow << "\n";                                   \
            }                                                                                                  \
            (luMemory) = luMemoryNow;                                                                          \
            CGS_ASSERT(!BrnResource::Allocators::mbAssertOnMemoryChange, "Memory check failed");               \
        }                                                                                                      \
    } while (0)

#endif // GAMESOURCE_RESOURCE_BRNRESOURCEALLOCATOR_H
