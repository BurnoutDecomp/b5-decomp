#include "GameShared/GameClasses/System/Resource/CgsResourceIdListResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"  // GetLoadBase64
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"        // CgsResource::ID
#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::IdListResourceType::FixDown @ 0x828F75D8
//   CgsResource::IdListResourceType::FixUp   @ 0x828F75F8
//
// The serialised resource is a CgsResource::ResourceIdList header whose leading
// member (mpaIds) is a file-relative pointer. FixDown/FixUp rebase that one pointer
// by the load-base delta, which the X360 reads from the first word of the
// rw::Resource arg (rw::Resource::m_baseResources[0]). The pointer is only adjusted
// when non-zero (a null id-array stays null).
//
// SEAM (platform-4 widened blob): the converted x64 data widens the header to
// { ID* mpaIds u64 slot @+0, u32 muNumIds @+8, pad; the u64 id array follows @+0x10 },
// matching the committed CgsResource::ResourceIdList (CgsResourceIdList.h -- the type's
// primary consumer). The X360 u32-slot pokes (*(u32*)res +/- delta) become the natural
// pointer rebase on the NAMED member, mirroring ZoneListResourceType's corrected
// pattern; the delta is the full-width GetLoadBase64 (the x64 heap base does not fit
// the console's 32-bit delta).

namespace CgsResource
{
    namespace
    {
        // File-local view of the widened serialised header. Field names mirror the
        // committed CgsResource::ResourceIdList (CgsResourceIdList.h: mpaIds @+0,
        // muNumIds @+8 after widening); modelled locally because that class keeps
        // its members private.
        struct SerialisedIdList
        {
            ID* mpaIds;   // +0x00  file-relative -> loaded id-array pointer (u64 slot)
            u32 muNumIds; // +0x08  number of ids (the u64 id array follows @+0x10)
        };
    }

    // ⭐ BODIED 2026-08-10 (world-collision wave). This class was a PARTIAL HOLLOW SHELL:
    // its header declared four virtuals and this TU defined only FixDown/FixUp, so the
    // type could never be instantiated, was therefore never registered, and the loader
    // logged "[bundle] UNREGISTERED resource type id 37 in 'worldcol.bin'" and SKIPPED
    // FixUp for all 396 zone-collision id lists. mpaIds then stayed the on-disk value
    // 0x10 and the first GetId() dereferenced address 0x10 -- a measured AV, resolved
    // through Burnout_PC.map to DoAcquireResourceListRequest+0x1A0.
    //
    // Neither body survives as an X360 export (the name index over all 30,084 JSONs has
    // exactly FixDown @0x828F75D8 and FixUp @0x828F75F8 for this class): both are the
    // tiny constant-returning kind the ARTIST link ICF-folds. The DWARF still lists them
    // (CgsResourceIdListResourceType.cpp:47 / :66), so they exist and are private virtuals.

    // The type id. ATTESTED BY THE DATA, not chosen: every one of the 396 "TRK_CLIL<n>"
    // resources in the shipped WORLDCOL.BIN carries resource-type id 37, and those are
    // exactly the id lists WorldEntityModule::PrepareZoneCollision acquires. The world
    // support transcoder names the same pairing in its own header ("IdList (type 0x25 /
    // 37)"). @ CgsResourceIdListResourceType.cpp:47 (ICF-folded in ARTIST).
    static const uint32_t KU_ID_LIST_RESOURCE_TYPE_ID = 37;   // 0x25

    uint32_t IdListResourceType::GetTypeID() const
    {
        return KU_ID_LIST_RESOURCE_TYPE_ID;
    }

    // The five-entry serialised descriptor, same shape as every sibling (payload size in
    // slot 0 at alignment 16, the other four empty).
    // ⚠️ FLAG -- the SIZE EXPRESSION is inferred, the shape is not. The console body is
    // ICF-folded, so nothing attests how it spans the blob; the size below is read off the
    // shipped data through the porter's own parser, which asserts
    // `len(blob) == 0x10 + 8*numIds + 8` for all 396 lists in WORLDCOL.BIN (a 16-byte
    // header, the u64 id array, and an 8-byte uninitialised writer tail that the LE rebuild
    // zeroes but keeps). This is the SERIALISE-OUT path only: nothing in the tree calls it
    // at runtime (the loader sizes resources from the bundle's own entry table), so it
    // cannot affect the load. Do not promote it to VERIFIED without the folded body.
    ResourceDescriptor IdListResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const SerialisedIdList* lpList = static_cast<const SerialisedIdList*>(lpResource);

        ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size =
            static_cast<u32>(sizeof(SerialisedIdList) + 8u * lpList->muNumIds + 8u);
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;
        for (int li = 1; li < 5; ++li)
        {
            lDescriptor.m_baseResourceDescriptors[li].m_size      = 0;
            lDescriptor.m_baseResourceDescriptors[li].m_alignment = 1;
        }
        return lDescriptor;
    }

    // FixDown: file-relative-ise the id-array pointer (subtract the delta), only if
    // non-zero. @ 0x828F75D8
    void IdListResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        SerialisedIdList* lpList = static_cast<SerialisedIdList*>(lpResource);
        if (lpList->mpaIds)
            lpList->mpaIds = reinterpret_cast<ID*>(
                reinterpret_cast<uintptr_t>(lpList->mpaIds) - CgsResource::GetLoadBase64(lrResource));
    }

    // FixUp: rebase the id-array pointer to the loaded address (add the delta), only
    // if non-zero. @ 0x828F75F8
    void IdListResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        SerialisedIdList* lpList = static_cast<SerialisedIdList*>(lpResource);
        if (lpList->mpaIds)
            lpList->mpaIds = reinterpret_cast<ID*>(
                reinterpret_cast<uintptr_t>(lpList->mpaIds) + CgsResource::GetLoadBase64(lrResource));
    }
}
