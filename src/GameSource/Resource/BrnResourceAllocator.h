#ifndef GAMESOURCE_RESOURCE_BRNRESOURCEALLOCATOR_H
#define GAMESOURCE_RESOURCE_BRNRESOURCEALLOCATOR_H

#include "types.hpp"
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"
#include "GameShared/GameClasses/Memory/CgsMemoryBank.h"
#include "rw/rwcore_structs.h"   // rw::Resource, rw::ResourceDescriptor
#include "GameShared/GameClasses/Development/MessageSystem/CgsMessage.h"   // KX_FILTER_* (memory-check print)
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT (LinearResourceAllocator queries)

namespace rw { namespace core { struct GeneralResourceAllocator; } }   // GetGameDataGeneralAllocator return type

// The named wrappers and startup-global storage follow ARTIST, with declaration
// shape corroborated by DecFIGS BrnResourceAllocator.h. Native lazy backing is an
// explicit boundary in the implementation; it shares these public wrappers.

namespace BrnResource
{

// The Burnout heap-backed RenderWare resource allocator. Public Allocate() returns
// an rw::Resource carved from the underlying general allocator (the X360 build
// resolves this through the IResourceAllocator vtable's DoAllocate slot).
class HeapResourceAllocator : public rw::IResourceAllocator
{
public:
    HeapResourceAllocator();
    void Construct(rw::core::GeneralResourceAllocator* lpAllocator,
                   const char* lpcName, s32 liPrintLevel);
    // Allocate a resource matching lDescriptor (lpcName is a debug tag). The
    // returned rw::Resource's m_baseResources[0] is the memory-resource pointer.
    rw::Resource Allocate(const rw::ResourceDescriptor& lDescriptor, const char* lpcName = 0);

    // The virtual wrapper validates the request and delegates all five lanes to
    // the internal General allocator. Allocate is the existing public convenience entry.
    rw::Resource DoAllocate(const rw::ResourceDescriptor& lDescriptor, const char* lpcName) override;
    void DoFree(const rw::Resource& lrResource) override;
    void DoFreeDisposable(rw::Resource& lrResource) override;

private:
    rw::core::GeneralResourceAllocator* mpGeneralAllocator;
    const char* mpcName;
    s32 miPrintLevel;
};

// The initial process-wide RenderWare resource allocator. ARTIST Construct @0x82661A60 and
// DoAllocate @0x82664800 traverse exactly five LinearMalloc records at the X360 0x1C stride;
// DecFIGS declares six, which is the PS3 shape and is not used by the X360 target. The native
// member array remains typed and widens naturally with the host pointers. DoFree and
// DoFreeDisposable preserve the original assertion-only behavior.
class DefaultLinearAllocator : public rw::IResourceAllocator
{
public:
    void Construct(u32 luPreAllocatedSize);
    void CheckRenderwareBudget();
    ~DefaultLinearAllocator() override;

protected:
    rw::Resource DoAllocate(const rw::ResourceDescriptor& lrDescriptor, const char* lpcName) override;
    void DoFree(const rw::Resource& lrResource) override;
    void DoFreeDisposable(rw::Resource& lrResource) override;

private:
    CgsMemory::LinearMalloc maAllocators[5];
};

// The named game-side wrapper over a RenderWare linear allocator.
class LinearResourceAllocator : public rw::IResourceAllocator
{
public:
    LinearResourceAllocator();
    void Construct(rw::LinearResourceAllocator* lpAllocator, const char* lpcName);
    rw::ResourceDescriptor GetCapacity() const;
    rw::ResourceDescriptor GetCurrentUsage() const;
    const rw::Resource&    GetLinearHeapBase() const;

protected:
    rw::Resource DoAllocate(const rw::ResourceDescriptor& lrDescriptor, const char* lpcName) override;
    void DoFree(const rw::Resource& lrResource) override;
    void DoFreeDisposable(rw::Resource& lrResource) override;

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

// Returns the one public GlobalDebug wrapper; the internal pointer is a distinct
// GeneralResourceAllocator. The native getter lazily adopts its existing low-memory arena.
HeapResourceAllocator* GetDebugAllocator();

// The GameData general resource allocator the loading-screen module loads carve from (SoundModule
// etc.). [PC-LEAF / allocator gate] The X360 builds one allocator per bank in GameDataModule::
// CreateAllocators (memory-map-driven) and vends it via GameDataIO::OutputBuffer::GetAllocator;
// pending that per-bank population, this returns a single real rw::core::GeneralResourceAllocator
// lazily adopted over a region carved from the debug heap. (The full per-bank CreateAllocators
// population is the faithful follow-on; see the allocator-gate memory.)
rw::core::GeneralResourceAllocator* GetGameDataGeneralAllocator();

// X360 0x82661760: the optional message prefix and each status line are gated by filter bit 0;
// preserve the original labels/order and stream the PC's full-width native counters as u64 MB.
void PrintConsoleMemory(const char* lpcMessage);
void ClearResource(rw::Resource& lrResource, const rw::ResourceDescriptor& lrDescriptor);

// The platform's free physical memory in bytes.
u32 GetAvailableMemory();

// Owns the engine's named global allocators. Construct is recovered separately
// from native startup activation; its physical/GPU backing remains unvalidated on PC.
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
    static void Construct();

    // Native lazy graphics backing and the recovered constructor use the same
    // canonical GlobalGraphics wrapper. Aligned CPU memory does not prove GPU visibility.
    static rw::IResourceAllocator* GetGlobalGraphicsAllocator();

private:
    friend HeapResourceAllocator* GetDebugAllocator();
    friend class HeapResourceAllocator;

    static void EnsureNativeDebugBacking();
    static void ConstructGlobalSystemMemory();
    static void ConstructGlobalDebugMemory();
    static void ConstructGlobalGraphicsMemory();
    static void ConstructGlobalResourceMemory(const CgsMemory::MemoryBank::Params* lpParams);
    static void ConstructChildAllocators();

    static DefaultLinearAllocator mInitialDefaultAllocator;
    static rw::Resource mGlobalSystemMemory;
    static rw::ResourceDescriptor mGlobalSystemMemoryDescriptor;
    static rw::core::GeneralResourceAllocator* mpInternalGlobalSystemAllocator;
    static HeapResourceAllocator mGlobalSystemAllocator;
    static rw::Resource mGlobalDebugMemory;
    static rw::ResourceDescriptor mGlobalDebugMemoryDescriptor;
    static HeapResourceAllocator mGlobalDebugAllocator;
    static rw::Resource mGlobalGraphicsMemory;
    static rw::ResourceDescriptor mGlobalGraphicsMemoryDescriptor;
    static rw::LinearResourceAllocator* mpInternalGraphicsAllocator;
    static LinearResourceAllocator mGlobalGraphicsAllocator;
    static rw::Resource mGlobalResourceMemory;
    static rw::ResourceDescriptor mGlobalResourceMemoryDescriptor;
    static rw::LinearResourceAllocator* mpInternalResourceAllocator;
    static LinearResourceAllocator mGlobalResourceAllocator;
    static HeapResourceAllocator mRuntimeAllocator;
    static LinearResourceAllocator mGlobalMemFileAllocator;
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
