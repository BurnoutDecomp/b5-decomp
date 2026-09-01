// Paradise-era RenderWare 4 core `rw::` type vocabulary.
// ARTIST fixes the game-facing structure and behaviour. Host pointer widening and
// native platform services are applied only where they do not change that structure.
#pragma once
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <coreallocator/icoreallocator_interface.h>
#include "rwcore_enums.h"
#include "rw/core/debug/DebugCriticalSection.h"  // canonical DebugCriticalSection (SKIP_EMIT_BODY)

// Host-layout verification is meaningful only on a 64-bit build.
#if defined(RW_VERIFY_LAYOUT) && (UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFull)
  #include <cstddef>
  #define RW_SIZE_ASSERT(T, N) static_assert(sizeof(T) == (N), #T " size")
#else
  #define RW_SIZE_ASSERT(T, N)
#endif

namespace rw {

// --- Paradise resource family ---------------------------------------------
// ARTIST constructs, copies, allocates, and frees exactly five lanes. Keeping one
// canonical count prevents host-only vendor revisions from narrowing the game ABI.
enum : uint32_t { KU_RESOURCE_LANE_COUNT = 5 };

struct BaseResourceDescriptor {  // sizeof = 8
    uint32_t m_size;       // +0
    uint32_t m_alignment;  // +4

    // The X360 out-of-line ctor @0x821F05C8 value-initialises to the
    // identity descriptor { m_size = 0, m_alignment = 1 }. Body in
    // renderware/src/rw/BaseResourceDescriptor.cpp.
    BaseResourceDescriptor();
};
RW_SIZE_ASSERT(rw::BaseResourceDescriptor, 8);

template <uint32_t Count>
struct BaseResources {
    void* m_baseResources[Count];
};

template <uint32_t Count>
struct BaseResourceDescriptors {
    BaseResourceDescriptor m_baseResourceDescriptors[Count];

    // RenderWare accumulates each sub-allocation's requirement: round this entry's
    // running size up to the other's alignment, widen to the larger alignment, then
    // add the other's size.
    BaseResourceDescriptors& operator+=(const BaseResourceDescriptors& lOther)
    {
        for (uint32_t luIndex = 0; luIndex < Count; ++luIndex)
        {
            BaseResourceDescriptor& lDescriptor = m_baseResourceDescriptors[luIndex];
            const BaseResourceDescriptor& lOtherDescriptor = lOther.m_baseResourceDescriptors[luIndex];
            if (lOtherDescriptor.m_alignment > 1)
                lDescriptor.m_size = (lOtherDescriptor.m_alignment - 1 + lDescriptor.m_size) & ~(lOtherDescriptor.m_alignment - 1);
            if (lDescriptor.m_alignment < lOtherDescriptor.m_alignment)
                lDescriptor.m_alignment = lOtherDescriptor.m_alignment;
            lDescriptor.m_size += lOtherDescriptor.m_size;
        }
        return *this;
    }
};

struct Resource : public BaseResources<KU_RESOURCE_LANE_COUNT> {
    Resource()
    {
        for (uint32_t luIndex = 0; luIndex < KU_RESOURCE_LANE_COUNT; ++luIndex)
            m_baseResources[luIndex] = nullptr;
    }
};
RW_SIZE_ASSERT(rw::Resource, 40);

struct ResourceDescriptor : public BaseResourceDescriptors<KU_RESOURCE_LANE_COUNT> {};
RW_SIZE_ASSERT(rw::ResourceDescriptor, 40);
// --------------------------------------------------------------------------

struct DefaultSystemAllocatorInitializer {  // sizeof = 1
    uint8_t _pad0[1];  // +0

