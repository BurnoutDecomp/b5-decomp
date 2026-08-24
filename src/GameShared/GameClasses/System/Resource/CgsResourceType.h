#ifndef CGS_RESOURCE_TYPE_H
#define CGS_RESOURCE_TYPE_H

#include "types.hpp"

// Forward declarations: the resource-type virtuals take rw::Resource by reference
// and return the serialised descriptor by value, but a *declaration* only needs the
// name, not the complete type. A TU that defines or calls one of those virtuals
// includes the real rw header (rwcore_structs.h, which defines rw::Resource and the
// rw::BaseResourceDescriptors<N> template); a TU that only overrides GetTypeID does
// not, so this header stays dependency-light.
namespace rw
{
    struct Resource;
    struct IResourceAllocator;
    template <uint32_t Count> struct BaseResourceDescriptors;
}

namespace CgsResource
{
    // The X360/PS3 serialised descriptor is five (size, alignment) entries
    // (rw::BaseResourceDescriptors<5>). The PC rwcore.lib narrowed rw::ResourceDescriptor
    // to <4>; we target the X360 spine, so the descriptor type is fixed at <5> here.
    typedef rw::BaseResourceDescriptors<5> ResourceDescriptor;

    // Recovered from the DecFIGS DWARF (CgsResourceType.h).
    enum EDebugResourceCategory
    {
        E_DEBUGRESOURCECATEGORY_OVERHEAD = 0,
        E_DEBUGRESOURCECATEGORY_MATERIAL = 1,
        E_DEBUGRESOURCECATEGORY_MESH     = 2,
        E_DEBUGRESOURCECATEGORY_TEXTURE  = 3,
        E_DEBUGRESOURCECATEGORY_MISC     = 4,
        E_DEBUGRESOURCECATEGORY_DEBUG    = 5,
        E_DEBUGRESOURCECATEGORY_COUNT    = 6
    };

    // Polymorphic base of every resource-type handler. A concrete handler derives
    // from this (often via BinaryFileResourceType) and overrides the virtuals it
    // needs; GetTypeID is overridden by every handler to return its registry id.
    //
    // The 12 virtuals are declared in their X360 vtable order — DO NOT REORDER.
    // (operator delete exists on the real Type but is non-virtual and not needed
    // to compile against this base, so it is omitted.)
    class Type
    {
    public:
        Type();

        // Non-virtual, like the X360's: caches CanDefrag()/GetTypeID() into the
        // members the hot paths read (Pool::AllocateMemoryForResource reads the
        // cached flag at Type+4, the pool-module type table keys on the cached id
        // at Type+8). RegisterResourceTypes @0x82667EA8 runs it once per handler
        // right after construction; our TypeRegistry::Register is that seam.
        void InitCachedValues();

        virtual uint32_t               GetTypeID() const;
        virtual ResourceDescriptor     GetSerialisedResourceDescriptor(const void* lpResource) const;
        virtual bool                   DeSerialise(void* lpResource) const;
        virtual void                   FixDown(void* lpResource, const rw::Resource& lrResource) const;
        virtual void                   FixUp(void* lpResource, const rw::Resource& lrResource) const;
        virtual void                   PostFixUp(void* lpResource, const rw::Resource& lrResource) const;
        virtual void                   ReBase(void* lpResource, rw::Resource& lrSource, rw::Resource& lrDest, ResourceDescriptor& lrSize, s32 liMemType) const;
        virtual uint32_t               GetImportCount(const void* lpResource) const;
        virtual void                   GetImportPointer(const void* lpResource, uint32_t luIndex, uint32_t* lpuOffset, const void** lppValue) const;
        virtual bool                   CanDefrag() const;
        virtual bool                   DebugValidate(const void* lpResource) const;
        virtual EDebugResourceCategory GetDebugResourceCategory() const;

        bool     GetCachedCanDefrag() const;
        uint32_t GetCachedId() const;

        // Placement-new override that carves a Type out of a RenderWare resource allocator.
        // X360 0x82666180: builds a five-entry resource descriptor whose first entry is
        // {m_size = luSize, m_alignment = 4} (the remaining four entries are {0,1}) and
        // allocates through the allocator's resource-allocation virtual, returning the
        // main-memory base pointer of the carved resource. Modelled here via the engine's
        // rw::IResourceAllocator::DoAllocate (semantic parity, not byte-match).
        static void* operator new(size_t luSize, rw::IResourceAllocator* lpAllocator);

    protected:
        void ReBaseTechniqueFixupWithOffset(void* lpResource, rw::Resource& lrSource, rw::Resource& lrDest, ResourceDescriptor& lrSize, s32 liMemType) const;
        void ReBaseTechniqueFixDownAndCopy(void* lpResource, rw::Resource& lrSource, rw::Resource& lrDest, ResourceDescriptor& lrSize, s32 liMemType) const;

    private:
        bool     mbCachedCanDefrag;
        uint32_t muCachedId;
    };
}

#endif
