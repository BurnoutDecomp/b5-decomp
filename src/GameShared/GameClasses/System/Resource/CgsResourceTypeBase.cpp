#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource, rw::BaseResourceDescriptors<5> (= ResourceDescriptor)

// Default bodies for the CgsResource::Type polymorphic base. Concrete handlers (RwRaster, Font, ...)
// override the virtuals they implement; whatever they leave alone falls through to these no-op /
// identity defaults. The X360 caches CanDefrag()+GetTypeID() into mbCachedCanDefrag/muCachedId via a
// (non-virtual) InitCachedValues run after each handler is constructed; that caching is deferred, so
// the ctor leaves safe defaults (can't-defrag, id 0). The pool only reads GetCachedCanDefrag as a heap
// placement hint, so the defaults are benign for the load path.

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

    bool Type::CanDefrag() const
    {
        return false;
    }

    bool Type::DebugValidate(const void* /*lpResource*/) const
    {
        return true;
    }

    EDebugResourceCategory Type::GetDebugResourceCategory() const
    {
        return E_DEBUGRESOURCECATEGORY_MISC;
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