    // ADDITIVE (member fn, no storage): the X360 lazy installer @0x82BBD160 that points
    // ResourceAllocatorRegistry::s_defaultAllocator at the static native system allocator
    // the first time it runs. Declaring it does not change sizeof (no data members added).
    DefaultSystemAllocatorInitializer();
};
RW_SIZE_ASSERT(rw::DefaultSystemAllocatorInitializer, 1);

struct Exception {  // sizeof = 48 on the x64 host
    void* __vftable;  // +0
    void* m_pMsg;  // +8
    void* m_pFile;  // +16
    void* m_pFunc;  // +24
    uint32_t m_Line;  // +32
    uint8_t _pad0[4];  // +36
    void* m_pWhere;  // +40
};
RW_SIZE_ASSERT(rw::Exception, 48);

struct DocException {  // sizeof = 56 on the x64 host
    ::rw::Exception field_0x0;  // +0
    uint32_t m_messageId;  // +48
    uint8_t _pad0[4];  // +52
};
RW_SIZE_ASSERT(rw::DocException, 56);

struct DocException_vtbl {  // sizeof = 8 on the x64 host
    void* _rw__DocException;  // +0
};
RW_SIZE_ASSERT(rw::DocException_vtbl, 8);

struct DocMessage {  // sizeof = 16 on the x64 host
    void* m_file;  // +0
    uint32_t m_line;  // +8
    uint8_t _pad0[4];  // +12
};
RW_SIZE_ASSERT(rw::DocMessage, 16);

struct DocMessageData {  // sizeof = 8 on the x64 host
    void* m_messageHandler;  // +0
};
RW_SIZE_ASSERT(rw::DocMessageData, 8);

struct Exception_vtbl {  // sizeof = 8 on the x64 host
    void* _rw__Exception;  // +0
};
RW_SIZE_ASSERT(rw::Exception_vtbl, 8);

struct LLLink {  // sizeof = 24 on the x64 host
    void* next;  // +0
    void* prev;  // +8
    void* data;  // +16
};
RW_SIZE_ASSERT(rw::LLLink, 24);

struct LinkListTag {  // sizeof = 24 on the x64 host
    ::rw::LLLink terminator;  // +0
};
RW_SIZE_ASSERT(rw::LinkListTag, 24);

struct FreeList {  // sizeof = 80 on the x64 host
    ::rw::LLLink link;  // +0
    uint32_t entrySize;  // +24
    uint32_t entriesPerBlock;  // +28
    uint32_t maxBlocks;  // +32
    uint32_t alignment;  // +36
    uint32_t hint;  // +40
    uint32_t heapSize;  // +44
    ::rw::LinkListTag blockList;  // +48
    uint32_t flags;  // +72
    uint8_t _pad0[4];  // +76
};
RW_SIZE_ASSERT(rw::FreeList, 80);

// ARTIST vtable off_82181118 fixes the resource-allocator method order used by every
// concrete allocator in the game:
//   [0] deleting destructor, [1] Alloc(size,name,flags,alignment,type),
//   [2] Alloc(size,name,flags), [3] Free(pointer,size), [4] DoAllocate,
//   [5] DoFree, [6] DoFreeDisposable.
// The interface is one host pointer wide. The helper bodies below are reconstructed from
// ARTIST 0x8266AC50, 0x8266ABD8, 0x82666128 and 0x826645F8.
struct IResourceAllocator : EA::Allocator::ICoreAllocator {  // one host vptr
    ~IResourceAllocator() override = default;
    void* Alloc(size_t luSize, const char* lpcName, uint32_t luFlags,
                uint32_t luAlignment, uint32_t luAlignmentOffset = 0) override;
    void* Alloc(size_t luSize, const char* lpcName, uint32_t luFlags) override;
    void Free(void* lpBlock, size_t luSizeOrFlags = 0) override;
    virtual ::rw::Resource DoAllocate(const ::rw::ResourceDescriptor& lrDescriptor,
                                      const char* lpcName);
    virtual void DoFree(const ::rw::Resource& /*lrResource*/) {}
    virtual void DoFreeDisposable(::rw::Resource& lrResource);
};
RW_SIZE_ASSERT(rw::IResourceAllocator, 8);

struct IResourceAllocator_vtbl {  // Paradise seven-slot vtable image on x64
    void* _rw__IResourceAllocator;  // +0
    void* Alloc;  // +8
    void* Alloc_2;  // +16
    void* Free;  // +24
    void* DoAllocate;  // +32
    void* DoFree;  // +40
    void* DoFreeDisposable;  // +48
};
RW_SIZE_ASSERT(rw::IResourceAllocator_vtbl, 56);

// ARTIST shows real public derivation and overrides the DoAllocate/DoFree slots.
struct LinearResourceAllocator : IResourceAllocator {  // sizeof = 176 on the x64 host
    ::rw::Resource m_heapResource;  // +8
    ::rw::ResourceDescriptor m_heapCapacity;  // +48
    ::rw::ResourceDescriptor m_currentUsage;  // +88
    ::rw::ResourceDescriptor m_paddingUsed;  // +128
    uint32_t m_numAllocations;  // +168
    uint8_t _pad0[4];  // +172

    // [PC-LEAF] reconstructed methods (bodies in renderware/src/rwcore_alloc.cpp). Non-virtual
    // (the resource code holds the concrete LinearResourceAllocator*, e.g. via AllocatorList), so
    // A linear (bump) allocator over the five Paradise resource pools:
    // each pool [m_heapResource[t], +m_heapCapacity[t].m_size) is sub-allocated by bumping
    // m_currentUsage[t]. Frees happen en masse (re-Initialize). Faithful to the X360 semantics
    // (LinearResourceAllocator::GetResourceDescriptor 0x..., Initialize, DoAllocate); the bump math
    // uses the same five-lane bump semantics with host-width pointers.
    static ResourceDescriptor* GetResourceDescriptor(ResourceDescriptor* lpOut, const ResourceDescriptor* lpIn);
    void  Initialize(const Resource& lrResource, const ResourceDescriptor& lrCapacity);
    void* Alloc(uint32_t luType, uint32_t luSize, uint32_t luAlignment);
    void  Free(void* lpBlock);

