#ifndef AI_SECTIONS_RESOURCE_TYPE_H
#define AI_SECTIONS_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
#include "BrnCommonTypes.h"   // Vector2 / Vector3 (rw::math::vpu aliases)

namespace BrnAI
{
// Forward decls (separate TUs); only pointer members reference them.
struct Portal;
struct BoundaryLine;
struct SectionResetPair;

// BrnAI::AISection -- DWARF AISectionsData.h:339 (members :462-473); 24-byte stride.
struct AISection
{
    Portal*       mpaPortals;       // +0   :462
    BoundaryLine* mpaNoGoLines;     // +4   :463
    Vector2*      mpaCorners;       // +8   :464 (DWARF FontChar::/SmoothStep:: prefix is a namespace-merge artifact; real type is the rw Vector2)
    u32           mId;              // +12  :465 (AISection::AISectionId == u32)
    s16           miSpanIndex;      // +16  :467 (Road::SpanIndex == int16_t)
    u16           muNumNoGoLines;   // +18  :468
    u8            mu8NumPortals;    // +20  :470
    u8            muSpeed;          // +21  :471
    u8            mu8eDistrict;     // +22  :472
    u8            mx8Flags;         // +23  :473
    Vector3 GetMiddle() const;      // :343 (defined in the AISection TU)

    // True when this section must not be used as a reset-on-track link target. X360
    // @0x8276AC18 (BrnAI::ResetOnTrackManager::UpdateResetOnTrackSectionUsingCurrentSection):
    // reads mx8Flags @+23 and returns true if either bit 0x01 or bit 0x40 is set, false otherwise.
    bool IsUnsuitableForResetOnTrackLink() const;
};

// BrnAI::AISectionsData -- DWARF AISectionsData.h:568 (members :637-650); 64 bytes.
struct AISectionsData
{
    AISection*        mpaSections;            // +0   :637
    SectionResetPair* mpaSectionResetPairs;   // +4   :638
    f32               mafSectionMinSpeeds[5]; // +8   :640
    f32               mafSectionMaxSpeeds[5]; // +28  :641
    u32               muNumSections;          // +48  :643
    u32               muNumSectionResetPairs; // +52  :644
    u32               muVersion;              // +56  :646
private:
    u32               muSizeInBytes;          // +60  :650 (private)
public:
    int FixUp(int liDelta);    // delta-based fix shims forwarded to from AISectionsResourceType (own TU)
    int FixDown(int liDelta);

    u32              GetSizeInBytes() const { return muSizeInBytes; }   // :590 -- replaces the raw +60 read
    const AISection* GetAISection(u32 luSectionIndex) const;           // :594 (DWARF const drift)
    Vector3          GetMiddle(u16 luSectionIndex) const;              // :613
};

class AISectionsResourceType : public CgsResource::Type
{
public:
    uint32_t                        GetTypeID() const override;
    void                            FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void                            FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
};
}

#endif
