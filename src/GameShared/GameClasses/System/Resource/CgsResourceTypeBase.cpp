#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource, rw::BaseResourceDescriptors<5> (= ResourceDescriptor)

// Default bodies for the CgsResource::Type polymorphic base. Concrete handlers (RwRaster, Font, ...)
// override the virtuals they implement; whatever they leave alone falls through to these no-op /
// identity defaults. The X360 caches CanDefrag()+GetTypeID() into mbCachedCanDefrag/muCachedId via a
// (non-virtual) InitCachedValues run after each handler is constructed (RegisterResourceTypes
// @0x82667EA8 news each handler then calls it); TypeRegistry::Register is our equivalent seam. The
// ctor still leaves safe defaults so an unregistered Type behaves like the old deferred state.

namespace CgsResource
{
    Type::Type()
        : mbCachedCanDefrag(false)
        , muCachedId(0)
    {
    }

    uint32_t Type::GetTypeID() const
    {
        return 0xFFFFFFFFu;   // every concrete handler overrides this with its registry id
    }

    ResourceDescriptor Type::GetSerialisedResourceDescriptor(const void* /*lpResource*/) const
    {
        return ResourceDescriptor();
    }

    bool Type::DeSerialise(void* /*lpResource*/) const
    {
        return true;
    }

    void Type::FixDown(void* /*lpResource*/, const rw::Resource& /*lrResource*/) const
    {
    }

    void Type::FixUp(void* /*lpResource*/, const rw::Resource& /*lrResource*/) const
    {
    }

    void Type::PostFixUp(void* /*lpResource*/, const rw::Resource& /*lrResource*/) const
    {
    }

    void Type::ReBase(void* /*lpResource*/, rw::Resource& /*lrSource*/, rw::Resource& /*lrDest*/,
                      ResourceDescriptor& /*lrSize*/, s32 /*liMemType*/) const
    {
    }

    uint32_t Type::GetImportCount(const void* /*lpResource*/) const
    {
        return 0;
    }

    void Type::GetImportPointer(const void* /*lpResource*/, uint32_t /*luIndex*/,
                                uint32_t* lpuOffset, const void** lppValue) const
    {
        if (lpuOffset != 0) *lpuOffset = 0;
        if (lppValue != 0)  *lppValue = 0;
    }

    // X360 ground truth (ARTIST .i64, 2026-08-24): every one of the 76 registered handler
    // vtables points its CanDefrag slot (index 9, vtable+0x24) at the same 0x82C296C8
    // `li r3, 1; blr` -- CanDefrag is TRUE for every resource type, no handler overrides
    // it to false. The pool therefore allocates EVERY resource from the top of its heap
    // (Pool::AllocateMemoryForResource 0x828FDAC8 reads the cached Type+4 flag), and the
    // world/car/traffic pools defragment (CreatePools forces allow-defrag for ids 3/4/15).
    // The old `return false` here silently flipped every allocation to the bottom leg and
    // starved the (future) defragmenter of candidates.
    bool Type::CanDefrag() const
    {
        return true;
    }

    bool Type::DebugValidate(const void* /*lpResource*/) const
    {
        return true;
    }

    EDebugResourceCategory Type::GetDebugResourceCategory() const
    {
        return E_DEBUGRESOURCECATEGORY_MISC;
    }

    // 0x82666178-adjacent (inlined on the X360 into RegisterResourceTypes' per-handler
    // triple): snapshot the virtuals into the cached members the hot paths read.
    void Type::InitCachedValues()
    {
        mbCachedCanDefrag = CanDefrag();
        muCachedId        = GetTypeID();
    }

    bool Type::GetCachedCanDefrag() const
    {
        return mbCachedCanDefrag;
    }

    uint32_t Type::GetCachedId() const
    {
        return muCachedId;
    }
}