    // The vtable's DoAllocate slot ([6] +48, ?DoAllocate@LinearResourceAllocator@rw@@MEAA...
    // -- the override the PDB vtable dump above proves): carve one Resource matching the
    // descriptor by bumping each per-type pool. Consumers that only hold the abstract
    // IResourceAllocator* (TriangleCacheManager::Prepare's "CachedObjectSlots" carve) reach
    // the linear pools through this. Body in renderware/src/rwcore_alloc.cpp.
    virtual ::rw::Resource DoAllocate(const ::rw::ResourceDescriptor& lrDescriptor, const char* lpcName) override;
    virtual void DoFree(const ::rw::Resource& lrResource) override;
};
RW_SIZE_ASSERT(rw::LinearResourceAllocator, 176);

struct LinearResourceAllocator_vtbl {  // Paradise seven-slot vtable image on x64
    void* _rw__LinearResourceAllocator;  // +0
    void* Alloc;  // +8
    void* Alloc_2;  // +16
    void* Free;  // +24
    void* DoAllocate;  // +32
    void* DoFree;  // +40
    void* DoFreeDisposable;  // +48
};
RW_SIZE_ASSERT(rw::LinearResourceAllocator_vtbl, 56);

struct RGBA {  // sizeof = 4
    RGBA() : m_rgba(0) {}
    RGBA(uint8_t lu8Red, uint8_t lu8Green, uint8_t lu8Blue, uint8_t lu8Alpha);

    uint32_t m_rgba;  // +0
};
RW_SIZE_ASSERT(rw::RGBA, 4);

struct IResourceAllocator;  // fwd (defined above)

struct ResourceAllocatorRegistry {  // sizeof = 1
    uint8_t _pad0[1];  // +0

    // ADDITIVE (static -> does not affect sizeof): the process-wide default resource
    // allocator slot. rw::DefaultSystemAllocatorInitializer's ctor (X360 @0x82BBD160)
    // points this at the platform-native allocator instance the first time
    // rw::ResourceAllocatorRegistry::GetDefaultAllocator runs, if it is still null.
    static ::rw::IResourceAllocator* s_defaultAllocator;

