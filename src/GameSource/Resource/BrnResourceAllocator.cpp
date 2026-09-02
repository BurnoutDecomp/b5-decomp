#include "GameSource/Resource/BrnResourceAllocator.h"
#include "ppmalloc/EAGeneralAllocator.h"   // the backing EA general allocator
#include "rw/rwcore_general_alloc.h"       // rw::core::GeneralResourceAllocator (GetGameDataGeneralAllocator)
#include "GameShared/GameClasses/Memory/PC/CgsLowMemoryPC.h"   // LowMemory::Reserve (low-4GB root block)
#include <new>   // std::nothrow (the GlobalGraphics arena carve)

// BrnResource::Allocators / HeapResourceAllocator - the engine's root debug resource allocator,
// the heap the whole GameData resource system (ConstructResourceModule -> ResourceModule::Construct)
// carves its module/bank memory from, and the one ICE::ICEMemory::Construct allocates its debug pool
// from.
//
// [PC-LEAF, consistent with the allocator-layer decision]: on X360 this is
// Allocators::mGlobalDebugAllocator - an rw allocator over a memory region the boot path reserves;
// here it is a big malloc-backed region with an EA::Allocator::GeneralAllocator over it. The backing
// block is allocated lazily on first use; mpInternalDebugAllocator is published non-null at static
// init (it points at the allocator object) so the ICE null-check passes regardless of init order.
namespace BrnResource
{
    namespace
    {
        // 768 MB root debug heap. [PC-LEAF] the X360 reserves a fixed ~smaller budget split across banks;
        // the PC bring-up routes the 27 pools' memory (~266 MB) through the MemoryModule's root banks (a
        // ~384 MB type0+type1 backing carved from here), with a debug-heap FALLBACK if a bank carve fails
        // (another ~266 MB) -- so this is sized to hold the bank backing + a full fallback at once.
        const size_t KN_DEBUG_HEAP_SIZE = 768u * 1024u * 1024u;

        EA::Allocator::GeneralAllocator s_DebugGeneralAllocator;   // ctor -> Init (empty) at static init
        HeapResourceAllocator           s_DebugResourceAllocator;
        bool                            s_bDebugHeapBacked = false;

        // Lazily reserve + adopt the backing block (avoids the reservation at static init).
        //
        // FLAG PC-platform leaf: the block MUST live below 4 GB. Everything the resource
        // system touches is carved from this one root -- the MemoryModule root banks and
        // all 27 pools (so every bnd2 resource payload and its in-place FixUp), plus all
        // 24 memory-map allocators (so the AttribSys package heap: every Attrib::Alloc,
        // the schema.vlt/.bin blobs and the Attrib vault's PtrN pointer fixups). The
        // shipped serialised format stores those pointers in FOUR-BYTE slots, read back
        // with the committed PointerFromU32 idiom; on the 32-bit X360 that is free, on
        // x64 it only round-trips when the pointee is under 4 GB. The plain CRT malloc
        // this used to be handed out ~0x21e_00000000 on Windows x64, so the first fixed-up
        // slot dereference faulted (strlen on Attrib's truncated typenames pointer).
        // See GameShared/GameClasses/Memory/PC/CgsLowMemoryPC.h.
        void EnsureDebugHeapBacked()
        {
            if (s_bDebugHeapBacked)
                return;
            void* lpBlock = CgsMemory::LowMemory::Reserve(KN_DEBUG_HEAP_SIZE);
            if (lpBlock)
                s_DebugGeneralAllocator.AddCore(lpBlock, KN_DEBUG_HEAP_SIZE);
            s_bDebugHeapBacked = true;
        }
    }

    // Published non-null at static init; points at the root HeapResourceAllocator (which IS-A
    // rw::IResourceAllocator) so the resource-module Construct chain can carve memory through its
    // virtual DoAllocate. The backing 64 MB block is adopted lazily on first Allocate. (ICE null-checks
    // this; the resource modules cast it to rw::IResourceAllocator* and call DoAllocate.)
    void* Allocators::mpInternalDebugAllocator = static_cast<rw::IResourceAllocator*>(&s_DebugResourceAllocator);

    HeapResourceAllocator* GetDebugAllocator()
    {
        EnsureDebugHeapBacked();
        return &s_DebugResourceAllocator;
    }

    namespace
    {
        // The single GameData general allocator (allocator-gate leaf). 64 MB carved from the debug
        // heap, lazily on first use. [PC-LEAF] stands in for the per-bank rw general allocators the
        // X360's GameDataModule::CreateAllocators builds from the memory map; the loading-screen
        // module loads (e.g. SoundModule) carve their module memory from it.
        const size_t                       KN_GAMEDATA_GENERAL_SIZE = 64u * 1024u * 1024u;
        rw::core::GeneralResourceAllocator s_GameDataGeneralAllocator;   // EA allocator members ctor at static init
        bool                               s_bGameDataGeneralInit = false;
    }

    rw::core::GeneralResourceAllocator* GetGameDataGeneralAllocator()
    {
        if (!s_bGameDataGeneralInit)
        {
            EnsureDebugHeapBacked();
            void* lpRegion = s_DebugGeneralAllocator.MallocAligned(KN_GAMEDATA_GENERAL_SIZE, 16);
            if (lpRegion)
            {
                rw::Resource lResource;
                for (uint32_t lu = 0; lu < rw::KU_RESOURCE_LANE_COUNT; ++lu)
                    lResource.m_baseResources[lu] = 0;
                lResource.m_baseResources[0] = lpRegion;

                rw::ResourceDescriptor lCapacity;
                for (uint32_t lu = 0; lu < rw::KU_RESOURCE_LANE_COUNT; ++lu)
                {
                    lCapacity.m_baseResourceDescriptors[lu].m_size      = 0;
                    lCapacity.m_baseResourceDescriptors[lu].m_alignment = 1;
                }
                lCapacity.m_baseResourceDescriptors[0].m_size      = KN_GAMEDATA_GENERAL_SIZE;
                lCapacity.m_baseResourceDescriptors[0].m_alignment = 16;

                s_GameDataGeneralAllocator.Initialize(lResource, lCapacity);
            }
            s_bGameDataGeneralInit = true;
        }
        return &s_GameDataGeneralAllocator;
    }

