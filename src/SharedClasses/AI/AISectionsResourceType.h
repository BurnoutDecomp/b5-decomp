#ifndef AI_SECTIONS_RESOURCE_TYPE_H
#define AI_SECTIONS_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
#include "BrnCommonTypes.h"   // Vector2 / Vector3 (rw::math::vpu aliases)

namespace CgsMemory { class LinearMalloc; }

namespace BrnAI
{
// Forward decls (separate TUs); only pointer members reference them.
struct Portal;
struct BoundaryLine;
struct SectionResetPair;
// AISectionPointMap -- uniform-grid spatial index over the sections. DWARF home
// AISectionsData.h:540; the full (X360-offset-authoritative) layout lives in
// AISectionsData.cpp, which owns BuildAISectionPointMap/FindNearestAISection. Only
// pointer/return references appear here, so a forward decl suffices.
struct AISectionPointMap;

// Number of AI-section speed classes. AISection::muSpeed (+0x15) indexes the
// parallel mafSectionMinSpeeds[5] / mafSectionMaxSpeeds[5] bands below. The X360
// bounds it with "muSpeed < E_SECTION_SPEED_COUNT" (cmplwi ..,5) at
// AISectionsData.h:907 (e.g. RaceBalancingRoute::GetAISectionSpeed @0x827697A0).
// Additive grow flagged by the RaceBalancingRoute group; value attested by the
// asm literal and by the [5]-sized speed arrays.
const u32 E_SECTION_SPEED_COUNT = 5;

// Number of corners / edges of an AISection's convex footprint polygon. The X360
// IsInside @0x82677058 / PassesThrough @0x826772A8 walk mpaCorners with an 8-byte
// (Vector2) stride, seed the wrap-around edge from corner[3] (lfs 0x18/0x1C off the
// corner base) and terminate the loop at offset 0x20 == 4 * 8 -- i.e. exactly 4
// corners. The inlined GetCorner bounds-assert string is
// "liCornerIndex >= 0 && liCornerIndex < KI_AI_SECTION_EDGES", which names this
// constant directly. Value attested by the asm loop bound (cmpwi ..,4 / cmpwi r,0x20).
const s32 KI_AI_SECTION_EDGES = 4;

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

    // Return the luPortalIndex'th portal of this section. X360 @0x8230F5D0: bounds-checks
    // luPortalIndex against mu8NumPortals (+20) firing a streamed dev-assert on overrun,
    // then returns &mpaPortals[luPortalIndex] (Portal is a 20-byte struct).
    const Portal* GetPortal(u8 luPortalIndex) const;

    // Convex point-in-polygon test of (lfX, lfY) against the section's KI_AI_SECTION_EDGES
    // corner footprint (the world (x, z) ground plane). X360 @0x82677058.
    bool IsInside(f32 lfX, f32 lfY) const;

    // True if the 2D segment lStart->lEnd enters this section (either endpoint inside, or it
    // crosses a polygon edge with the query parameter in [0,1] and the edge parameter >= 0).
    // X360 @0x826772A8.
    bool PassesThrough(Vector2 lStart, Vector2 lEnd) const;
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

    // Spatial-lookup pair (bodied in AISectionsData.cpp). BuildAISectionPointMap @0x8267A688
    // allocates the grid index out of the supplied LinearMalloc; FindNearestAISection
    // @0x82676CC0 (the point-map overload, :623) returns the original index of the section
    // whose middle is closest to the query position.
    AISectionPointMap* BuildAISectionPointMap(CgsMemory::LinearMalloc* lpMalloc) const;  // :626
    u16                FindNearestAISection(Vector3 lPosition, AISectionPointMap* lpMap) const; // :623
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