    // ADDITIVE (static member fns, no storage -> sizeof unchanged): the registry
    // accessors. GetDefaultAllocator (X360 @0x82BBD1E0) lazily runs the
    // DefaultSystemAllocatorInitializer once (guarded) then returns the slot;
    // SetDefaultAllocator (X360 @0x82BBC490) overwrites the slot.
    static ::rw::IResourceAllocator* GetDefaultAllocator();
    static void SetDefaultAllocator(::rw::IResourceAllocator* lpAllocator);
};
RW_SIZE_ASSERT(rw::ResourceAllocatorRegistry, 1);

// ARTIST's process default is Xbox2NativeSystemAllocator. Its allocation/free
// bodies are 0x82BBCA38/0x82BBCAE8 and lane two uses XMem physical memory.
struct Xbox2NativeSystemAllocator : IResourceAllocator {
    ::rw::Resource DoAllocate(const ::rw::ResourceDescriptor& lrDescriptor,
                              const char* lpcName) override;
    void DoFree(const ::rw::Resource& lrResource) override;
};
RW_SIZE_ASSERT(rw::Xbox2NativeSystemAllocator, 8);

// PC-native default. This preserves Paradise's five-lane resource contract and
// seven-slot interface while using the host CRT for the platform allocation leaf.
struct SystemAllocatorGeneric : IResourceAllocator {
    ::rw::Resource DoAllocate(const ::rw::ResourceDescriptor& lrDescriptor,
                              const char* lpcName) override;
    void DoFree(const ::rw::Resource& lrResource) override;
};
RW_SIZE_ASSERT(rw::SystemAllocatorGeneric, 8);

struct TargetResource {
    void* m_baseResources[KU_RESOURCE_LANE_COUNT];
};
RW_SIZE_ASSERT(rw::TargetResource, 40);

struct ThrowDocException {  // sizeof = 24 on the x64 host
    void* m_file;  // +0
    void* m_function;  // +8
    uint32_t m_line;  // +16
    uint8_t _pad0[4];  // +20
};
RW_SIZE_ASSERT(rw::ThrowDocException, 24);

struct ZeroingSystemAllocatorGeneric : SystemAllocatorGeneric {
    ::rw::Resource DoAllocate(const ::rw::ResourceDescriptor& lrDescriptor,
                              const char* lpcName) override;
};
RW_SIZE_ASSERT(rw::ZeroingSystemAllocatorGeneric, 8);
}  // namespace rw
namespace rw::core {

struct IStringAllocator {  // sizeof = 8 on the x64 host
    void* __vftable;  // +0
};
RW_SIZE_ASSERT(rw::core::IStringAllocator, 8);

struct IStringAllocator_vtbl {  // sizeof = 80 on the x64 host
    void* Modify;  // +0
    void* Allocate;  // +8
    void* Deallocate;  // +16
    void* GetAllocatorType;  // +24
    void* Assign;  // +32
    void* Compare;  // +40
    void* SetLength;  // +48
    void* GetLength;  // +56
    void* GetCapacity;  // +64
    void* _rw__core__IStringAllocator;  // +72
};
RW_SIZE_ASSERT(rw::core::IStringAllocator_vtbl, 80);

struct IWStringAllocator {  // sizeof = 8 on the x64 host
    void* __vftable;  // +0
};
RW_SIZE_ASSERT(rw::core::IWStringAllocator, 8);

struct IWStringAllocator_vtbl {  // sizeof = 80 on the x64 host
    void* Modify;  // +0
    void* Allocate;  // +8
    void* Deallocate;  // +16
    void* GetAllocatorType;  // +24
    void* Assign;  // +32
    void* Compare;  // +40
    void* SetLength;  // +48
    void* GetLength;  // +56
    void* GetCapacity;  // +64
    void* _rw__core__IWStringAllocator;  // +72
};
RW_SIZE_ASSERT(rw::core::IWStringAllocator_vtbl, 80);

struct String {  // sizeof = 16 on the x64 host
    void* mAllocator;  // +0
    void* mString;  // +8
};
RW_SIZE_ASSERT(rw::core::String, 16);

// vtable ??_7StringConstAllocator@core@rw@@6B@ @ 000131a8 (9 entries):
//   [ 0] +0   ?Modify@StringConstAllocator@core@rw@@UEAAXPEAPEAD@Z
//   [ 1] +8   ?Allocate@StringConstAllocator@core@rw@@UEAAPEAD_K@Z
//   [ 2] +16  ?Deallocate@StringConstAllocator@core@rw@@UEAAXPEAD@Z
//   [ 3] +24  ?GetAllocatorType@StringConstAllocator@core@rw@@UEBAIXZ
//   [ 4] +32  ?Assign@StringConstAllocator@core@rw@@UEAAXAEAVString@23@PEAPEADAEBV423@PEAD@Z
//   [ 5] +40  ?Compare@StringConstAllocator@core@rw@@UEAAHPEBD0@Z
//   [ 6] +48  ?SetLength@StringConstAllocator@core@rw@@UEAAXPEAD_K@Z
//   [ 7] +56  ?GetLength@StringConstAllocator@core@rw@@UEBA_KPEBD@Z
//   [ 8] +64  ?GetCapacity@StringConstAllocator@core@rw@@UEBA_KPEBD@Z
struct StringConstAllocator {  // sizeof = 8 on the x64 host
    ::rw::core::IStringAllocator field_0x0;  // +0
};
RW_SIZE_ASSERT(rw::core::StringConstAllocator, 8);

struct StringConstAllocator_vtbl {  // sizeof = 80 on the x64 host
    void* Modify;  // +0
    void* Allocate;  // +8
    void* Deallocate;  // +16
    void* GetAllocatorType;  // +24
    void* Assign;  // +32
    void* Compare;  // +40
    void* SetLength;  // +48
    void* GetLength;  // +56
    void* GetCapacity;  // +64
    void* _rw__core__StringConstAllocator;  // +72
};
RW_SIZE_ASSERT(rw::core::StringConstAllocator_vtbl, 80);

// vtable ??_7StringCoreAllocator@core@rw@@6B@ @ 00013108 (9 entries):
//   [ 0] +0   ?Modify@StringCoreAllocator@core@rw@@UEAAXPEAPEAD@Z
//   [ 1] +8   ?Allocate@StringCoreAllocator@core@rw@@UEAAPEAD_K@Z
//   [ 2] +16  ?Deallocate@StringCoreAllocator@core@rw@@UEAAXPEAD@Z
//   [ 3] +24  ?GetAllocatorType@StringCoreAllocator@core@rw@@UEBAIXZ
//   [ 4] +32  ?Assign@StringCoreAllocator@core@rw@@UEAAXAEAVString@23@PEAPEADAEBV423@PEAD@Z
//   [ 5] +40  ?Compare@StringCoreAllocator@core@rw@@UEAAHPEBD0@Z
//   [ 6] +48  ?SetLength@StringCoreAllocator@core@rw@@UEAAXPEAD_K@Z
//   [ 7] +56  ?GetLength@StringCoreAllocator@core@rw@@UEBA_KPEBD@Z
//   [ 8] +64  ?GetCapacity@StringCoreAllocator@core@rw@@UEBA_KPEBD@Z
struct StringCoreAllocator {  // sizeof = 16 on the x64 host
    ::rw::core::IStringAllocator field_0x0;  // +0
    void* mCoreAllocator;  // +8
};
RW_SIZE_ASSERT(rw::core::StringCoreAllocator, 16);

struct StringCoreAllocator_vtbl {  // sizeof = 80 on the x64 host
    void* Modify;  // +0
    void* Allocate;  // +8
    void* Deallocate;  // +16
    void* GetAllocatorType;  // +24
    void* Assign;  // +32
    void* Compare;  // +40
    void* SetLength;  // +48
    void* GetLength;  // +56
    void* GetCapacity;  // +64
    void* _rw__core__StringCoreAllocator;  // +72
};
RW_SIZE_ASSERT(rw::core::StringCoreAllocator_vtbl, 80);

// vtable ??_7StringFixedAllocator@core@rw@@6B@ @ 000131f8 (9 entries):
//   [ 0] +0   ?Modify@StringFixedAllocator@core@rw@@UEAAXPEAPEAD@Z
//   [ 1] +8   ?Allocate@StringFixedAllocator@core@rw@@UEAAPEAD_K@Z
//   [ 2] +16  ?Deallocate@StringFixedAllocator@core@rw@@UEAAXPEAD@Z
//   [ 3] +24  ?GetAllocatorType@StringFixedAllocator@core@rw@@UEBAIXZ
//   [ 4] +32  ?Assign@StringFixedAllocator@core@rw@@UEAAXAEAVString@23@PEAPEADAEBV423@PEAD@Z
//   [ 5] +40  ?Compare@StringFixedAllocator@core@rw@@UEAAHPEBD0@Z
//   [ 6] +48  ?SetLength@StringFixedAllocator@core@rw@@UEAAXPEAD_K@Z
//   [ 7] +56  ?GetLength@StringFixedAllocator@core@rw@@UEBA_KPEBD@Z
//   [ 8] +64  ?GetCapacity@StringFixedAllocator@core@rw@@UEBA_KPEBD@Z
struct StringFixedAllocator {  // sizeof = 8 on the x64 host
    ::rw::core::IStringAllocator field_0x0;  // +0
};
RW_SIZE_ASSERT(rw::core::StringFixedAllocator, 8);

struct StringFixedAllocator_vtbl {  // sizeof = 80 on the x64 host
    void* Modify;  // +0
    void* Allocate;  // +8
    void* Deallocate;  // +16
    void* GetAllocatorType;  // +24
    void* Assign;  // +32
    void* Compare;  // +40
    void* SetLength;  // +48
    void* GetLength;  // +56
    void* GetCapacity;  // +64
    void* _rw__core__StringFixedAllocator;  // +72
};
RW_SIZE_ASSERT(rw::core::StringFixedAllocator_vtbl, 80);

// vtable ??_7StringRefAllocator@core@rw@@6B@ @ 00013158 (9 entries):
//   [ 0] +0   ?Modify@StringRefAllocator@core@rw@@UEAAXPEAPEAD@Z
//   [ 1] +8   ?Allocate@StringRefAllocator@core@rw@@UEAAPEAD_K@Z
//   [ 2] +16  ?Deallocate@StringRefAllocator@core@rw@@UEAAXPEAD@Z
//   [ 3] +24  ?GetAllocatorType@StringRefAllocator@core@rw@@UEBAIXZ
//   [ 4] +32  ?Assign@StringRefAllocator@core@rw@@UEAAXAEAVString@23@PEAPEADAEBV423@PEAD@Z
//   [ 5] +40  ?Compare@StringRefAllocator@core@rw@@UEAAHPEBD0@Z
//   [ 6] +48  ?SetLength@StringRefAllocator@core@rw@@UEAAXPEAD_K@Z
//   [ 7] +56  ?GetLength@StringRefAllocator@core@rw@@UEBA_KPEBD@Z
//   [ 8] +64  ?GetCapacity@StringRefAllocator@core@rw@@UEBA_KPEBD@Z
struct StringRefAllocator {  // sizeof = 16 on the x64 host
    ::rw::core::IStringAllocator field_0x0;  // +0
    void* mCoreAllocator;  // +8
};
RW_SIZE_ASSERT(rw::core::StringRefAllocator, 16);

struct StringRefAllocator_vtbl {  // sizeof = 80 on the x64 host
    void* Modify;  // +0
    void* Allocate;  // +8
    void* Deallocate;  // +16
    void* GetAllocatorType;  // +24
    void* Assign;  // +32
    void* Compare;  // +40
    void* SetLength;  // +48
    void* GetLength;  // +56
    void* GetCapacity;  // +64
    void* _rw__core__StringRefAllocator;  // +72
};
RW_SIZE_ASSERT(rw::core::StringRefAllocator_vtbl, 80);

struct WString {  // sizeof = 16 on the x64 host
    void* mAllocator;  // +0
    void* mWString;  // +8
};
RW_SIZE_ASSERT(rw::core::WString, 16);

// vtable ??_7WStringConstAllocator@core@rw@@6B@ @ 00013338 (9 entries):
//   [ 0] +0   ?Modify@WStringConstAllocator@core@rw@@UEAAXPEAPEA_W@Z
//   [ 1] +8   ?Allocate@WStringConstAllocator@core@rw@@UEAAPEA_W_K@Z
//   [ 2] +16  ?Deallocate@WStringConstAllocator@core@rw@@UEAAXPEA_W@Z
//   [ 3] +24  ?GetAllocatorType@WStringConstAllocator@core@rw@@UEBAIXZ
//   [ 4] +32  ?Assign@WStringConstAllocator@core@rw@@UEAAXAEAVWString@23@PEAPEA_WAEBV423@PEA_W@Z
//   [ 5] +40  ?Compare@WStringConstAllocator@core@rw@@UEAAHPEB_W0@Z
//   [ 6] +48  ?SetLength@WStringConstAllocator@core@rw@@UEAAXPEA_W_K@Z
//   [ 7] +56  ?GetLength@WStringConstAllocator@core@rw@@UEBA_KPEB_W@Z
//   [ 8] +64  ?GetCapacity@WStringConstAllocator@core@rw@@UEBA_KPEB_W@Z
struct WStringConstAllocator {  // sizeof = 8 on the x64 host
    ::rw::core::IWStringAllocator field_0x0;  // +0
};
RW_SIZE_ASSERT(rw::core::WStringConstAllocator, 8);

struct WStringConstAllocator_vtbl {  // sizeof = 80 on the x64 host
    void* Modify;  // +0
    void* Allocate;  // +8
    void* Deallocate;  // +16
    void* GetAllocatorType;  // +24
    void* Assign;  // +32
    void* Compare;  // +40
    void* SetLength;  // +48
    void* GetLength;  // +56
    void* GetCapacity;  // +64
    void* _rw__core__WStringConstAllocator;  // +72
};
RW_SIZE_ASSERT(rw::core::WStringConstAllocator_vtbl, 80);

// vtable ??_7WStringCoreAllocator@core@rw@@6B@ @ 00013298 (9 entries):
//   [ 0] +0   ?Modify@WStringCoreAllocator@core@rw@@UEAAXPEAPEA_W@Z
//   [ 1] +8   ?Allocate@WStringCoreAllocator@core@rw@@UEAAPEA_W_K@Z
//   [ 2] +16  ?Deallocate@WStringCoreAllocator@core@rw@@UEAAXPEA_W@Z
//   [ 3] +24  ?GetAllocatorType@WStringCoreAllocator@core@rw@@UEBAIXZ
//   [ 4] +32  ?Assign@WStringCoreAllocator@core@rw@@UEAAXAEAVWString@23@PEAPEA_WAEBV423@PEA_W@Z
//   [ 5] +40  ?Compare@WStringCoreAllocator@core@rw@@UEAAHPEB_W0@Z
//   [ 6] +48  ?SetLength@WStringCoreAllocator@core@rw@@UEAAXPEA_W_K@Z
//   [ 7] +56  ?GetLength@WStringCoreAllocator@core@rw@@UEBA_KPEB_W@Z
//   [ 8] +64  ?GetCapacity@WStringCoreAllocator@core@rw@@UEBA_KPEB_W@Z
struct WStringCoreAllocator {  // sizeof = 16 on the x64 host
    ::rw::core::IWStringAllocator field_0x0;  // +0
    void* mCoreAllocator;  // +8
};
RW_SIZE_ASSERT(rw::core::WStringCoreAllocator, 16);

struct WStringCoreAllocator_vtbl {  // sizeof = 80 on the x64 host
    void* Modify;  // +0
    void* Allocate;  // +8
    void* Deallocate;  // +16
    void* GetAllocatorType;  // +24
    void* Assign;  // +32
    void* Compare;  // +40
    void* SetLength;  // +48
    void* GetLength;  // +56
    void* GetCapacity;  // +64
    void* _rw__core__WStringCoreAllocator;  // +72
};
RW_SIZE_ASSERT(rw::core::WStringCoreAllocator_vtbl, 80);

// vtable ??_7WStringFixedAllocator@core@rw@@6B@ @ 00013388 (9 entries):
//   [ 0] +0   ?Modify@WStringFixedAllocator@core@rw@@UEAAXPEAPEA_W@Z
//   [ 1] +8   ?Allocate@WStringFixedAllocator@core@rw@@UEAAPEA_W_K@Z
//   [ 2] +16  ?Deallocate@WStringFixedAllocator@core@rw@@UEAAXPEA_W@Z
//   [ 3] +24  ?GetAllocatorType@WStringFixedAllocator@core@rw@@UEBAIXZ
//   [ 4] +32  ?Assign@WStringFixedAllocator@core@rw@@UEAAXAEAVWString@23@PEAPEA_WAEBV423@PEA_W@Z
//   [ 5] +40  ?Compare@WStringFixedAllocator@core@rw@@UEAAHPEB_W0@Z
//   [ 6] +48  ?SetLength@WStringFixedAllocator@core@rw@@UEAAXPEA_W_K@Z
//   [ 7] +56  ?GetLength@WStringFixedAllocator@core@rw@@UEBA_KPEB_W@Z
//   [ 8] +64  ?GetCapacity@WStringFixedAllocator@core@rw@@UEBA_KPEB_W@Z
struct WStringFixedAllocator {  // sizeof = 8 on the x64 host
    ::rw::core::IWStringAllocator field_0x0;  // +0
};
RW_SIZE_ASSERT(rw::core::WStringFixedAllocator, 8);

struct WStringFixedAllocator_vtbl {  // sizeof = 80 on the x64 host
    void* Modify;  // +0
    void* Allocate;  // +8
    void* Deallocate;  // +16
    void* GetAllocatorType;  // +24
    void* Assign;  // +32
    void* Compare;  // +40
    void* SetLength;  // +48
    void* GetLength;  // +56
    void* GetCapacity;  // +64
    void* _rw__core__WStringFixedAllocator;  // +72
};
RW_SIZE_ASSERT(rw::core::WStringFixedAllocator_vtbl, 80);

// vtable ??_7WStringRefAllocator@core@rw@@6B@ @ 000132e8 (9 entries):
//   [ 0] +0   ?Modify@WStringRefAllocator@core@rw@@UEAAXPEAPEA_W@Z
//   [ 1] +8   ?Allocate@WStringRefAllocator@core@rw@@UEAAPEA_W_K@Z
//   [ 2] +16  ?Deallocate@WStringRefAllocator@core@rw@@UEAAXPEA_W@Z
//   [ 3] +24  ?GetAllocatorType@WStringRefAllocator@core@rw@@UEBAIXZ
//   [ 4] +32  ?Assign@WStringRefAllocator@core@rw@@UEAAXAEAVWString@23@PEAPEA_WAEBV423@PEA_W@Z
//   [ 5] +40  ?Compare@WStringRefAllocator@core@rw@@UEAAHPEB_W0@Z
//   [ 6] +48  ?SetLength@WStringRefAllocator@core@rw@@UEAAXPEA_W_K@Z
//   [ 7] +56  ?GetLength@WStringRefAllocator@core@rw@@UEBA_KPEB_W@Z
//   [ 8] +64  ?GetCapacity@WStringRefAllocator@core@rw@@UEBA_KPEB_W@Z
struct WStringRefAllocator {  // sizeof = 16 on the x64 host
    ::rw::core::IWStringAllocator field_0x0;  // +0
    void* mCoreAllocator;  // +8
};
RW_SIZE_ASSERT(rw::core::WStringRefAllocator, 16);

struct WStringRefAllocator_vtbl {  // sizeof = 80 on the x64 host
    void* Modify;  // +0
    void* Allocate;  // +8
    void* Deallocate;  // +16
    void* GetAllocatorType;  // +24
    void* Assign;  // +32
    void* Compare;  // +40
    void* SetLength;  // +48
    void* GetLength;  // +56
    void* GetCapacity;  // +64
    void* _rw__core__WStringRefAllocator;  // +72
};
RW_SIZE_ASSERT(rw::core::WStringRefAllocator_vtbl, 80);
}  // namespace rw::core
namespace rw::core::debug {

struct PrintOpts {  // sizeof = 6
    uint8_t mDate;  // +0
    uint8_t mTime;  // +1
    uint8_t mFile;  // +2
    uint8_t mLine;  // +3
    uint8_t mName;  // +4
    uint8_t mNewLine;  // +5
};
RW_SIZE_ASSERT(rw::core::debug::PrintOpts, 6);
}  // namespace rw::core::debug
namespace rw::core::debug::detail {

// DebugCriticalSection is SKIP_EMIT_BODY: the canonical hand-maintained wrapper (with
// Create/Enter/Leave) lives in rw/core/debug/DebugCriticalSection.h (#included above);
// its x64 layout matches (int miInitialised + CRITICAL_SECTION = 44 bytes). Emitting the
// PDB-derived opaque body here too would be an ODR redefinition (C2011).
RW_SIZE_ASSERT(rw::core::debug::detail::DebugCriticalSection, 44);
}  // namespace rw::core::debug::detail
namespace rw::core::debug {

struct Channel {  // sizeof = 240 on the x64 host
    uint32_t mDeviceCount;  // +0
    uint32_t mIndex;  // +4
    uint8_t mEnabled;  // +8
    uint8_t mHaveInfo;  // +9
    uint8_t mName[32];  // +10
    uint8_t _pad0[6];  // +42
    void* mDeviceList[16];  // +48
    void* mFileName;  // +176
    uint32_t mLine;  // +184
    ::rw::core::debug::detail::DebugCriticalSection mCriticalSection;  // +188
    ::rw::core::debug::PrintOpts mPrintOpts;  // +232
    uint8_t _pad1[2];  // +238
};
RW_SIZE_ASSERT(rw::core::debug::Channel, 240);

// vtable ??_7Device@debug@core@rw@@6B@ @ 00012ab0 (2 entries):
//   [ 1] +8   ?OutputData@Device@debug@core@rw@@UEAAXPEBX_K@Z
//   [ 2] +16  ?Flush@Device@debug@core@rw@@UEAAXXZ
struct Device {  // sizeof = 112 on the x64 host
    void* __vftable;  // +0
    uint8_t mEnabled;  // +8
    uint8_t _pad0[7];  // +9
    void* mFormatter;  // +16
    void* mMsgBuffer;  // +24
    uint64_t mMsgBufLen;  // +32
    void* mOutBuffer;  // +40
    void* mOutBufP;  // +48
    uint64_t mOutBufLen;  // +56
    ::rw::core::debug::detail::DebugCriticalSection mCriticalSection;  // +64
    uint8_t _pad1[4];  // +108
};
RW_SIZE_ASSERT(rw::core::debug::Device, 112);

struct ConsoleDevice {  // sizeof = 112 on the x64 host
    ::rw::core::debug::Device field_0x0;  // +0
};
RW_SIZE_ASSERT(rw::core::debug::ConsoleDevice, 112);

struct ConsoleDevice_vtbl {  // sizeof = 32 on the x64 host
    void* _rw__core__debug__ConsoleDevice;  // +0
    void* OutputData;  // +8
    void* Flush;  // +16
    void* Output;  // +24
};
RW_SIZE_ASSERT(rw::core::debug::ConsoleDevice_vtbl, 32);

struct Device_vtbl {  // sizeof = 32 on the x64 host
    void* _rw__core__debug__Device;  // +0
    void* OutputData;  // +8
    void* Flush;  // +16
    void* Output;  // +24
};
RW_SIZE_ASSERT(rw::core::debug::Device_vtbl, 32);

// vtable ??_7FileDevice@debug@core@rw@@6B@ @ 00012fc8 (3 entries):
//   [ 1] +8   ?OutputData@Device@debug@core@rw@@UEAAXPEBX_K@Z
//   [ 2] +16  ?Flush@FileDevice@debug@core@rw@@UEAAXXZ
//   [ 3] +24  ?Output@FileDevice@debug@core@rw@@EEAAXPEBX_K@Z
struct FileDevice {  // sizeof = 120 on the x64 host
    ::rw::core::debug::Device field_0x0;  // +0
    uint32_t mHandle;  // +112
    uint8_t _pad0[4];  // +116
};
RW_SIZE_ASSERT(rw::core::debug::FileDevice, 120);

struct FileDevice_vtbl {  // sizeof = 32 on the x64 host
    void* _rw__core__debug__FileDevice;  // +0
    void* OutputData;  // +8
    void* Flush;  // +16
    void* Output;  // +24
};
RW_SIZE_ASSERT(rw::core::debug::FileDevice_vtbl, 32);

struct IFormatter {  // sizeof = 8 (retained x64 symbol export)
    virtual bool Format(char* lpBuffer, size_t luBufferLength, ::rw::core::debug::Channel* lpChannel,
                        const char* lpcFormat, va_list lArgs) = 0;
    virtual ~IFormatter() {}

