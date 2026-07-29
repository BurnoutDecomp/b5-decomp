#include "SharedClasses/AI/AISectionsResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT + Begin/Fire/End + KI_MESSAGEBUFFERSIZE
#include "GameShared/GameClasses/Development/CgsStrStream.h"  // CgsDev::StrStream (GetMiddle miss message)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnAI::AISectionsResourceType::FixDown                       @ 0x8267F4A0
//   BrnAI::AISectionsResourceType::FixUp                         @ 0x8267DB28
//   BrnAI::AISectionsResourceType::GetSerialisedResourceDescriptor @ 0x8267AE88
//   BrnAI::AISectionsResourceType::GetTypeID                     @ 0x82674D50
//
// FixUp/FixDown forward to BrnAI::AISectionsData (own TU), passing the delta (the
// rw::Resource's load base). GetSerialisedResourceDescriptor returns a five-entry
// descriptor: entry 0 = {16, count}, entries 1..4 = {count, 1}, count == AISectionsData::GetSizeInBytes() (member +60).

namespace BrnAI
{
    static const uint32_t KU_AI_SECTIONS_RESOURCE_TYPE_ID = 65537;

    uint32_t AISectionsResourceType::GetTypeID() const
    {
        return KU_AI_SECTIONS_RESOURCE_TYPE_ID;
    }

    // @0x8267F4A0 / @0x8267DB28. Both thunks hand AISectionsData the resource's segment-0
    // base -- FixDown dereferences the rw::Resource in the thunk itself, FixUp forwards the
    // reference and lets the body do the `lwz r29, 0(r4)`. Same value either way.
    //
    // ⚠️ WIDENED: these used to narrow the base through `static_cast<int>(GetLoadBase(...))`,
    // which truncates a 64-bit host allocation to its low 32 bits (bug class (a)) -- and the
    // lane data now carries 64-bit slots. GetLoadBase64 is the full-width read.
    void AISectionsResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<AISectionsData*>(lpResource)->FixDown(
            reinterpret_cast<const void*>(CgsResource::GetLoadBase64(lrResource)));
    }

    void AISectionsResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<AISectionsData*>(lpResource)->FixUp(
            reinterpret_cast<const void*>(CgsResource::GetLoadBase64(lrResource)));
    }

    // @0x8267AE88. Byte-for-byte the same shape as
    // TrafficDataResourceType::GetSerialisedResourceDescriptor @0x82760660: ONE block whose
    // size is the payload's own muSizeInBytes word (read at the console's +0x3C) with a
    // 16-byte alignment, and four unused {0, 1} blocks.
    //
    // ⚠️ FIXED: the previous transcription had size and alignment SWAPPED on entry 0 (it
    // asked for a 16-BYTE block aligned to muSizeInBytes) and filled entries 1..4 with
    // {muSizeInBytes, 1} instead of {0, 1} -- which would have made the pool allocate a
    // 16-byte block for a 3 MB resource and then four more copies of it.
    CgsResource::ResourceDescriptor AISectionsResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const u32 luSizeInBytes = static_cast<const AISectionsData*>(lpResource)->GetSizeInBytes();

        CGS_ASSERT(luSizeInBytes != 0u, "Uninitialised AI Sections resource");

        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luSizeInBytes;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 0x10u;
        for (int li = 1; li < 5; ++li)
        {
            lDescriptor.m_baseResourceDescriptors[li].m_size      = 0u;
            lDescriptor.m_baseResourceDescriptors[li].m_alignment = 1u;
        }
        return lDescriptor;
    }

    // X360 0x8230F6D0: a2 >= a1[12] (muNumSections@+48) fires assert; returns 24*a2 + *a1 ==
    // &mpaSections[luSectionIndex]. DWARF const drift applied (:594 const AISection* ... (uint32_t)
    // const). CGS_ASSERT fills __FILE__/__LINE__; the X360-baked d:\p4 path/line 1201 is dropped.
    const BrnAI::AISection* BrnAI::AISectionsData::GetAISection( u32 luSectionIndex ) const
    {
        CGS_ASSERT( luSectionIndex < muNumSections, "luSectionIndex < muNumSections" );
        return &mpaSections[luSectionIndex];
    }

    // X360 0x82764370: a1 is the Vector3 sret pointer, a2 is this, a3 (u16) is luSectionIndex.
    // Bounds-check a3 >= a2[12] (muNumSections@+48). On miss the disasm builds the diagnostic via
    // the StrStream operator<<(int) overloads and FireAssert(buf, "<path>", 683); then forwards to
    // BrnAI::AISection::GetMiddle on mpaSections[luSectionIndex] (24-byte stride == sizeof(AISection)).
    Vector3 BrnAI::AISectionsData::GetMiddle( u16 luSectionIndex ) const
    {
        if ( luSectionIndex >= muNumSections )
        {
            // X360 streams the diagnostic into the global CgsDev::Assert message buffer; matching
            // the committed BrnTriggerData.cpp / CgsID.cpp precedent, build it into a local buffer.
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream( lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
            lStream << "Can't access section " << static_cast<s32>(luSectionIndex)
                    << ", only got " << static_cast<s32>(muNumSections) << ".";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert( lacMessageBuffer,
                                        "..\\..\\..\\SharedClasses\\AI/AISectionsData.h", 683 );
            CgsDev::Assert::EndAssert();
        }

        return mpaSections[luSectionIndex].GetMiddle();
    }
}