    // Out-of-line defaulted destructor: anchors the DefaultLinearAllocator vtable in this TU so
    // the compiler emits its polymorphic-delete thunk (the X360 `scalar deleting destructor'
    // @0x82666948). The underlying object teardown is trivial (only the rw::IResourceAllocator
    // base) -- the X360 thunk just rewrites the vptr and conditionally operator-deletes.
    DefaultLinearAllocator::~DefaultLinearAllocator() = default;

    // rw::IResourceAllocator virtual entry point -> forward to Allocate.
    rw::Resource HeapResourceAllocator::DoAllocate(const rw::ResourceDescriptor& lDescriptor, const char* lpcName)
    {
        return Allocate(lDescriptor, lpcName);
    }

    // Carve a resource matching lDescriptor from the root debug heap. The returned rw::Resource's
    // m_baseResources[0] is the memory block (the X360 DoAllocate fills the per-pool base pointers;
    // PC leaf serves pool 0 from the general heap).
    rw::Resource HeapResourceAllocator::Allocate(const rw::ResourceDescriptor& lDescriptor, const char* /*lpcName*/)
    {
        EnsureDebugHeapBacked();
        rw::Resource lResource;
        for (uint32_t lu = 0; lu < rw::KU_RESOURCE_LANE_COUNT; ++lu)
            lResource.m_baseResources[lu] = 0;

        const uint32_t luSize  = lDescriptor.m_baseResourceDescriptors[0].m_size;
        uint32_t       luAlign = lDescriptor.m_baseResourceDescriptors[0].m_alignment;
        if (luAlign < 16)
            luAlign = 16;
        if (luSize)
            lResource.m_baseResources[0] = s_DebugGeneralAllocator.MallocAligned(luSize, luAlign);
        return lResource;
    }

    // =====================================================================================
    // GetGlobalGraphicsAllocator -- the process-wide "GlobalGraphics" linear resource
    // allocator (X360 object @ dword_82F2C814). ADDITIVE 2026-09-02 (tyre-mark wave):
    // BrnResourceAllocator.h has declared it since the shadow wave, BrnRendererMemory.cpp's
    // banner has called it out as "declaration-only, defined nowhere" ever since, and the
    // effects link is the first consumer that cannot proceed without it (LNK2019, measured).
    //
    // ⚠ FLAG PC-platform leaf. The console builds this object in
    // Allocators::ConstructGlobalGraphicsMemory @0x82666B70 over an XPhysicalAlloc'd region --
    // physically contiguous, GPU-visible memory sized from the X360 memory map
    // (Allocators::InitMemoryMap @0x82664728). Neither exists on the host and neither is
    // reconstructed. What every CALLER needs is the abstract rw::IResourceAllocator contract
    // (DoAllocate hands back a lane-0 block), so this supplies exactly that over one host heap
    // block -- the SAME shape BrnRendererModule.cpp's `sWorldDispatchAllocator` already uses for
    // the sky dome's buffers. A platform substitution, not a stand-in for game logic.
    //
    // WHO TAKES IT: ParticleModule::Prepare @0x8229BEA0 hands `off_82F2C814` to all five
    // contained Im3d renderers; the one that matters for the tyre mark, Im3dSkidsRenderer::
    // Construct @0x82295150, allocates the skid vertex/pixel ProgramBuffers through it (456 +
    // 228 bytes of microcode plus their headers). 256 KB leaves room for the rest of that family
    // as it lands; the arena is carved lazily, so a build that never touches effects pays nothing.
    // =====================================================================================
    namespace
    {
        rw::LinearResourceAllocator sGlobalGraphicsAllocator;
        bool                        sbGlobalGraphicsReady = false;
        const uint32_t              KU_GLOBAL_GRAPHICS_ARENA_BYTES = 256u * 1024u;
    }

    rw::IResourceAllocator* Allocators::GetGlobalGraphicsAllocator()
    {
        if (sbGlobalGraphicsReady)
            return &sGlobalGraphicsAllocator;

        void* lpHeap = ::operator new(KU_GLOBAL_GRAPHICS_ARENA_BYTES, std::nothrow);
        if (lpHeap == 0)
            return 0;

        rw::Resource           lHeapResource;
        rw::ResourceDescriptor lHeapCapacity;
        for (uint32_t luLane = 0; luLane < rw::KU_RESOURCE_LANE_COUNT; ++luLane)
        {
            lHeapResource.m_baseResources[luLane] = (luLane == 0) ? lpHeap : 0;
            lHeapCapacity.m_baseResourceDescriptors[luLane].m_size =
                (luLane == 0) ? KU_GLOBAL_GRAPHICS_ARENA_BYTES : 0u;
            lHeapCapacity.m_baseResourceDescriptors[luLane].m_alignment = (luLane == 0) ? 16u : 1u;
        }
        sGlobalGraphicsAllocator.Initialize(lHeapResource, lHeapCapacity);

        sbGlobalGraphicsReady = true;
        return &sGlobalGraphicsAllocator;
    }
}