    static void* operator new(size_t luSize);
    static void  operator delete(void* lpBlock);
};
RW_SIZE_ASSERT(rw::core::debug::IFormatter, 8);

// vtable ??_7Formatter@debug@core@rw@@6B@ @ 00012ad0 (1 entries):
//   [ 0] +0   ?Format@Formatter@debug@core@rw@@UEAA_NPEAD_KPEAVChannel@234@PEBD0@Z
struct Formatter : IFormatter {  // sizeof = 8 (retained x64 symbol export)
    bool Format(char* lpBuffer, size_t luBufferLength, ::rw::core::debug::Channel* lpChannel,
                const char* lpcFormat, va_list lArgs) override;
    ~Formatter() override = default;
};
RW_SIZE_ASSERT(rw::core::debug::Formatter, 8);

struct Formatter_vtbl {  // sizeof = 16 on the x64 host
    void* Format;  // +0
    void* _rw__core__debug__Formatter;  // +8
};
RW_SIZE_ASSERT(rw::core::debug::Formatter_vtbl, 16);

struct IFormatter_vtbl {  // sizeof = 16 on the x64 host
    void* Format;  // +0
    void* _rw__core__debug__IFormatter;  // +8
};
RW_SIZE_ASSERT(rw::core::debug::IFormatter_vtbl, 16);

struct Manager {  // sizeof = 1
    uint8_t _pad0[1];  // +0

    // ADDITIVE (static member fn, no storage -> sizeof unchanged): the debug-subsystem
    // bring-up entry point (X360 rw::core::debug::Manager::CreateInstance @0x82BBD380).
    // Records the supplied resource allocator as the subsystem allocator, then constructs
    // the singleton IFormatter through it. Body in renderware/src/rw/core/debug/Manager.cpp.
    static int CreateInstance(::rw::IResourceAllocator* lpAllocator);
};
RW_SIZE_ASSERT(rw::core::debug::Manager, 1);
}  // namespace rw::core::debug
namespace rw {

struct int128_t {  // sizeof = 16
    uint64_t m_low;  // +0
    uint64_t m_high;  // +8
};
RW_SIZE_ASSERT(rw::int128_t, 16);
}  // namespace rw
namespace rw::internal::memory {

struct Profile {  // sizeof = 1
    uint8_t _pad0[1];  // +0
};
RW_SIZE_ASSERT(rw::internal::memory::Profile, 1);
}  // namespace rw::internal::memory
namespace rw {

struct rwDebugPrintHelper {  // sizeof = 1024
    uint8_t m_buf[1024];  // +0
};
RW_SIZE_ASSERT(rw::rwDebugPrintHelper, 1024);
}  // namespace rw
namespace rw::shared_globals::internal {

struct AutoinitOSGlobalManager {  // sizeof = 1
    uint8_t _pad0[1];  // +0
};
RW_SIZE_ASSERT(rw::shared_globals::internal::AutoinitOSGlobalManager, 1);

struct OSGlobalManager {  // sizeof = 72 on the x64 host
    uint8_t mOSGlobalList[24];  // +0  was: rw::shared_globals::internal::OSGlobalManager::OSGlobalList
    uint8_t mRefCount[4];  // +24  was: EA::Thread::AtomicInt<unsigned int>
    uint8_t _pad0[4];  // +28
    uint8_t mcsLock[40];  // +32  was: Win32Mutex

    // ADDITIVE (member fn, no storage): the X360 ctor @ 0x82BBC878 that the
    // boot trace executes. Declaring it does not change sizeof (no data members
    // added). Body in renderware/src/rw/OSGlobalManager.cpp. The X360 ctor makes
    // mOSGlobalList an empty circular intrusive list (next=prev=this), zeroes the
    // atomic mRefCount, then RtlInitializeCriticalSection(&mcsLock).
    OSGlobalManager();
};
RW_SIZE_ASSERT(rw::shared_globals::internal::OSGlobalManager, 72);

struct OSGlobalNode {  // sizeof = 24 on the x64 host
    void* mpNext;  // +0
    void* mpPrev;  // +8
    uint32_t mOSGlobalID;  // +16
    uint32_t mOSGlobalRefCount;  // +20
};
RW_SIZE_ASSERT(rw::shared_globals::internal::OSGlobalNode, 24);
}  // namespace rw::shared_globals::internal
namespace rw {

struct uint128_t {  // sizeof = 16
    uint64_t m_low;  // +0
    uint64_t m_high;  // +8
};
RW_SIZE_ASSERT(rw::uint128_t, 16);
}  // namespace rw

#undef RW_SIZE_ASSERT
